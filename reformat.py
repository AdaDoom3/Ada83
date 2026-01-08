#!/usr/bin/env python3
"""
Ada83 Code Reformatter
Transforms abbreviated, terse C code into Literate Programming style.
"""

import re
import sys

# Type name mappings
TYPE_NAMES = {
    r'\bU\b': 'Unsigned_Big_Integer',
    r'\bR\b': 'Rational_Number',
    r'\bFP\b': 'Fat_Pointer',
    r'\bAB\b': 'Array_Bounds',
    r'\bTQ\b': 'Thread_Queue',
    r'\bTA\b': 'Thread_Args',
    r'\bS\b': 'String_Slice',
    r'\bL\b': 'Source_Location',
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
    r'\bRC\b': 'Reference_Counter',
    r'\bLU\b': 'Label_Use',
    r'\bGT\b': 'Generic_Type',
    r'\bLE\b': 'Label_Entry',
    r'\bNV\b': 'Node_Vector',
    r'\bSV\b': 'Symbol_Vector',
    r'\bRV\b': 'Reference_Vector',
    r'\bLV\b': 'Label_Vector',
    r'\bGV\b': 'Generic_Vector',
    r'\bFV\b': 'File_Vector',
    r'\bSLV\b': 'String_List_Vector',
    r'\bLEV\b': 'Label_Entry_Vector',
}

# Function name mappings
FUNCTION_NAMES = {
    # BigInt operations
    r'\bun\(': 'unsigned_bigint_new(',
    r'\buf\(': 'unsigned_bigint_free(',
    r'\bug\(': 'unsigned_bigint_grow(',
    r'\bUZ\(': 'UNSIGNED_BIGINT_NORMALIZE(',
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

    # Memory & strings
    r'\bal\(': 'arena_allocate(',
    r'\bsd\(': 'string_duplicate(',
    r'\bsi\(': 'string_equal_ignore_case(',
    r'\bLC\(': 'string_to_lowercase(',
    r'\bsh\(': 'string_hash(',
    r'\bdie\(': 'fatal_error(',

    # Lexer/Parser
    r'\bkl\(': 'keyword_lookup(',
    r'\bln\(': 'lexer_new(',
    r'\bav\(': 'advance_char(',
    r'\bsw\(': 'skip_whitespace(',
    r'\bmt\(': 'make_token(',
    r'\bnx\(': 'next_token(',
    r'\bpk\(': 'peek_token(',
    r'\bex\(': 'expect_token(',

    # Code generation
    r'\bnt\(': 'new_temporary(',
    r'\bnl\(': 'new_label(',
    r'\bgex\(': 'generate_expression(',
    r'\bgss\(': 'generate_statement_sequence(',
    r'\bgbf\(': 'generate_block_frame(',
    r'\bgdl\(': 'generate_declaration(',
    r'\blbl\(': 'emit_label(',
    r'\bbr\(': 'emit_branch(',
    r'\bcbr\(': 'emit_conditional_branch(',
    r'\bvcast\(': 'value_cast(',
    r'\bvcmp\(': 'value_compare(',
    r'\bvcmpi\(': 'value_compare_integer(',
    r'\bvcmpf\(': 'value_compare_float(',
    r'\bvbool\(': 'value_to_boolean(',
    r'\bvpow\(': 'value_power(',
    r'\bvpowi\(': 'value_power_integer(',
    r'\bvpows\(': 'value_power_float(',
    r'\bgfp\(': 'generate_fat_pointer(',
    r'\bgfpd\(': 'get_fat_pointer_data(',
    r'\bgfpb\(': 'get_fat_pointer_bounds(',
    r'\bgag\(': 'generate_aggregate(',

    # Symbol table
    r'\bsyf\(': 'symbol_find(',
    r'\bsyfa\(': 'symbol_find_arity(',
    r'\bsyi\(': 'symbol_insert(',
    r'\bsyd\(': 'symbol_define(',
    r'\bsya\(': 'symbol_add_overload(',

    # Type operations
    r'\btcc\(': 'type_concrete(',
    r'\brst\(': 'resolve_subtype(',
    r'\bth\(': 'type_hash(',
    r'\bact\(': 'ada_c_type(',
    r'\btk2v\(': 'token_kind_to_value_kind(',
    r'\brepr_cat\(': 'representation_category(',

    # Miscellaneous
    r'\besn\(': 'encode_symbol_name(',
    r'\bhnf\(': 'has_nested_function(',
    r'\bhnfs\(': 'has_nested_function_in_statements(',
    r'\bsbody\(': 'symbol_body(',
    r'\bsspec\(': 'symbol_spec(',
    r'\bgattr\(': 'get_attribute_name(',
    r'\bflbl\(': 'find_label(',
    r'\beex\(': 'emit_exception(',
    r'\belbl\(': 'emit_label_definition(',
    r'\badcl\(': 'add_declaration(',
    r'\bvt\(': 'value_type_string(',
    r'\bglbl_bb\(': 'get_or_create_label_basic_block(',

    # More operations
    r'\bslv\(': 'string_list_vector_append(',
    r'\bsev\(': 'symbol_vector_append(',
    r'\bnev\(': 'node_vector_append(',
    r'\blev\(': 'label_entry_vector_append(',
    r'\bfev\(': 'file_vector_append(',
    r'\brlv\(': 'reference_vector_append(',
}

