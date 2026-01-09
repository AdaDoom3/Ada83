#!/bin/bash
# Comprehensive String_Slice field fix
# Replace ALL .s and .n at end of expressions with .string and .length
# This is aggressive but the compiler will tell us if we break anything

# Fix .n → .length (but only when followed by word boundary)
sed -i 's/\.n\b/.length/g' ada83.c

# Fix .s → .string (but only when followed by word boundary or [)
sed -i 's/\.s\b/.string/g' ada83.c
sed -i 's/\.s\[/.string[/g' ada83.c

echo "Applied comprehensive String_Slice fixes"
