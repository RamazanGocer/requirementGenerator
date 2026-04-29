"""Parse C header (.h) files into structured dicts for DD_HEADER generation."""
import re
from pathlib import Path

from .doxygen_parser import extract_preceding_comment, parse_comment
from .size_calculator import get_type_size, struct_size, union_size


# ---------------------------------------------------------------------------
# Public API
# ---------------------------------------------------------------------------

def parse_header(filepath):
    content = Path(filepath).read_text(encoding='utf-8', errors='replace')
    clean = _clean_source(content)   # strings + comments stripped

    return {
        'filename': Path(filepath).name,
        'defines':   _parse_defines(content, clean),
        'typedefs':  _parse_typedefs(content, clean),
        'enums':     _parse_enums(content, clean),
        'structs':   _parse_structs(content, clean),
        'unions':    _parse_unions(content, clean),
        'funcptrs':  _parse_funcptrs(content, clean),
        'inlines':   _parse_inlines(content, clean),
    }


# ---------------------------------------------------------------------------
# Source pre-processing
# ---------------------------------------------------------------------------

def _clean_source(content):
    """
    Return a version of content with string/char literals and comments
    replaced by whitespace — preserving line numbers and character offsets.
    """
    result = []
    i, n = 0, len(content)
    while i < n:
        if content[i:i+2] == '/*':
            end = content.find('*/', i + 2)
            end = n if end == -1 else end + 2
            block = content[i:end]
            result.append(re.sub(r'[^\n]', ' ', block))
            i = end
        elif content[i:i+2] == '//':
            end = content.find('\n', i)
            end = n if end == -1 else end
            result.append(' ' * (end - i))
            i = end
        elif content[i] == '"':
            j = i + 1
            while j < n:
                if content[j] == '\\':
                    j += 2
                elif content[j] == '"':
                    j += 1; break
                else:
                    j += 1
            result.append(' ' * (j - i))
            i = j
        elif content[i] == "'":
            j = i + 1
            while j < n:
                if content[j] == '\\':
                    j += 2
                elif content[j] == "'":
                    j += 1; break
                else:
                    j += 1
            result.append(' ' * (j - i))
            i = j
        else:
            result.append(content[i])
            i += 1
    return ''.join(result)


def _get_comment(content, pos):
    return parse_comment(extract_preceding_comment(content, pos))


# ---------------------------------------------------------------------------
# #define macros
# ---------------------------------------------------------------------------

_INCLUDE_GUARD_RE = re.compile(r'^[A-Z0-9_]+_H(?:PP)?$')
_SKIP_DEFINES = {'HAS_C11', 'C_VERSION'}


def _parse_defines(content, clean):
    defines = {}   # name → dict  (dedup: keep last)

    # Handle line continuations before matching
    joined = re.sub(r'\\\n', ' ', clean)
    joined_orig = re.sub(r'\\\n', ' ', content)

    pat = re.compile(
        r'^[ \t]*#[ \t]*define[ \t]+(\w+)(\([^)]*\))?[ \t]*(.*?)$',
        re.MULTILINE
    )
    for m in pat.finditer(joined):
        name = m.group(1)
        func_args = m.group(2)
        value = m.group(3).strip()

        if name in _SKIP_DEFINES:
            continue
        if _INCLUDE_GUARD_RE.match(name) and not value:
            continue
        if name.startswith('_'):
            continue

        # Get comment from original (non-joined) content near same offset
        doc = _get_comment(content, m.start())

        is_func = func_args is not None
        dtype, size = _infer_define_type(value, is_func)

        defines[name] = {
            'name': name + (func_args or ''),
            'identifier': name,
            'value': value,
            'is_funclike': is_func,
            'data_type': dtype,
            'size': size,
            'object_text': (
                f'Function-like macro {name}{func_args} expands to: {value}'
                if is_func
                else f'Macro {name} is defined as {value}'
            ),
            'description': doc['brief'],
        }

    return list(defines.values())


def _infer_define_type(value, is_func):
    if is_func:
        return 'macro (function-like)', 0
    if re.fullmatch(r'-?\d+[uUlL]*', value):
        return 'int', 4
    if re.fullmatch(r'-?\d+\.\d+[\w]*', value):
        return 'double', 8
    if re.fullmatch(r'0[xX][0-9A-Fa-f]+[uUlL]*', value):
        return 'uint32_t', 4
    return 'macro', 0


# ---------------------------------------------------------------------------
# typedef aliases
# ---------------------------------------------------------------------------

def _parse_typedefs(content, clean):
    results = []
    pat = re.compile(
        r'\btypedef\b\s+'
        r'(?!struct\b|enum\b|union\b|void\s*\(\s*\*)'
        r'((?:unsigned\s+|signed\s+|long\s+)*\w+(?:\s+\w+)*?)\s+'
        r'(\w+)\s*;',
        re.MULTILINE
    )
    seen = set()
    for m in pat.finditer(clean):
        alias_type = m.group(1).strip()
        name = m.group(2).strip()
        if name in seen or alias_type in ('struct', 'enum', 'union'):
            continue
        seen.add(name)
        doc = _get_comment(content, m.start())
        size = get_type_size(alias_type)
        results.append({
            'name': name,
            'alias_of': alias_type,
            'data_type': alias_type,
            'size': size,
            'object_text': f'{name} is a typedef alias for {alias_type}',
            'description': doc['brief'],
        })
    return results


# ---------------------------------------------------------------------------
# Enums
# ---------------------------------------------------------------------------