# Common variable patterns (context-sensitive, applied more carefully)
COMMON_VARS = {
    r'\bIP\b': 'include_paths',
    r'\bIPN\b': 'include_path_count',
    r'\bM\b': 'main_arena',
    r'\bE\b': 'error_count',
    r'\bKW\b': 'keywords',
    r'\bTN\b': 'token_names',
    r'\bSEP_PKG\b': 'separate_package',
}

# Enum constants
ENUM_CONSTANTS = {
    r'\bVK_I\b': 'VALUE_KIND_INTEGER',
    r'\bVK_F\b': 'VALUE_KIND_FLOAT',
    r'\bVK_P\b': 'VALUE_KIND_POINTER',
    r'\bTY_B\b': 'TYPE_BOOLEAN',
    r'\bTY_C\b': 'TYPE_CHARACTER',
    r'\bTY_I\b': 'TYPE_INTEGER',
    r'\bTY_UI\b': 'TYPE_UNSIGNED_INTEGER',
    r'\bTY_E\b': 'TYPE_ENUMERATION',
    r'\bTY_D\b': 'TYPE_DERIVED',
    r'\bTY_F\b': 'TYPE_FLOAT',
    r'\bTY_UF\b': 'TYPE_UNIVERSAL_FLOAT',
    r'\bTY_FX\b': 'TYPE_FIXED_POINT',
    r'\bTY_AC\b': 'TYPE_ACCESS',
    r'\bTY_FT\b': 'TYPE_FAT',
    r'\bTY_S\b': 'TYPE_STRING',
    r'\bTY_A\b': 'TYPE_ARRAY',
    r'\bTY_R\b': 'TYPE_RECORD',
    r'\bRC_FLOAT\b': 'REPR_CAT_FLOAT',
    r'\bRC_PTR\b': 'REPR_CAT_POINTER',
    r'\bN_INT\b': 'NODE_INTEGER',
    r'\bN_REAL\b': 'NODE_REAL',
    r'\bN_CHAR\b': 'NODE_CHARACTER',
    r'\bN_STR\b': 'NODE_STRING',
    r'\bN_NULL\b': 'NODE_NULL',
    r'\bN_ID\b': 'NODE_IDENTIFIER',
    r'\bN_BIN\b': 'NODE_BINARY_OP',
    r'\bN_IF\b': 'NODE_IF',
    r'\bN_CS\b': 'NODE_CASE',
    r'\bN_LP\b': 'NODE_LOOP',
    r'\bN_BL\b': 'NODE_BLOCK',
    r'\bN_AG\b': 'NODE_AGGREGATE',
    r'\bN_ASC\b': 'NODE_ASSOCIATION',
    r'\bN_RN\b': 'NODE_RANGE',
    r'\bN_CHK\b': 'NODE_CHECK',
    r'\bN_PB\b': 'NODE_PROCEDURE_BODY',
    r'\bN_FB\b': 'NODE_FUNCTION_BODY',
    r'\bN_PD\b': 'NODE_PROCEDURE_DECL',
    r'\bN_FD\b': 'NODE_FUNCTION_DECL',
    r'\bN_CM\b': 'NODE_COMPONENT',
}

def replace_boolean_operators(code):
    """Replace C boolean operators with Ada-style operators."""
    # Replace && with and (careful with & which is bitwise)
    code = re.sub(r'&&', ' and ', code)
    # Replace || with or
    code = re.sub(r'\|\|', ' or ', code)
    # Replace ! with not (but not != or !!)
    code = re.sub(r'!\s*([a-zA-Z_])', r'not \1', code)
    code = re.sub(r'!\s*\(', r'not (', code)
    return code

def apply_transformations(code, mappings):
    """Apply regex-based transformations."""
    for pattern, replacement in mappings.items():
        code = re.sub(pattern, replacement, code)
    return code

def add_spacing(code):
    """Add strategic spacing for readability."""
    lines = code.split('\n')
    result = []
    prev_was_func = False

    for line in lines:
        # Add blank line before function definitions
        if re.match(r'^(static\s+)?\w+\s+\**\w+\s*\([^)]*\)\s*\{', line):
            if result and not prev_was_func:
                result.append('')
            prev_was_func = True
        else:
            prev_was_func = False
        result.append(line)

    return '\n'.join(result)

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 reformat.py input.c", file=sys.stderr)
        sys.exit(1)

    input_file = sys.argv[1]

    with open(input_file, 'r') as f:
        code = f.read()

    print("Applying type name transformations...", file=sys.stderr)
    code = apply_transformations(code, TYPE_NAMES)

    print("Applying function name transformations...", file=sys.stderr)
    code = apply_transformations(code, FUNCTION_NAMES)

    print("Applying enum constant transformations...", file=sys.stderr)
    code = apply_transformations(code, ENUM_CONSTANTS)

    print("Applying variable name transformations...", file=sys.stderr)
    code = apply_transformations(code, COMMON_VARS)

    print("Replacing boolean operators...", file=sys.stderr)
    code = replace_boolean_operators(code)

    print("Adding spacing...", file=sys.stderr)
    code = add_spacing(code)

    print(code)

if __name__ == '__main__':
    main()
