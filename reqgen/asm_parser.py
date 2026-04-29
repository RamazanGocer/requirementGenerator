"""Parse standalone GAS assembly (.s) files for SRS_LLR generation."""
import re
from pathlib import Path


def parse_asm(filepath):
    content = Path(filepath).read_text(encoding='utf-8', errors='replace')
    return {
        'filename': Path(filepath).name,
        'functions': _parse_functions(content),
        'data_symbols': _parse_data_symbols(content),
    }


# ---------------------------------------------------------------------------
# Function extraction
# ---------------------------------------------------------------------------

def _parse_functions(content):
    functions = []
    seen = set()

    globl_pat = re.compile(r'^\s*\.globl\s+(\w+)', re.MULTILINE)
    label_pat_tmpl = r'^{name}:\s*(?:\n|$)'

    for gm in globl_pat.finditer(content):
        name = gm.group(1)
        if name in seen:
            continue

        # Find label definition
        label_m = re.search(
            label_pat_tmpl.format(name=re.escape(name)),
            content[gm.start():],
            re.MULTILINE
        )
        if not label_m:
            continue

        label_abs = gm.start() + label_m.end()
        seen.add(name)

        # Body ends at next .globl, .data, .bss, .rodata, .section, or .end
        end_m = re.search(
            r'^\s*(?:\.globl|\.data|\.bss|\.rodata|\.section|\.end)\b',
            content[label_abs:],
            re.MULTILINE
        )
        body = content[label_abs: label_abs + end_m.start()] if end_m else content[label_abs:]

        comment = _preceding_comment(content, gm.start())
        desc = _comment_to_desc(comment, name)
        instructions = _extract_instructions(body)

        functions.append({
            'name': name,
            'description': desc,
            'instructions': instructions,
        })

    return functions


def _preceding_comment(content, pos):
    before = content[:pos].rstrip()
    if before.endswith('*/'):
        start = before.rfind('/*')
        if start >= 0:
            return before[start:]
    return ''


def _comment_to_desc(comment, func_name):
    if not comment:
        return _name_to_desc(func_name)
    # Strip delimiters and * lines
    text = re.sub(r'^/\*+', '', comment)
    text = re.sub(r'\*+/$', '', text)
    lines = []
    for line in text.split('\n'):
        line = re.sub(r'^\s*\*\s?', '', line).strip()
        line = re.sub(r'[-=]{3,}', '', line).strip()
        if line:
            lines.append(line)
    # Skip "N. func_name(...)" header lines and ABI detail lines
    _skip_pat = re.compile(
        r'^\d+\.'           # "1. func_name"
        r'|^Arguments:'     # ABI argument info
        r'|^Return:'        # ABI return info
        r'|^rdi\s*='        # register info
        r'|^rsi\s*='
    )
    for line in lines:
        if _skip_pat.match(line):
            continue
        return line
    return _name_to_desc(func_name)


def _name_to_desc(name):
    desc = re.sub(r'^asm_', '', name)
    desc = desc.replace('_', ' ').strip()
    return desc.capitalize() if desc else name


_MNEMONIC_DESC = {
    'MOV':   None,   # too generic — skip
    'RET':   None,
    'ADD':   'perform integer addition',
    'SUB':   'perform integer subtraction',
    'IMUL':  'perform signed integer multiplication',
    'IDIV':  'perform signed integer division',
    'CDQ':   'sign-extend EAX into EDX:EAX for division',
    'AND':   'perform bitwise AND',
    'OR':    'perform bitwise OR',
    'XOR':   'perform bitwise XOR',
    'NOT':   'perform bitwise NOT (one\'s complement)',
    'SHL':   'perform logical shift left',
    'SHR':   'perform logical shift right',
    'SAR':   'perform arithmetic shift right',
    'ROL':   'perform rotate left',
    'ROR':   'perform rotate right',
    'CMP':   'compare two values and set flags',
    'JG':    'branch if greater (signed)',
    'JL':    'branch if less (signed)',
    'JGE':   'branch if greater or equal',
    'JLE':   'branch if less or equal',
    'JMP':   'unconditional branch',
    'JE':    'branch if equal',
    'JNE':   'branch if not equal',
    'JGE':   'branch if greater or equal',
    'CMOVL': 'conditionally move if less (branchless select)',
    'CMOVG': 'conditionally move if greater (branchless select)',
    'INC':   'increment by 1',
    'DEC':   'decrement by 1',
    'PUSH':  'push value onto the stack',
    'POP':   'pop value from the stack',
    'CALL':  'call a subroutine',
    'REP':   'repeat a string instruction',
    'STOSB': 'store byte string (fill memory)',
    'BSF':   'perform bit scan forward to find lowest set bit',
    'XCHG':  'atomically exchange two values',
    'NOP':   'execute no-operation',
}


def _extract_instructions(body):
    """Return unique, meaningful instruction entries from body."""
    seen = set()
    results = []
    for line in body.split('\n'):
        line = line.strip()
        if not line or line.startswith('.') or line.startswith('#') or line.endswith(':'):
            continue
        m = re.match(r'(\w+)', line)
        if not m:
            continue
        instr = m.group(1).upper()
        if instr in seen:
            continue
        seen.add(instr)
        desc = _MNEMONIC_DESC.get(instr)
        if desc is None:
            continue  # skip MOV/RET noise
        results.append({'instruction': instr, 'description': desc})
    return results


# ---------------------------------------------------------------------------
# Data section symbols
# ---------------------------------------------------------------------------

def _parse_data_symbols(content):
    symbols = []
    # .data / .bss / .rodata section globals
    pat = re.compile(
        r'^\s*\.globl\s+(\w+)\s*\n'
        r'(?:.*\n)*?'
        r'\s*\1:\s*\n'
        r'\s*\.(long|zero|string|byte|short|quad)\s+(.*)',
        re.MULTILINE
    )
    for m in pat.finditer(content):
        name = m.group(1)
        directive = m.group(2)
        value = m.group(3).strip()
        dtype_map = {
            'long': 'uint32_t', 'zero': 'uint8_t[]', 'string': 'const char*',
            'byte': 'uint8_t', 'short': 'uint16_t', 'quad': 'uint64_t',
        }
        size_map = {
            'long': 4, 'byte': 1, 'short': 2, 'quad': 8,
        }
        # For .zero N, size = N; for .string, size = len(value)+1
        if directive == 'zero':
            try:
                sz = int(value)
            except ValueError:
                sz = '?'
        elif directive == 'string':
            sz = len(value) - 2 + 1 if value.startswith('"') else '?'
        else:
            sz = size_map.get(directive, '?')

        symbols.append({
            'name': name,
            'data_type': dtype_map.get(directive, directive),
            'size': sz,
            'value': value,
            'section': directive,
        })
    return symbols
