"""Parse doxygen-style and plain block comments."""
import re


def extract_preceding_comment(content, pos):
    """Return the comment block immediately before position pos."""
    before = content[:pos].rstrip()

    # Block comment: find the LAST */ before pos, then walk back to its /*
    if before.endswith('*/'):
        start = before.rfind('/*')
        if start >= 0:
            return before[start:]

    # Consecutive // line comments immediately before pos
    lines = before.split('\n')
    comment_lines = []
    for line in reversed(lines):
        stripped = line.strip()
        if stripped.startswith('//'):
            comment_lines.insert(0, stripped)
        elif stripped == '':
            if comment_lines:
                break
        else:
            break
    return '\n'.join(comment_lines)


def parse_comment(raw):
    """
    Parse raw comment text (doxygen or plain) into structured fields.
    Returns: {brief, params [(name, desc)], returns, note}
    """
    result = {'brief': '', 'params': [], 'returns': '', 'note': ''}
    if not raw:
        return result

    # Strip /* */ delimiters and leading * on each line
    text = raw
    text = re.sub(r'^/\*+!?', '', text)
    text = re.sub(r'\*+/$', '', text)
    lines = []
    for line in text.split('\n'):
        line = re.sub(r'^\s*\*\s?', '', line)
        # Strip ===...=== separator lines
        cleaned = re.sub(r'=+', '', line).strip()
        if cleaned:
            lines.append(cleaned)
    text = '\n'.join(lines)

    if any(tag in text for tag in ('@brief', '@param', '@return', '@note')):
        # Doxygen format
        m = re.search(r'@brief\s+(.+?)(?=@\w|\Z)', text, re.DOTALL)
        if m:
            result['brief'] = ' '.join(m.group(1).split())

        for m in re.finditer(r'@param\s+(\w+)\s+(.+?)(?=@\w|\Z)', text, re.DOTALL):
            result['params'].append((m.group(1), ' '.join(m.group(2).split())))

        m = re.search(r'@return\s+(.+?)(?=@\w|\Z)', text, re.DOTALL)
        if m:
            result['returns'] = ' '.join(m.group(1).split())

        m = re.search(r'@note\s+(.+?)(?=@\w|\Z)', text, re.DOTALL)
        if m:
            result['note'] = ' '.join(m.group(1).split())
    else:
        # Plain comment — join all lines as brief
        brief = ' '.join(text.split())
        result['brief'] = brief

    return result