def _parse_enums(content, clean):
    results = []
    pat = re.compile(
        r'\btypedef\s+enum\s*\w*\s*\{([^}]*)\}\s*(\w+)\s*;',
        re.DOTALL
    )
    for m in pat.finditer(clean):
        name = m.group(2)
        # Parse values from original content (keeps comments for better descriptions)
        orig_body = content[m.start(1):m.end(1)]
        doc = _get_comment(content, m.start())
        values = _parse_enum_values(m.group(1))
        results.append({
            'name': name,
            'data_type': 'enum (int)',
            'size': 4,
            'values': values,
            'object_text': f'{name} is an enumeration type',
            'description': doc['brief'],
        })
    return results


def _parse_enum_values(body):
    body = re.sub(r'/\*.*?\*/', '', body, flags=re.DOTALL)
    body = re.sub(r'//.*', '', body)
    vals = []
    counter = 0
    for m in re.finditer(r'(\w+)\s*(?:=\s*([^,}\n]+))?', body):
        vname = m.group(1)
        if m.group(2):
            raw = m.group(2).strip()
            try:
                counter = int(raw, 0)
            except ValueError:
                counter = raw  # expression
        vals.append({'name': vname, 'value': str(counter)})
        if isinstance(counter, int):
            counter += 1
    return vals


# ---------------------------------------------------------------------------
# Structs
# ---------------------------------------------------------------------------

def _parse_structs(content, clean):
    results = []
    # Match typedef struct { ... } Name; — handle nested anonymous structs/unions
    pat = re.compile(
        r'\btypedef\s+struct\s*\w*\s*(\{(?:[^{}]|\{[^{}]*\})*\})\s*(\w+)\s*;',
        re.DOTALL
    )
    for m in pat.finditer(clean):
        name = m.group(2)
        body = m.group(1)[1:-1]   # strip outer braces
        doc = _get_comment(content, m.start())
        fields = _parse_fields(body)
        size = struct_size(fields)
        results.append({
            'name': name,
            'data_type': f'struct {name}',
            'size': size,
            'fields': fields,
            'object_text': f'{name} is a structure type',
            'description': doc['brief'],
        })
    return results


# ---------------------------------------------------------------------------
# Unions
# ---------------------------------------------------------------------------

def _parse_unions(content, clean):
    results = []
    pat = re.compile(
        r'\btypedef\s+union\s*\w*\s*(\{(?:[^{}]|\{[^{}]*\})*\})\s*(\w+)\s*;',
        re.DOTALL
    )
    for m in pat.finditer(clean):
        name = m.group(2)
        body = m.group(1)[1:-1]
        doc = _get_comment(content, m.start())
        fields = _parse_fields(body)
        size = union_size(fields)
        results.append({
            'name': name,
            'data_type': f'union {name}',
            'size': size,
            'fields': fields,
            'object_text': f'{name} is a union type',
            'description': doc['brief'],
        })
    return results


def _parse_fields(body):
    """Parse struct/union body into field list. Handles nested anon structs."""
    # Remove nested struct/union bodies (keep field names only at top level)
    # First strip nested braces content
    flat = re.sub(r'\{[^{}]*\}', '{ }', body)
    flat = re.sub(r'/\*.*?\*/', '', flat, flags=re.DOTALL)
    flat = re.sub(r'//.*', '', flat)

    fields = []
    # Groups: (base_type) (field_name) (array[N])? (:bits)?
    pat = re.compile(
        r'\b((?:(?:unsigned|signed|long|short|struct|enum|union)\s+)*\w+(?:\s*\*)?)\s+'
        r'(\w+)\s*'
        r'(\[\d+\])?\s*'       # optional array dimension
        r'(?::\s*(\d+))?\s*;'  # optional bit-field width
    )
    for m in pat.finditer(flat):
        base_type = m.group(1).strip()
        fname = m.group(2).strip()
        array_sfx = m.group(3) or ''
        bits = m.group(4)
        dtype = base_type + array_sfx   # e.g. "char[32]"
        size = get_type_size(dtype)
        fields.append({
            'name': fname,
            'data_type': dtype,
            'size': size,
            'bit_width': bits or '0',
        })
    return fields


# ---------------------------------------------------------------------------
# Function pointer typedefs
# ---------------------------------------------------------------------------

def _parse_funcptrs(content, clean):
    results = []
    pat = re.compile(
        r'\btypedef\s+([\w\s\*]+?)\s*\(\s*\*\s*(\w+)\s*\)\s*\(([^)]*)\)\s*;'
    )
    for m in pat.finditer(clean):
        ret = m.group(1).strip()
        name = m.group(2).strip()
        params = m.group(3).strip()
        doc = _get_comment(content, m.start())
        results.append({
            'name': name,
            'return_type': ret,
            'params': params,
            'data_type': 'function pointer',
            'size': 8,
            'object_text': f'{name} is a function pointer: {ret} (*{name})({params})',
            'description': doc['brief'],
        })
    return results


# ---------------------------------------------------------------------------
# Static inline functions
# ---------------------------------------------------------------------------

def _parse_inlines(content, clean):
    results = []
    pat = re.compile(
        r'\bstatic\s+inline\s+([\w\s\*]+?)\s+(\w+)\s*\(([^)]*)\)\s*\{'
    )
    for m in pat.finditer(clean):
        ret = m.group(1).strip()
        name = m.group(2).strip()
        params = m.group(3).strip()
        doc = _get_comment(content, m.start())
        results.append({
            'name': name,
            'return_type': ret,
            'params': params,
            'data_type': ret,
            'size': get_type_size(ret) if ret != 'void' else 0,
            'object_text': f'static inline {ret} {name}({params})',
            'description': doc['brief'],
            'params_doc': doc['params'],
            'returns_doc': doc['returns'],
        })
    return results
