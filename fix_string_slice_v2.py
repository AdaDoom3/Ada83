#!/usr/bin/env python3
"""
Fix String_Slice fields: .s → .string, .n → .length
Only for variables that are known to be String_Slice type.
"""

import re

with open('ada83.c', 'r') as f:
    code = f.read()

# Common String_Slice variable names found in function signatures
# Let me fix the most common patterns:

# Parameters named 's', 'a', 'b' in string functions
code = re.sub(r'\bs\.s\b', 's.string', code)
code = re.sub(r'\bs\.n\b', 's.length', code)
code = re.sub(r'\ba\.s\b', 'a.string', code)
code = re.sub(r'\ba\.n\b', 'a.length', code)
code = re.sub(r'\bb\.s\b', 'b.string', code)
code = re.sub(r'\bb\.n\b', 'b.length', code)

# Common String_Slice field names in structs
code = re.sub(r'\.nm\.s\b', '.nm.string', code)
code = re.sub(r'\.nm\.n\b', '.nm.length', code)
code = re.sub(r'->nm\.s\b', '->nm.string', code)
code = re.sub(r'->nm\.n\b', '->nm.length', code)

# KW array field
code = re.sub(r'KW\[i\]\.k\.s', 'KW[i].k.string', code)

# Other common String_Slice vars
code = re.sub(r'\blb\.s\b', 'lb.string', code)
code = re.sub(r'\bppkg\.s\b', 'ppkg.string', code)
code = re.sub(r'\bppkg\.n\b', 'ppkg.length', code)

with open('ada83.c', 'w') as f:
    f.write(code)

print("Fixed String_Slice fields")
