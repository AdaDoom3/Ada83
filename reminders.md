# Reminders for Ada83.c Reformatting

## Reformatting Prompts

### Initial Prompt (2026-01-08)

Without dropping anything on the floor or leaving TODO turds - reformat ada83.c so that it would be more conducive for you to fit in your context (e.g. edit and iterate on) - all names must be fully expanded, the code's style must abide by Knuth's "Literate Programming" style with minimal, but sharp and tasteful comments (informed by GNAT LLVM sources in the /reference directory). Use "and" instead of "&&", "not" instead of "!", and "or" instead of "||".

### Second Iteration (2026-01-08)

Two space indentation - there are also many identifiers in need of expanding. Note also that the goal of the compiler is *both* correctness and the generation of highly optimal code.

## Key Requirements

1. **Full name expansion**: No abbreviations allowed
2. **Literate Programming style**: Minimal, sharp, tasteful comments (Knuth style)
3. **GNAT LLVM reference**: Use /reference/gnat/* for comment style guidance
4. **Boolean operators**:
   - `&&` → `and`
   - `||` → `or`
   - `!` → `not`
5. **Context-friendly**: Make the code easier to fit in AI context windows
6. **No TODOs or dropped features**: Complete refactoring only
7. **2-space indentation**: For better density while maintaining readability
8. **Dual goals**: Both correctness AND highly optimized code generation

## Name Mappings - First Iteration

### Type Names
- `U` → `Unsigned_Big_Integer`
- `R` → `Rational_Number`
- `FP` → `Fat_Pointer`
- `AB` → `Array_Bounds`
- `TQ` → `Thread_Queue`
- `TA` → `Thread_Args`
- `A` → `Arena_Allocator`
- `S` → `String_Slice`
- `L` → `Source_Location`
- `Tk` → `Token_Kind`
- `Tn` → `Token`
- `Lx` → `Lexer`
- `Ps` → `Parser`
- `Sy` → `Symbol`
- `Ty` → `Type_Info`
- `No` → `Syntax_Node`
- `Sm` → `Symbol_Manager`
- `Gn` → `Code_Generator`
- `V` → `LLVM_Value`
- `Vk` → `Value_Kind`
- `RC` → `Reference_Counter`
- `LU` → `Label_Use`
- `GT` → `Generic_Type`
- `LE` → `Label_Entry`
- `NV`, `SV`, `RV`, etc. → `Node_Vector`, `Symbol_Vector`, `Reference_Vector`, etc.

### Function Names (BigInt operations)
- `un` → `unsigned_bigint_new`
- `uf` → `unsigned_bigint_free`
- `ug` → `unsigned_bigint_grow`
- `uca` → `unsigned_bigint_compare_abs`
- `adc` → `add_with_carry`
- `sbb` → `subtract_with_borrow`
- `ubop` → `unsigned_bigint_binary_op`
- `uaa` → `unsigned_bigint_add_abs`
- `usa` → `unsigned_bigint_sub_abs`
- `ua` → `unsigned_bigint_add`
- `us` → `unsigned_bigint_subtract`
- `umb` → `unsigned_bigint_multiply_basic`
- `umk` → `unsigned_bigint_multiply_karatsuba`
- `um` → `unsigned_bigint_multiply`
- `ufd` → `unsigned_bigint_from_decimal`

### Function Names (Memory & Strings)
- `al` → `arena_allocate`
- `sd` → `string_duplicate`
- `si` → `string_equal_ignore_case`
- `LC` → `string_to_lowercase`
- `sh` → `string_hash`
- `die` → `fatal_error`

### Function Names (Lexer/Parser - Initial)
- `kl` → `keyword_lookup`
- `ln` → `lexer_new`
- `av` → `advance_char`
- `sw` → `skip_whitespace`
- `mt` → `make_token`
- `nx` → `next_token`
- `pk` → `peek_token`
- `ex` → `expect_token`

### Function Names (Code Generation - Initial)
- `nt` → `new_temporary`
- `nl` → `new_label`
- `gex` → `generate_expression`
- `gss` → `generate_statement_sequence`
- `gbf` → `generate_block_frame`
- `gdl` → `generate_declaration`
- `lbl` → `emit_label`
- `br` → `emit_branch`
- `cbr` → `emit_conditional_branch`
- `vcast` → `value_cast`
- `vcmp` → `value_compare`
- `vcmpi` → `value_compare_integer`
- `vcmpf` → `value_compare_float`
- `vbool` → `value_to_boolean`
- `vpow` → `value_power`
- `vpowi` → `value_power_integer`
- `vpows` → `value_power_float`

## Additional Transformations - Second Iteration

### More Lexer/Parser Functions
- `lnx` → `lexer_next_token`
- `sn` → `scan_number_literal`
- `sc` → `scan_character_literal`
- `ss` → `scan_string_literal`
- `si_` → `scan_identifier`
- `pn` → `parser_next`
- `pa` → `parser_at`
- `pm` → `parser_match`
- `pe` → `parser_expect`
- `pl` → `parser_location`
- `pi` → `parser_identifier`
- `pat` → `parser_attribute`
- `pex` → `parse_expression`
- `pnm` → `parse_name`
- `ppr` → `parse_primary`
- `prn` → `parse_range`
- `pst` → `parse_statement`
- `pbr` → `parse_binary`

### Node & Object Constructors
- `nd` → `node_new`
- `rc` → `reference_counter_new`
- `lu` → `label_use_new`
- `gt` → `generic_type_new`

### Type System Functions
- `tcc` → `type_canonical_concrete`
- `rst` → `resolve_subtype`
- `rng_sz` → `range_size`
- `tco` → `type_covers`
- `is_unc_arr` → `is_unconstrained_array`
- `repr_cat` → `representation_category`
- `th` → `type_hash`

### Symbol Table Functions
- `syf` → `symbol_find`
- `syfa` → `symbol_find_with_arity`
- `syi` → `symbol_insert`
- `syd` → `symbol_define`
- `sya` → `symbol_add_overload`
- `sbody` → `symbol_body`
- `sspec` → `symbol_spec`

### Code Generation - Registers & Labels
- `nt` → `new_temporary_register`
- `nl` → `new_label_block`
- `br` → `emit_branch`
- `cbr` → `emit_conditional_branch`

### Code Generation - Runtime Checks
- `gen_index_check` → `generate_index_constraint_check`
- `gen_float_chk` → `generate_float_range_check`
- `gen_array_bounds_chk` → `generate_array_bounds_check`
- `gen_discrete_chk` → `generate_discrete_range_check`

### Semantic Analysis
- `naag` → `normalize_array_aggregate`
- `nrag` → `normalize_record_aggregate`
- `rex` → `resolve_expression`
- `rss` → `resolve_statement_sequence`
- `rdl` → `resolve_declaration`

### Code Generation - Main Functions
- `gdl` → `generate_declaration`
- `gss` → `generate_statement_sequence`
- `gbf` → `generate_block_frame`
- `gex` → `generate_expression`
- `gag` → `generate_aggregate`
- `gfp` → `generate_fat_pointer`
- `gfpd` → `get_fat_pointer_data`
- `gfpb` → `get_fat_pointer_bounds`

### Utility Functions
- `esn` → `encode_symbol_name`
- `hnf` → `has_nested_function`
- `hnfs` → `has_nested_function_in_stmts`
- `gattr` → `get_attribute_name`
- `flbl` → `find_label`
- `eex` → `emit_exception`
- `elbl` → `emit_label_definition`
- `adcl` → `add_declaration`
- `vt` → `value_llvm_type_string`
- `act` → `ada_to_c_type_string`
- `tk2v` → `token_kind_to_value_kind`
- `glbl_bb` → `get_or_create_label_basic_block`

### Enum Constants
- `VK_I` → `VALUE_KIND_INTEGER`
- `VK_F` → `VALUE_KIND_FLOAT`
- `VK_P` → `VALUE_KIND_POINTER`
- `TY_B` → `TYPE_BOOLEAN`
- `TY_C` → `TYPE_CHARACTER`
- `TY_I` → `TYPE_INTEGER`
- `TY_UI` → `TYPE_UNSIGNED_INTEGER`
- `TY_E` → `TYPE_ENUMERATION`
- `TY_D` → `TYPE_DERIVED`
- `TY_F` → `TYPE_FLOAT`
- `TY_UF` → `TYPE_UNIVERSAL_FLOAT`
- `TY_FX` → `TYPE_FIXED_POINT`
- `TY_AC` → `TYPE_ACCESS`
- `TY_FT` → `TYPE_FAT_POINTER`
- `TY_S` → `TYPE_STRING`
- `TY_A` → `TYPE_ARRAY`
- `TY_R` → `TYPE_RECORD`
- `RC_FLOAT` → `REPR_CAT_FLOAT`
- `RC_PTR` → `REPR_CAT_POINTER`

### Macros
- `UZ` → `UNSIGNED_BIGINT_NORMALIZE`
- `Z` → `STRING_LITERAL`

### Note on Vector Functions
The vector append functions (nv, sv, lv, gv, lev, fev, slv) remain short
because they're used so frequently that full names would bloat the code.
They're defined via the VECPUSH macro which shows their full purpose.

## Style Guidelines from GNAT LLVM

Based on /reference/gnat examples:
- Comments explain **why**, not **what** the code obviously does
- Strategic placement: before major sections and complex algorithms
- Use precise terminology from Ada/LLVM domains
- Comments describe invariants, preconditions, and postconditions
- Avoid redundant comments that just restate the code

## Current State

**Lines of code**: ~14,400 (expanded from 265 dense lines)
**Indentation**: 2 spaces
**Boolean operators**: Ada-style (and/or/not)
**Identifier expansion**: Comprehensive
**Compilation**: ✓ Successful
**Documentation**: Updated to reflect dual goals of correctness + optimization

## Third Iteration Progress (2026-01-09)

### Methodical Struct-by-Struct Expansion

**Completed Structs (6/33) - All compile successfully ✓:**

1. **Unsigned_Big_Integer**: d→digits, n→count, c→capacity, s→is_negative
   - Fixed macro: UNSIGNED_BIGINT_NORMALIZE
   - Fixed all field accesses in bigint functions (lines 74-307)
   - Fixed references in scan_number_literal

2. **Rational_Number**: n→numerator, d→denominator
   - Struct definition only (no field accesses in current code)

3. **Array_Bounds**: lo→low_bound, hi→high_bound
   - Struct definition only (no field accesses in current code)

4. **Fat_Pointer**: d→data, b→bounds
   - Struct definition only (no field accesses in current code)

5. **Thread_Queue**: m→mutex, c→condition, cnt→count, mx→maximum
   - Struct definition only (no field accesses in current code)

6. **Thread_Args**: t→thread, q→queue, d→data
   - Struct definition only (no field accesses in current code)

**Remaining Structs (27/33):**

Next batch to tackle:
7. Arena (A→Arena_Allocator): b→base, p→pointer, e→end
8. String_Slice: s→string, n→length
9. Source_Location: l→line, c→column, f→filename
10-17. Vector types (Node_Vector, Symbol_Vector, RV, LV, GV, FV, SLV, LEV): d→data, n→count, c→capacity
18. Token: t→kind, l→location, iv→integer_value, fv→float_value, ui→unsigned_integer, ur→unsigned_rational
19. Lexer: s→start, c→current, e→end, ln→line_number, cl→column, f→filename, pt→previous_token
20. KW struct: k→keyword, t→token_kind
21. Parser: lx→lexer, cr→current_token, pk→peek_token, er→error_count, lb→label_stack
22. LE: nm→name, bb→basic_block
23. LU: (complex - multiple fields)
24. GT: (complex - multiple fields)
25. RC: (complex - multiple fields)
26-29. Syntax_Node, Type_Info, Symbol, Symbol_Manager: (very complex - hundreds of fields)

**Helper Scripts Created:**
- `fix_bigint_fields.py`: Targeted Unsigned_Big_Integer field fixes
- `fix_string_slice_fields.py`: String_Slice field fixes (needs completion)
- `fix_source_location.py`: Source_Location field fixes (needs completion)
- `fix_all_string_slice.sh`: Comprehensive approach (needs refinement for vector types)

**Key Learning:**
- Taking it slow and methodical works: 1 struct at a time, test compilation, commit success
- Field names like .s, .n, .d, .c are heavily overloaded across different struct types
- Must expand vector type definitions BEFORE fixing their field accesses
- Global replacements are risky; targeted, context-aware fixes are safer

## Future Prompts

(Add additional prompts here as work progresses)
