# Ada83 Compiler Development Reminders

## Current Task (2026-01-19)

Fix ada83.c according to increasingly complex custom tests with the following requirements:

### Code Style Requirements

1. **Fully Expanded Names**: All variable and function names must be fully expanded (no abbreviations)
2. **Literate Programming Style**: Follow Knuth's "Literate Programming" style
   - Minimal but sharp and tasteful comments
   - Comments should explain *why*, not *what*
   - Informed by GNAT LLVM sources in `/reference` directory
3. **Logical Operators**: Use C99 iso646.h macros consistently:
   - Use `and` instead of `&&`
   - Use `or` instead of `||`
   - Use `not` instead of `!`
4. **No TODO turds**: Complete all implementations fully, no placeholders
5. **Nothing dropped on the floor**: All functionality must be preserved and working

### Reference Materials

- GNAT LLVM sources: `/reference/gnat/`
- Ada83 LRM: `/reference/Ada83_LRM.md`
- DIANA specification: `/reference/DIANA.md`
- GNAT design docs: `/reference/gnat-design.md`
- Variable mapping: `/home/user/Ada83/VARIABLE_MAPPING.md` (comprehensive mapping created)

### Test Suite

- Main test harness: `test.sh`
- ACATS tests: `acats/*.ada`
- Test classes: A (acceptance), B (illegality), C (executable), D (numerics), E (inspection), L (post-compilation)
- Current B-test coverage: 33% (b22003a shows 1/3 errors detected)

### Git Workflow

- Branch: `claude/fix-ada83-literate-gvKiU`
- Commit with clear messages
- Push when complete with: `git push -u origin claude/fix-ada83-literate-gvKiU`

## Progress Summary

### Completed (2026-01-19)
1. ✅ Fixed T_ERR token handling in parser for better lexical error reporting
2. ✅ Added `string_equal()` function for exact string comparisons
3. ✅ Created comprehensive VARIABLE_MAPPING.md with all abbreviation mappings
4. ✅ Documented Symbol_Manager struct with full literate programming style (53 lines of comments)
5. ✅ Expanded all Symbol_Manager field names in struct definition (26 fields expanded)
6. ✅ Code already uses `and`, `or`, `not` throughout (via iso646.h) - no changes needed

### In Progress
- Systematically replacing ~500+ Symbol_Manager field references throughout 15,555-line codebase
- Need to carefully replace each field access and test after batches

### Remaining Work (Estimated Scope)
This is a MASSIVE refactoring requiring thousands of careful edits:

**Struct Field Expansions** (~2000+ references total):
- Type_Info: `k` → `kind` (~767 references via `->k`)
- Syntax_Node: `k` → `kind` (~146 references via `.k`)
- Symbol: `k` → `kind` (~hundreds of references)
- Symbol_Manager: 26 fields → expanded names (~500+ references)
- Code_Generator: 15+ fields → expanded names (~300+ references)
- Parser, Lexer: ~10 fields → expanded names (~200+ references)

**Enum Value Expansions** (~1000+ references):
- Token_Kind: ~30 abbreviated values (T_LP, T_RP, T_CM, etc.)
- Node_Kind: ~80 abbreviated values (N_AG, N_BIN, N_UN, etc.)

**Function Parameters & Local Variables** (~5000+ references):
- Context-dependent single letters (i, j, k, n, s, p, t, etc.)
- Two-letter abbreviations (nm, ty, sp, ch, tn, dt, etc.)
- Must be done carefully with context awareness

**Literate Comments**:
- Add/enhance comments for remaining ~200 functions
- Document complex algorithms and design decisions
- Reference Ada 83 LRM and DIANA where appropriate

### Strategy for Completion

**Phase 1**: Core Struct Fields (Foundation)
1. Complete Symbol_Manager field replacements
2. Expand Type_Info.k and Syntax_Node.k (most frequent)
3. Expand Symbol.k
4. Test thoroughly after each struct

**Phase 2**: Secondary Structs
5. Code_Generator fields
6. Parser/Lexer fields
7. Test thoroughly

**Phase 3**: Enum Values (can use sed safely)
8. Token_Kind values
9. Node_Kind values
10. Test thoroughly

**Phase 4**: Function-Level (Most Complex)
11. Function parameters (start with most common: nm, ty, sp)
12. Local variables (context-dependent - most time-consuming)
13. Continuous testing

**Phase 5**: Documentation
14. Add literate comments to remaining functions
15. Final review and cleanup

### Challenges & Notes
- File is 15,555 lines - too large to load entirely in some editors
- Many abbreviations are context-dependent (e.g., `s` = symbol/string/start)
- Need to preserve all functionality - no breaking changes
- Must test incrementally to catch issues early
- Estimated total effort: 20-40 hours of careful refactoring

### Tools & Approach
- sed for batch replacements (with careful regex)
- Individual Edit tool for context-dependent changes
- make compiler && ./test.sh after each major batch
- Git commits after each phase for rollback safety

## Future Prompts

(Future task prompts will be added here to maintain continuity)
