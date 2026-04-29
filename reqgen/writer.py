"""Write parsed C data to pipe-delimited TXT files for DOORS DXL import."""
import re
from datetime import date
from pathlib import Path


# Pipe character must not appear inside field values — sanitize to avoid
# breaking DXL parsing.
def _s(value):
    """Convert value to string and escape pipe characters."""
    return str(value).replace('|', '/').replace('\n', ' ').strip()


# ---------------------------------------------------------------------------
# DD_HEADER writer
# ---------------------------------------------------------------------------

def write_dd_header(parsed, output_path):
    """
    Write a DD_HEADER.txt file.

    Record format (pipe-delimited):
      SECTION   | Title
      DEFINE    | Name | Value | DataType | SizeBytes | ObjectText | Description
      DEFINE_FUNC| Name | Value | DataType | SizeBytes | ObjectText | Description
      TYPEDEF   | Name | AliasOf | DataType | SizeBytes | ObjectText | Description
      ENUM      | Name | DataType | SizeBytes | ObjectText | Description
      ENUM_VAL  | EnumName | ValName | Value
      STRUCT    | Name | DataType | SizeBytes | ObjectText | Description
      STRUCT_FIELD| StructName | FieldName | DataType | SizeBytes | BitWidth
      UNION     | Name | DataType | SizeBytes | ObjectText | Description
      UNION_FIELD| UnionName | FieldName | DataType | SizeBytes | BitWidth
      FUNCPTR   | Name | ReturnType | Params | SizeBytes | ObjectText | Description
      INLINE    | Name | ReturnType | Params | SizeBytes | ObjectText | Description
    """
    lines = _dd_lines(parsed)
    Path(output_path).write_text('\n'.join(lines) + '\n', encoding='utf-8')


