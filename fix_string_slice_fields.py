#!/usr/bin/env python3
"""
Fix String_Slice fields: s→string, n→length
Need to be very careful as these are common letters.
"""

import re

with open('ada83.c', 'r') as f:
    code = f.read()

# String_Slice fields: .s → .string, .n → .length
# BUT only for String_Slice, not other structs

# The safest approach: fix specific patterns where we KNOW it's a String_Slice
# Look for patterns like: variable.s[ or variable.n + or s.s or s.n

# Fix .n to .length for String_Slice
code = re.sub(r'(\ba\.)n\b', r'\1length', code)  # String_Slice a, b parameters
code = re.sub(r'(\bb\.)n\b', r'\1length', code)
code = re.sub(r'(\bs\.)n\b', r'\1length', code)  # String_Slice s parameter

# Fix .s to .string for String_Slice
code = re.sub(r'(\ba\.)s\b', r'\1string', code)
code = re.sub(r'(\bb\.)s\b', r'\1string', code)
code = re.sub(r'(\bs\.)s\b', r'\1string', code)

# Also fix the KW array which has String_Slice fields
code = re.sub(r'(KW\[i\]\.)k\b', r'\1keyword', code)

with open('ada83.c', 'w') as f:
    f.write(code)

print("Fixed String_Slice fields")
