#!/usr/bin/env python3
"""
Ada83 Code Reformatter V2 - More careful version
Only transforms variable/function names, not enum constants
"""

import re
import sys

# Don't transform short enum constants (N_*) - keep them as-is for code compatibility
# Only transform types, functions, and global variables

# Type name mappings - but NOT the enum values themselves
TYPE_NAMES = {
    r'\bU\*': 'Unsigned_Big_Integer *',
    r'\bU\b(?!\))': 'Unsigned_Big_Integer',
    r'\bR\*': 'Rational_Number *',
    r'\bR\b(?!\))': 'Rational_Number',
    r'\bS\b(?![a-z])': 'String_Slice',
    r'\bL\b(?![a-z])': 'Source_Location',
    r'\bTk\b': 'Token_Kind',
    r'\bTn\b': 'Token',
    r'\bLx\b': 'Lexer',
    r'\bPs\b': 'Parser',
    r'\bSy\b': 'Symbol',
    r'\bTy\b': 'Type_Info',
    r'\bNo\b': 'Syntax_Node',
    r'\bSm\b': 'Symbol_Manager',
    r'\bGn\b': 'Code_Generator',
    r'\bVk\b': 'Value_Kind',
    r'\bNV\b': 'Node_Vector',
    r'\bSV\b': 'Symbol_Vector',
}

# Function name mappings
FUNCTION_NAMES = {
    r'\bun\(': 'unsigned_bigint_new(',
    r'\buf\(': 'unsigned_bigint_free(',
    r'\bug\(': 'unsigned_bigint_grow(',
    r'\buca\(': 'unsigned_bigint_compare_abs(',
    r'\badc\(': 'add_with_carry(',
    r'\bsbb\(': 'subtract_with_borrow(',
    r'\bubop\(': 'unsigned_bigint_binary_op(',
    r'\buaa\(': 'unsigned_bigint_add_abs(',
    r'\busa\(': 'unsigned_bigint_sub_abs(',
    r'\bua\(': 'unsigned_bigint_add(',
    r'\bus\(': 'unsigned_bigint_subtract(',
    r'\bumb\(': 'unsigned_bigint_multiply_basic(',
    r'\bumk\(': 'unsigned_bigint_multiply_karatsuba(',
    r'\bum\(': 'unsigned_bigint_multiply(',
    r'\bufd\(': 'unsigned_bigint_from_decimal(',
    r'\bal\(': 'arena_allocate(',
    r'\bsd\(': 'string_duplicate(',
    r'\bsi\(': 'string_equal_ignore_case(',
    r'\bLC\(': 'string_to_lowercase(',
    r'\bsh\(': 'string_hash(',
    r'\bdie\(': 'fatal_error(',
}

COMMON_VARS = {
    r'\bIP\[': 'include_paths[',
    r'\bIPN\b': 'include_path_count',
    r'\bM\.': 'main_arena.',
    r'\bE\+\+': 'error_count++',
    r'\bE<': 'error_count<',
}

def replace_boolean_operators(code):
    """Replace C boolean operators with Ada-style."""
    code = re.sub(r'&&', ' and ', code)
    code = re.sub(r'\|\|', ' or ', code)
    code = re.sub(r'!\s*([a-zA-Z_])', r'not \1', code)
    code = re.sub(r'!\s*\(', r'not (', code)
    return code

def apply_transformations(code, mappings):
    """Apply regex-based transformations."""
    for pattern, replacement in mappings.items():
        code = re.sub(pattern, replacement, code)
    return code

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 reformat_v2.py input.c", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r') as f:
        code = f.read()

    print("Applying transformations...", file=sys.stderr)
    code = apply_transformations(code, TYPE_NAMES)
    code = apply_transformations(code, FUNCTION_NAMES)
    code = apply_transformations(code, COMMON_VARS)
    code = replace_boolean_operators(code)

    print(code)

if __name__ == '__main__':
    main()
