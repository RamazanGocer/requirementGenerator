"""Parse C source (.c) files into structured dicts for SRS_LLR generation."""
import re
from pathlib import Path

from .doxygen_parser import extract_preceding_comment, parse_comment


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def parse_source(filepath):
    content = Path(filepath).read_text(encoding='utf-8', errors='replace')
    clean = _clean_source(content)
    return {
        'filename': Path(filepath).name,
        'functions': _parse_functions(content, clean),
    }


# ---------------------------------------------------------------------------
# Source pre-processing (same as header_parser but duplicated to avoid
# circular import — could be factored out if the project grows)
# ---------------------------------------------------------------------------

def _clean_source(content):
    result = []
    i, n = 0, len(content)
    while i < n:
        if content[i:i+2] == '/*':
            end = content.find('*/', i + 2)
            end = n if end == -1 else end + 2
            result.append(re.sub(r'[^\n]', ' ', content[i:end]))
            i = end
        elif content[i:i+2] == '//':
            end = content.find('\n', i)
            end = n if end == -1 else end
            result.append(' ' * (end - i))
            i = end
        elif content[i] == '"':
            j = i + 1
            while j < n:
                if content[j] == '\\': j += 2
                elif content[j] == '"': j += 1; break
                else: j += 1
            result.append(' ' * (j - i))
            i = j
        elif content[i] == "'":
            j = i + 1
            while j < n:
                if content[j] == '\\': j += 2
                elif content[j] == "'": j += 1; break
                else: j += 1
            result.append(' ' * (j - i))
            i = j
        else:
            result.append(content[i])
            i += 1
    return ''.join(result)


# ---------------------------------------------------------------------------
# Function parsing
# ---------------------------------------------------------------------------

_CONTROL_KEYWORDS = frozenset({
    'if', 'else', 'while', 'for', 'do', 'switch', 'return',
    'case', 'default', 'break', 'continue', 'goto',
})


def _parse_functions(content, clean):
    functions = []
    seen = set()

    # Match: return_type [*] function_name(params) {
    # Group 1 = base return type, Group 2 = optional *, Group 3 = name, Group 4 = params
    pat = re.compile(
        r'^([^(\n]+?)\s+'       # return type (everything before last word before '(')
        r'(\*+\s*)?'            # optional pointer stars
        r'(\w+)\s*'             # function name
        r'\(([^)]*)\)\s*'       # params
        r'\{',                   # opening brace
        re.MULTILINE
    )

    for m in pat.finditer(clean):
        name = m.group(3).strip()
        if name in _CONTROL_KEYWORDS or name in seen:
            continue

        stars = (m.group(2) or '').strip()
        ret_type = (m.group(1).strip() + (' ' + stars if stars else '')).strip()
        # Skip preprocessor / typedef / struct keywords leaking into match
        if any(kw in ret_type for kw in ('#', 'typedef')):
            continue

        params = m.group(4).strip()
        seen.add(name)

        brace_pos = m.end() - 1
        body_clean = _extract_body(clean, brace_pos)
        body_orig  = _extract_body(content, brace_pos)

        doc = parse_comment(extract_preceding_comment(content, m.start()))
        asm_blocks = _find_inline_asm(body_orig)

        functions.append({
            'name': name,
            'return_type': ret_type,
            'params': params,
            'description': doc['brief'],
            'params_doc': doc['params'],
            'returns_doc': doc['returns'],
            'has_asm': bool(asm_blocks),
            'asm_blocks': asm_blocks,
        })

    return functions


def _extract_body(content, open_brace):
    depth = 0
    for i in range(open_brace, len(content)):
        if content[i] == '{':
            depth += 1
        elif content[i] == '}':
            depth -= 1
            if depth == 0:
                return content[open_brace + 1:i]
    return ''


# ---------------------------------------------------------------------------
# Inline assembly detection
# ---------------------------------------------------------------------------

_ASM_PAT = re.compile(
    r'__asm__\s*(?:__volatile__)?\s*\(\s*"([^"]*)"',
    re.DOTALL
)

# Map instruction → human-readable description
_INSTR_DESC = {
    'NOP':    'execute a no-operation (NOP) instruction',
    'ADDL':   'perform 32-bit integer addition',
    'PUSHFQ': 'read the RFLAGS register',
    'POPQ':   'read the RFLAGS register',
    'CPUID':  'retrieve CPU identification information via the CPUID instruction',
    'MFENCE': 'execute a memory fence (MFENCE) to ensure memory ordering',
    'BSFL':   'perform a Bit Scan Forward (BSF) to find the lowest set bit position',
    'XCHGL':  'perform an atomic exchange (XCHG) of two integer values',
    'ROLL':   'perform a rotate-left (ROL) bit operation',
    'REP':    'execute a REP-prefixed string instruction',
}


def _find_inline_asm(body):
    blocks = []
    for m in _ASM_PAT.finditer(body):
        first_line = m.group(1).split('\\n')[0].strip()
        instr = first_line.split()[0].upper() if first_line.split() else 'ASM'
        desc = _INSTR_DESC.get(instr, f'execute the {instr} instruction')
        blocks.append({'instruction': instr, 'description': desc})
    return blocks