def _dd_lines(parsed):
    today = date.today().isoformat()
    lines = [
        f'# DD_HEADER | {_s(parsed["filename"])} | Generated: {today}',
        f'# Source file: {_s(parsed["filename"])}',
        '#',
        '# COLUMNS per record type:',
        '#   SECTION    | Title',
        '#   DEFINE     | Name|Value|DataType|Size(B)|ObjectText|Description',
        '#   DEFINE_FUNC| Name|Value|DataType|Size(B)|ObjectText|Description',
        '#   TYPEDEF    | Name|AliasOf|DataType|Size(B)|ObjectText|Description',
        '#   ENUM       | Name|DataType|Size(B)|ObjectText|Description',
        '#   ENUM_VAL   | EnumName|ValName|Value',
        '#   STRUCT     | Name|DataType|Size(B)|ObjectText|Description',
        '#   STRUCT_FIELD| StructName|FieldName|DataType|Size(B)|BitWidth',
        '#   UNION      | Name|DataType|Size(B)|ObjectText|Description',
        '#   UNION_FIELD| UnionName|FieldName|DataType|Size(B)|BitWidth',
        '#   FUNCPTR    | Name|ReturnType|Params|Size(B)|ObjectText|Description',
        '#   INLINE     | Name|ReturnType|Params|Size(B)|ObjectText|Description',
        '#',
    ]

    if parsed.get('defines'):
        lines.append('SECTION|Preprocessor Macros')
        for d in parsed['defines']:
            rec_type = 'DEFINE_FUNC' if d['is_funclike'] else 'DEFINE'
            lines.append('|'.join([
                rec_type,
                _s(d['name']),
                _s(d['value']),
                _s(d['data_type']),
                _s(d['size']),
                _s(d['object_text']),
                _s(d['description']),
            ]))

    if parsed.get('typedefs'):
        lines.append('SECTION|Type Definitions')
        for t in parsed['typedefs']:
            lines.append('|'.join([
                'TYPEDEF',
                _s(t['name']),
                _s(t['alias_of']),
                _s(t['data_type']),
                _s(t['size']),
                _s(t['object_text']),
                _s(t['description']),
            ]))

    if parsed.get('enums'):
        lines.append('SECTION|Enumerations')
        for e in parsed['enums']:
            lines.append('|'.join([
                'ENUM',
                _s(e['name']),
                _s(e['data_type']),
                _s(e['size']),
                _s(e['object_text']),
                _s(e['description']),
            ]))
            for v in e['values']:
                lines.append('|'.join([
                    'ENUM_VAL',
                    _s(e['name']),
                    _s(v['name']),
                    _s(v['value']),
                ]))

    if parsed.get('structs'):
        lines.append('SECTION|Structures')
        for s in parsed['structs']:
            lines.append('|'.join([
                'STRUCT',
                _s(s['name']),
                _s(s['data_type']),
                _s(s['size']),
                _s(s['object_text']),
                _s(s['description']),
            ]))
            for f in s['fields']:
                lines.append('|'.join([
                    'STRUCT_FIELD',
                    _s(s['name']),
                    _s(f['name']),
                    _s(f['data_type']),
                    _s(f['size']),
                    _s(f['bit_width']),
                ]))

    if parsed.get('unions'):
        lines.append('SECTION|Unions')
        for u in parsed['unions']:
            lines.append('|'.join([
                'UNION',
                _s(u['name']),
                _s(u['data_type']),
                _s(u['size']),
                _s(u['object_text']),
                _s(u['description']),
            ]))
            for f in u['fields']:
                lines.append('|'.join([
                    'UNION_FIELD',
                    _s(u['name']),
                    _s(f['name']),
                    _s(f['data_type']),
                    _s(f['size']),
                    _s(f['bit_width']),
                ]))

    if parsed.get('funcptrs'):
        lines.append('SECTION|Function Pointer Types')
        for fp in parsed['funcptrs']:
            lines.append('|'.join([
                'FUNCPTR',
                _s(fp['name']),
                _s(fp['return_type']),
                _s(fp['params']),
                _s(fp['size']),
                _s(fp['object_text']),
                _s(fp['description']),
            ]))

    if parsed.get('inlines'):
        lines.append('SECTION|Inline Functions')
        for il in parsed['inlines']:
            lines.append('|'.join([
                'INLINE',
                _s(il['name']),
                _s(il['return_type']),
                _s(il['params']),
                _s(il['size']),
                _s(il['object_text']),
                _s(il['description']),
            ]))

    return lines


# ---------------------------------------------------------------------------
# SRS_LLR writer
# ---------------------------------------------------------------------------

def write_srs_llr(parsed, output_path):
    """
    Write an SRS_LLR.txt file.

    Record format (pipe-delimited):
      FUNC_HEADING | FunctionName | HeadingText           | isReq=false
      FUNC_REQ     | FunctionName | RequirementText        | isReq=true
      FUNC_PARAM   | FunctionName | ParameterRequirement   | isReq=true
      FUNC_RETURN  | FunctionName | ReturnRequirement      | isReq=true
      ASM_HEADING  | FunctionName | HeadingText            | isReq=false
      ASM_REQ      | FunctionName | AssemblyRequirement    | isReq=true
    """
    lines = _srs_lines(parsed)
    Path(output_path).write_text('\n'.join(lines) + '\n', encoding='utf-8')


def _srs_lines(parsed):
    today = date.today().isoformat()
    lines = [
        f'# SRS_LLR | {_s(parsed["filename"])} | Generated: {today}',
        f'# Source file: {_s(parsed["filename"])}',
        '#',
        '# COLUMNS: RecordType | FunctionName | Text | isReq',
        '#   FUNC_HEADING | name | heading text        | false',
        '#   FUNC_REQ     | name | requirement text    | true',
        '#   FUNC_PARAM   | name | parameter req text  | true',
        '#   FUNC_RETURN  | name | return req text     | true',
        '#   ASM_HEADING  | name | heading text        | false',
        '#   ASM_REQ      | name | asm req text        | true',
        '#   DATA_HEADING | name | heading text        | false',
        '#   DATA_REQ     | name | data req text       | true',
        '#',
    ]

    for func in parsed.get('functions', []):
        lines.extend(_func_records(func))

    # Assembly data symbols (from asm_parser)
    for sym in parsed.get('data_symbols', []):
        lines.append('|'.join([
            'DATA_HEADING',
            _s(sym['name']),
            f'Assembly global variable: {_s(sym["name"])}',
            'false',
        ]))
        lines.append('|'.join([
            'DATA_REQ',
            _s(sym['name']),
            (f'The {_s(sym["name"])} variable shall be a {_s(sym["data_type"])} '
             f'of size {_s(sym["size"])} byte(s), initialized to {_s(sym["value"])}.'),
            'true',
        ]))

    return lines


