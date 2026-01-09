#!/bin/bash
# Check which structs are actually used beyond their definition

structs=(
  "Unsigned_Big_Integer"
  "Rational_Number"
  "Fat_Pointer"
  "Array_Bounds"
  "Thread_Queue"
  "Thread_Args"
  "Arena_Allocator"
  "String_Slice"
  "Source_Location"
  "Token"
  "Lexer"
  "Node_Vector"
  "Symbol_Vector"
  "Syntax_Node"
  "Type_Info"
  "Symbol"
)

for struct in "${structs[@]}"; do
  # Count uses beyond typedef line
  count=$(grep -n "\b$struct\b" ada83.c | grep -v "^[0-9]*:typedef struct" | grep -v "^[0-9]*:} $struct;" | wc -l)
  printf "%-25s: %3d uses\n" "$struct" "$count"
done
