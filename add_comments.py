#!/usr/bin/env python3
"""
Add Literate Programming style comments to formatted Ada83 compiler code.
Comments should be minimal, sharp, and tasteful - explaining WHY, not WHAT.
"""

import re

def add_file_header(code):
    """Add file-level documentation."""
    header = """/* ===========================================================================
 * Ada83 Compiler - A complete Ada 1983 compiler targeting LLVM IR
 * ===========================================================================
 *
 * This compiler implements the Ada 1983 standard, translating Ada source code
 * directly to LLVM intermediate representation. The implementation emphasizes
 * clarity and correctness over optimization, making extensive use of arbitrary
 * precision arithmetic for compile-time evaluation.
 *
 * Architecture Overview:
 *   - Lexical Analysis: Character-by-character scanning with Ada-specific rules
 *   - Parsing: Recursive descent parser producing abstract syntax trees
 *   - Semantic Analysis: Symbol table management with scope tracking
 *   - Code Generation: Direct emission of LLVM IR with Ada semantics
 *
 * Key Design Decisions:
 *   - Arena allocation for AST nodes (no individual frees during compilation)
 *   - Arbitrary precision integers for accurate constant evaluation
 *   - Fat pointers for Ada's unconstrained arrays and access types
 *   - Direct LLVM IR emission rather than intermediate representations
 */

"""
    return header + code

def add_section_markers(code):
    """Add comments marking major sections."""
    sections = [
        (r'(typedef struct\s*\{[^}]*\}\s*Unsigned_Big_Integer;)',
         '/* ---------------------------------------------------------------------------\n'
         ' * ARBITRARY PRECISION ARITHMETIC\n'
         ' * ---------------------------------------------------------------------------\n'
         ' * Ada requires exact integer arithmetic regardless of size. We implement\n'
         ' * arbitrary precision using arrays of 64-bit limbs with sign-magnitude\n'
         ' * representation. Karatsuba multiplication provides O(n^1.585) complexity.\n'
         ' */\n\n\\1'),

        (r'(static void unsigned_bigint_multiply_karatsuba)',
         '/* Karatsuba multiplication: recursively split operands for sub-quadratic time.\n'
         ' * Base case (n < 20) uses schoolbook algorithm to avoid recursion overhead.\n'
         ' */\nstatic void unsigned_bigint_multiply_karatsuba'),

        (r'(typedef struct\s*\{[^}]*\}\s*String_Slice;)',
         '\n/* ---------------------------------------------------------------------------\n'
         ' * MEMORY MANAGEMENT AND STRING HANDLING\n'
         ' * ---------------------------------------------------------------------------\n'
         ' * Arena allocation strategy: all compiler data structures allocated from\n'
         ' * large blocks, freed en masse at end of compilation. This eliminates\n'
         ' * bookkeeping overhead and prevents memory leaks from early exits.\n'
         ' */\n\n\\1'),

        (r'(typedef enum\{T_EOF=0,)',
         '\n/* ---------------------------------------------------------------------------\n'
         ' * LEXICAL ANALYSIS\n'
         ' * ---------------------------------------------------------------------------\n'
         ' * Token-based lexer with full Ada 83 keyword recognition. Handles based\n'
         ' * literals (2#1010#, 16#FF#), character literals with context sensitivity\n'
         ' * (X\'Last vs \'X\'), and string literals with doubled quote escaping.\n'
         ' */\n\n\\1'),

        (r'(typedef enum\{N_ERR=0,NODE_IDENTIFIER)',
         '\n/* ---------------------------------------------------------------------------\n'
         ' * ABSTRACT SYNTAX TREE\n'
         ' * ---------------------------------------------------------------------------\n'
         ' * Unified node structure with discriminated union. Each syntax construct\n'
         ' * gets a unique node kind. Type information attached during semantic\n'
         ' * analysis for later code generation.\n'
         ' */\n\n\\1'),

        (r'(static.*generate_expression\()',
         '\n/* ---------------------------------------------------------------------------\n'
         ' * CODE GENERATION\n'
         ' * ---------------------------------------------------------------------------\n'
         ' * Direct LLVM IR emission with Ada semantics. Handles fat pointers for\n'
         ' * unconstrained types, runtime bounds checking, and nested subprogram\n'
         ' * static links. All arithmetic respects Ada\'s type model.\n'
         ' */\n\n\\1'),
    ]

    for pattern, replacement in sections:
        code = re.sub(pattern, replacement, code, count=1)

    return code

def add_complex_algorithm_comments(code):
    """Add comments for complex algorithms."""

    # Add comment for bounds checking
    code = re.sub(
        r'(static void gen_index_check\()',
        '/* Generate LLVM code for Ada index constraint checking.\n'
        ' * Branches to error handler if index outside [low, high] bounds.\n'
        ' * The check is mandatory even if optimizer could prove safety.\n'
        ' */\n\\1',
        code)

    # Add comment for fat pointer operations
    code = re.sub(
        r'(static void generate_fat_pointer\()',
        '/* Construct Ada fat pointer: {data_ptr, bounds_struct}.\n'
        ' * Bounds structure contains (low, high) for each dimension.\n'
        ' * Required for passing unconstrained arrays and slice operations.\n'
        ' */\n\\1',
        code)

    # Add comment for array aggregate generation
    code = re.sub(
        r'(static.*generate_aggregate\()',
        '/* Generate code for Ada aggregates: arrays or records.\n'
        ' * Handles positional (1, 2, 3), named (A => 1, B => 2),\n'
        ' * and others (others => 0) notations with full type checking.\n'
        ' */\n\\1',
        code)

    return code

def add_key_function_comments(code):
    """Add brief comments to key functions."""

    patterns = [
        (r'(static void arena_allocate\()',
         '/* Bump allocator: advances pointer, never frees individual allocations. */\n\\1'),

        (r'(static.*string_hash\()',
         '/* FNV-1a hash: fast, good distribution for symbol table lookups. */\n\\1'),

        (r'(static.*unsigned_bigint_new\()',
         '/* Allocate big integer with capacity for specified number of 64-bit limbs. */\n\\1'),

        (r'(static.*skip_whitespace\()',
         '/* Consume whitespace and Ada comments (-- to end of line). */\n\\1'),

        (r'(static.*keyword_lookup\()',
         '/* Binary search would be faster, but linear search suffices for compilation speed. */\n\\1'),

        (r'(static void new_temporary\()',
         '/* Allocate LLVM SSA virtual register for intermediate computation result. */\n\\1'),

        (r'(static void emit_label\()',
         '/* Emit LLVM basic block label for control flow target. */\n\\1'),

        (r'(static.*value_cast\()',
         '/* Insert LLVM type conversion instruction (inttoptr, ptrtoint, bitcast, etc). */\n\\1'),
    ]

    for pattern, replacement in patterns:
        code = re.sub(pattern, replacement, code, count=1)

    return code

def main():
    with open('ada83_reformatted_step2.c', 'r') as f:
        code = f.read()

    print("Adding file header...", file=__import__('sys').stderr)
    code = add_file_header(code)

    print("Adding section markers...", file=__import__('sys').stderr)
    code = add_section_markers(code)

    print("Adding algorithm comments...", file=__import__('sys').stderr)
    code = add_complex_algorithm_comments(code)

    print("Adding function comments...", file=__import__('sys').stderr)
    code = add_key_function_comments(code)

    print(code)

if __name__ == '__main__':
    main()
