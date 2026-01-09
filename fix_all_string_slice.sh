#!/bin/bash
# Comprehensive fix for ALL String_Slice .s and .n accesses
# This replaces patterns like: variable.s → variable.string, variable.n → variable.length

# Fix all .s→.string (but avoid strings like "is", "as", etc.)
sed -i 's/\([a-zA-Z_][a-zA-Z0-9_]*\)\.s\b/\1.string/g' ada83.c

# Fix all .n→.length (but avoid patterns like line_number, column already)
sed -i 's/\([a-zA-Z_][a-zA-Z0-9_]*\)\.n\b/\1.length/g' ada83.c

# Now fix back any false positives we know about:
# None expected for these simple struct member accesses

echo "Fixed all String_Slice .s and .n accesses"