def _func_records(func):
    name = func['name']
    desc = func.get('description') or _name_to_desc(name)
    lines = []

    if func.get('has_asm'):
        heading_type = 'ASM_HEADING'
        req_type = 'ASM_REQ'
    else:
        heading_type = 'FUNC_HEADING'
        req_type = 'FUNC_REQ'

    lines.append('|'.join([heading_type, _s(name), _s(desc), 'false']))

    # Main functional requirement from brief
    shall_clause = _to_shall(desc)
    lines.append('|'.join([
        req_type, _s(name),
        f'The {_s(name)}() function shall {shall_clause}.',
        'true',
    ]))

    # Parameter requirements from doxygen @param
    for pname, pdesc in func.get('params_doc', []):
        lines.append('|'.join([
            'FUNC_PARAM', _s(name),
            f'The {_s(name)}() function shall accept parameter {_s(pname)}: {_s(pdesc)}.',
            'true',
        ]))

    # Return value requirement
    ret_doc = func.get('returns_doc', '')
    ret_type = func.get('return_type', '')
    if ret_doc:
        lines.append('|'.join([
            'FUNC_RETURN', _s(name),
            f'The {_s(name)}() function shall return {_s(ret_doc)}.',
            'true',
        ]))
    elif ret_type and ret_type.strip() not in ('void', ''):
        lines.append('|'.join([
            'FUNC_RETURN', _s(name),
            f'The {_s(name)}() function shall return a value of type {_s(ret_type)}.',
            'true',
        ]))

    # Per-instruction requirements for inline asm
    for asm in func.get('asm_blocks', []):
        lines.append('|'.join([
            'ASM_REQ', _s(name),
            f'The {_s(name)}() function shall {_s(asm["description"])}.',
            'true',
        ]))

    return lines


def _to_shall(brief):
    """Turn a brief description into a 'shall ...' clause."""
    s = brief.strip().rstrip('.')
    if not s:
        return 'perform its specified operation'
    return s[0].lower() + s[1:]


def _name_to_desc(name):
    words = re.sub(r'([a-z])([A-Z])', r'\1 \2', name)
    words = words.replace('_', ' ')
    return words.strip().capitalize()


# ---------------------------------------------------------------------------
# ASM SRS_LLR writer (for standalone .s files)
# ---------------------------------------------------------------------------

def write_srs_llr_asm(parsed, output_path):
    """Write SRS_LLR for a standalone assembly file."""
    asm_parsed = {
        'filename': parsed['filename'],
        'functions': _adapt_asm_functions(parsed['functions']),
        'data_symbols': parsed.get('data_symbols', []),
    }
    write_srs_llr(asm_parsed, output_path)


def _adapt_asm_functions(asm_funcs):
    """Convert asm_parser function dicts to the shape write_srs_llr expects."""
    adapted = []
    for f in asm_funcs:
        adapted.append({
            'name': f['name'],
            'return_type': 'int',
            'params': '',
            'description': f.get('description', ''),
            'params_doc': [],
            'returns_doc': '',
            'has_asm': True,
            'asm_blocks': [
                {'instruction': i['instruction'], 'description': i['description']}
                for i in f.get('instructions', [])
            ],
        })
    return adapted
