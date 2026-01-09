#!/usr/bin/env python3
"""
Fix ONLY Unsigned_Big_Integer field accesses in the bigint section (lines 74-307).
This is a targeted, careful fix for one struct type.
"""

import re

with open('ada83.c', 'r') as f:
    lines = f.readlines()

# Only process lines 74-307 (adjust for 0-indexing)
start_line = 73  # line 74 in 1-indexed
end_line = 307   # line 307 in 1-indexed

for i in range(start_line, min(end_line, len(lines))):
    line = lines[i]

    # Fix ->d to ->digits (for Unsigned_Big_Integer pointers)
    line = re.sub(r'(\w+)->d\b(?!\w)', r'\1->digits', line)

    # Fix ->n to ->count
    line = re.sub(r'(\w+)->n\b(?!\w)', r'\1->count', line)

    # Fix ->c to ->capacity
    line = re.sub(r'(\w+)->c\b(?!\w)', r'\1->capacity', line)

    # Fix ->s to ->is_negative
    line = re.sub(r'(\w+)->s\b(?!\w)', r'\1->is_negative', line)

    # Fix .d to .digits (for struct access)
    line = re.sub(r'(\w+)\.d\b(?!\w)', r'\1.digits', line)

    # Fix .n to .count
    line = re.sub(r'(\w+)\.n\b(?!\w)', r'\1.count', line)

    # Fix .c to .capacity
    line = re.sub(r'(\w+)\.c\b(?!\w)', r'\1.capacity', line)

    # Fix .s to .is_negative
    line = re.sub(r'(\w+)\.s\b(?!\w)', r'\1.is_negative', line)

    lines[i] = line

with open('ada83.c', 'w') as f:
    f.writelines(lines)

print("Fixed Unsigned_Big_Integer fields in lines 74-307")
