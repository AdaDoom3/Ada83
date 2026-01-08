# Reminders for Ada83.c Reformatting

## Current Prompt (2026-01-08)

Without dropping anything on the floor or leaving TODO turds - reformat ada83.c so that it would be more conducive for you to fit in your context (e.g. edit and iterate on) - all names must be fully expanded, the code's style must abide by Knuth's "Literate Programming" style with minimal, but sharp and tasteful comments (informed by GNAT LLVM sources in the /reference directory). Use "and" instead of "&&", "not" instead of "!", and "or" instead of "||".

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

## Name Mappings Identified

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

### Function Names (Lexer/Parser)
- `kl` → `keyword_lookup`
- `ln` → `lexer_new`
- `av` → `advance_char`
- `sw` → `skip_whitespace`
- `mt` → `make_token`
- `nx` → `next_token`
- `pk` → `peek_token`
- `ex` → `expect_token`

### Function Names (Code Generation)
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

### Variable Name Patterns
- `d` → `data` or `digit`
- `n` → `count` or `numerator`
- `c` → `capacity` or `carry` or `current` (context-dependent)
- `s` → `sign` or `string` or `symbol` (context-dependent)
- `p` → `pointer`
- `b` → `buffer` or `base` (context-dependent)
- `e` → `end` or `error` (context-dependent)
- `t` → `type` or `temporary` (context-dependent)
- `i`, `j`, `k` → `index`, `inner_index`, etc. (loop variables)
- `lo`, `hi` → `low_bound`, `high_bound`
- `sz` → `size`
- `mx` → `maximum`

## Style Guidelines from GNAT LLVM

Based on /reference/gnat examples:
- Comments explain **why**, not **what** the code obviously does
- Strategic placement: before major sections and complex algorithms
- Use precise terminology from Ada/LLVM domains
- Comments describe invariants, preconditions, and postconditions
- Avoid redundant comments that just restate the code

## Future Prompts

(Add additional prompts here as work progresses)
