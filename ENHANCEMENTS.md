# Ada83 Compiler Enhancement Path

## Current State
- **Lines**: 251 (ultra-compressed, Knuth-style)
- **Size**: 171KB (code-golfed, no comments)
- **Architecture**: Single-file Ada83→LLVM compiler
- **Style**: Haskell-like functional, C99, driver code

## ACATS Results (Baseline)
- **B-tests**: ~81% (error detection good but needs improvement)
- **C-tests**: ~44% (runtime checks need enhancement)
- **Overall**: Functional core compiler with room for constraint checking

## Enhancement Priorities (LRM & GNAT Reference)

### Phase 1: Runtime Constraint Checks (High Impact)
- Enhanced range checking for subtypes (CHK_RNG)
- Array index bounds checking (CHK_IDX)
- Discriminant checks (CHK_DSC)
- Division by zero (CHK_DIV)
- Access checks (CHK_ACC)

### Phase 2: Type System Completeness
- Fixed-point arithmetic codegen
- Derived type constraint inheritance
- Variant record discriminant checking
- Constrained array initialization

### Phase 3: Error Detection (B-tests)
- Lexer: malformed literals, illegal whitespace
- Parser: better error recovery, missing semicolons
- Semantic: duplicate declarations, type mismatches

### Phase 4: Missing Features
- Generic instantiation edge cases
- Task elaboration checks
- Representation clauses validation
- Pragma processing completeness

## Design Principles
1. **Ultra-compression**: Every byte counts
2. **No comments**: Code speaks for itself
3. **Functional style**: Minimal state, maximal clarity
4. **God-tier elegance**: Perfect semantics in minimal form

## Reference Materials
- `/reference/Ada83_LRM.md`: Language specification
- `/reference/gnat-design.md`: GNAT implementation guide
- `/reference/DIANA.md`: AST design patterns
- `/acats/`: Conformance test suite
