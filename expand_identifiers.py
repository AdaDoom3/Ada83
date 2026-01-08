#!/usr/bin/env python3
"""
Comprehensive identifier expansion for Ada83 compiler.
Expands ALL remaining abbreviated names for clarity.
"""

import re
import sys

# Complete type name mappings
TYPE_MAPPINGS = {
    r'\bFP\b': 'Fat_Pointer',
    r'\bAB\b': 'Array_Bounds',
    r'\bTQ\b': 'Thread_Queue',
    r'\bTA\b': 'Thread_Args',
}

# Macro mappings
MACRO_MAPPINGS = {
    r'\bUZ\(': 'UNSIGNED_BIGINT_NORMALIZE(',
    r'\bZ\(': 'STRING_LITERAL(',
    r'\bN\b(?=\s*\))': 'NULL_STRING_SLICE',  # N when used alone
}

# Complete function name mappings
FUNCTION_MAPPINGS = {
    # Lexer functions
    r'\blnx\(': 'lexer_next_token(',
    r'\bsn\(': 'scan_number_literal(',
    r'\bsc\(': 'scan_character_literal(',
    r'\bss\(': 'scan_string_literal(',
    r'\bsi_\(': 'scan_identifier(',

    # Node constructors
    r'\bnd\(': 'node_new(',
    r'\brc\(': 'reference_counter_new(',
    r'\blu\(': 'label_use_new(',
    r'\bgt\(': 'generic_type_new(',

    # Vector operations
    r'\bnv\(': 'node_vector_append(',
    r'\bsv\(': 'symbol_vector_append(',
    r'\blv\(': 'label_vector_append(',
    r'\bgv\(': 'generic_vector_append(',
    r'\blev\(': 'label_entry_vector_append(',
    r'\bfev\(': 'file_vector_append(',
    r'\bslv\(': 'string_list_vector_append(',

    # Parser functions
    r'\bpn\(': 'parser_next(',
    r'\bpa\(': 'parser_at(',
    r'\bpm\(': 'parser_match(',
    r'\bpe\(': 'parser_expect(',
    r'\bpl\(': 'parser_location(',
    r'\bpi\(': 'parser_identifier(',
    r'\bpat\(': 'parser_attribute(',
    r'\bpex\(': 'parse_expression(',
    r'\bpnm\(': 'parse_name(',
    r'\bppr\(': 'parse_primary(',
    r'\bprn\(': 'parse_range(',
    r'\bpst\(': 'parse_statement(',
    r'\bpbr\(': 'parse_binary(',

    # Type checking functions
    r'\btcc\(': 'type_canonical_concrete(',
    r'\brst\(': 'resolve_subtype(',
    r'\brng_sz\(': 'range_size(',
    r'\btco\(': 'type_covers(',
    r'\bis_unc_arr\(': 'is_unconstrained_array(',
    r'\brepr_cat\(': 'representation_category(',

    # Symbol table functions
    r'\bsyf\(': 'symbol_find(',
    r'\bsyfa\(': 'symbol_find_with_arity(',
    r'\bsyi\(': 'symbol_insert(',
    r'\bsyd\(': 'symbol_define(',
    r'\bsya\(': 'symbol_add_overload(',

    # Code generation - temporaries and labels
    r'\bnt\(': 'new_temporary_register(',
    r'\bnl\(': 'new_label_block(',
    r'\bbr\(': 'emit_branch(',
    r'\bcbr\(': 'emit_conditional_branch(',

    # Code generation - checks
    r'\bgen_index_check\(': 'generate_index_constraint_check(',
    r'\bgen_float_chk\(': 'generate_float_range_check(',
    r'\bgen_array_bounds_chk\(': 'generate_array_bounds_check(',
    r'\bgen_discrete_chk\(': 'generate_discrete_range_check(',

    # Semantic analysis - aggregates
    r'\bnaag\(': 'normalize_array_aggregate(',
    r'\bnrag\(': 'normalize_record_aggregate(',
    r'\brex\(': 'resolve_expression(',
    r'\brss\(': 'resolve_statement_sequence(',
    r'\brdl\(': 'resolve_declaration(',

    # Code generation - main functions
    r'\bgdl\(': 'generate_declaration(',
    r'\bgss\(': 'generate_statement_sequence(',
    r'\bgbf\(': 'generate_block_frame(',
    r'\bgex\(': 'generate_expression(',
    r'\bgag\(': 'generate_aggregate(',
    r'\bgfp\(': 'generate_fat_pointer(',
    r'\bgfpd\(': 'get_fat_pointer_data(',
    r'\bgfpb\(': 'get_fat_pointer_bounds(',

    # Utility functions
    r'\besn\(': 'encode_symbol_name(',
    r'\bhnf\(': 'has_nested_function(',
    r'\bhnfs\(': 'has_nested_function_in_stmts(',
    r'\bsbody\(': 'symbol_body(',
    r'\bsspec\(': 'symbol_spec(',
    r'\bgattr\(': 'get_attribute_name(',
    r'\bflbl\(': 'find_label(',
    r'\beex\(': 'emit_exception(',
    r'\belbl\(': 'emit_label_definition(',
    r'\badcl\(': 'add_declaration(',
    r'\bvt\(': 'value_llvm_type_string(',
    r'\bact\(': 'ada_to_c_type_string(',
    r'\btk2v\(': 'token_kind_to_value_kind(',
    r'\bth\(': 'type_hash(',
    r'\bglbl_bb\(': 'get_or_create_label_basic_block(',

    # Value operations
    r'\bvcast\(': 'value_cast(',
    r'\bvcmp\(': 'value_compare(',
    r'\bvcmpi\(': 'value_compare_integer(',
    r'\bvcmpf\(': 'value_compare_float(',
    r'\bvbool\(': 'value_to_boolean(',
    r'\bvpow\(': 'value_power(',
    r'\bvpowi\(': 'value_power_integer(',
    r'\bvpows\(': 'value_power_float(',
}

# Enum value mappings (for VALUE_KIND, etc.)
ENUM_MAPPINGS = {
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
    r'\bTY_FT\b': 'TYPE_FAT_POINTER',
    r'\bTY_S\b': 'TYPE_STRING',
    r'\bTY_A\b': 'TYPE_ARRAY',
    r'\bTY_R\b': 'TYPE_RECORD',
    r'\bRC_FLOAT\b': 'REPR_CAT_FLOAT',
    r'\bRC_PTR\b': 'REPR_CAT_POINTER',
}

def apply_transformations(code, mappings, description):
    """Apply regex-based transformations with progress indication."""
    print(f"  {description}...", file=sys.stderr)
    for pattern, replacement in mappings.items():
        code = re.sub(pattern, replacement, code)
    return code

def main():
    if len(sys.argv) != 2:
        print("Usage: python3 expand_identifiers.py input.c", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], 'r') as f:
        code = f.read()

    print("Expanding identifiers:", file=sys.stderr)
    code = apply_transformations(code, TYPE_MAPPINGS, "Type names")
    code = apply_transformations(code, MACRO_MAPPINGS, "Macro names")
    code = apply_transformations(code, FUNCTION_MAPPINGS, "Function names")
    code = apply_transformations(code, ENUM_MAPPINGS, "Enum constants")

    print(code)

if __name__ == '__main__':
    main()
