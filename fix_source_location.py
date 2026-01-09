#!/usr/bin/env python3
"""
Fix Source_Location fields: l→line, c→column, f→filename
Also fix KW array access.
"""

import re

with open('ada83.c', 'r') as f:
    code = f.read()

# Source_Location fields - fix l, c, f for variables named lc, l, loc etc
code = re.sub(r'\blc\.l\b', 'lc.line', code)
code = re.sub(r'\blc\.c\b', 'lc.column', code)
code = re.sub(r'\blc\.f\b', 'lc.filename', code)

code = re.sub(r'\bl\.l\b', 'l.line', code)
code = re.sub(r'\bl\.c\b', 'l.column', code)
code = re.sub(r'\bl\.f\b', 'l.filename', code)

code = re.sub(r'\bloc\.l\b', 'loc.line', code)
code = re.sub(r'\bloc\.c\b', 'loc.column', code)
code = re.sub(r'\bloc\.f\b', 'loc.filename', code)

# Fix KW struct accesses - the .k should be .keyword and .t should be .token_kind
code = re.sub(r'KW\[i\]\.keyword\.s', 'KW[i].keyword.string', code)
code = re.sub(r'KW\[i\]\.t\b', 'KW[i].token_kind', code)

with open('ada83.c', 'w') as f:
    f.write(code)

print("Fixed Source_Location and KW fields")
