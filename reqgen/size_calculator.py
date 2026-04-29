"""C type size calculation (x86-64 / LP64 / C11)."""
import re

_SIZE_MAP = {
    # 1-byte
    'char': 1, 'unsigned char': 1, 'signed char': 1,
    'uint8_t': 1, 'int8_t': 1, 'u8': 1, 's8': 1, '_Bool': 1, 'bool': 1,
    # 2-byte
    'short': 2, 'unsigned short': 2, 'short int': 2,
    'uint16_t': 2, 'int16_t': 2, 'u16': 2,
    # 4-byte
    'int': 4, 'unsigned int': 4, 'signed int': 4,
    'uint32_t': 4, 'int32_t': 4, 'uint': 4, 's32': 4,
    'float': 4, 'f32': 4,
    # 8-byte (LP64)
    'long': 8, 'unsigned long': 8,
    'long long': 8, 'unsigned long long': 8,
    'uint64_t': 8, 'int64_t': 8,
    'double': 8, 'f64': 8,
    'size_t': 8, 'ptrdiff_t': 8, 'intptr_t': 8, 'uintptr_t': 8,
    # 16-byte
    'long double': 16,
}


def get_type_size(type_str):
    """Return byte size for a C type string, or '?' if unknown."""
    s = re.sub(r'\b(const|volatile|restrict|static|extern|inline)\b', '', type_str).strip()

    if '*' in s:
        return 8   # pointer, x64

    # Array: base[N]
    m = re.match(r'(.+?)\[(\d+)\]\s*$', s)
    if m:
        base = get_type_size(m.group(1))
        return base * int(m.group(2)) if isinstance(base, int) else '?'

    if re.match(r'\benum\b', s):
        return 4

    return _SIZE_MAP.get(s, '?')


def struct_size(fields):
    """Naive struct size = sum of field sizes (no padding)."""
    total = 0
    for f in fields:
        sz = f.get('size', '?')
        if sz == '?':
            return '?'
        total += sz
    return total


def union_size(fields):
    """Union size = max of field sizes."""
    max_sz = 0
    for f in fields:
        sz = f.get('size', '?')
        if sz == '?':
            return '?'
        if sz > max_sz:
            max_sz = sz
    return max_sz
