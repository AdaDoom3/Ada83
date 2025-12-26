# A-Series ACATS Test Findings

## Current Status
**100/144 A-series tests passing (69% pass rate)**

## Major Achievement
Fixed symbol name generation for deterministic linking by simplifying format from `PACKAGE__NAME.params.sc.el` to `PACKAGE__NAME.params`.

## Remaining Issues (44 tests)

### 1. Runtime Crashes (18 tests)
Segfaults in generated code during execution. Tests compile and link successfully but crash at runtime.

**Examples:** a21001a, a22002a, a27003a, a35502q, a35801f

**Investigation:**
- LLVM IR is syntactically valid
- Crashes occur in A21001A.0 main procedure
- Native compilation also produces segfaults
- Likely causes: null pointer dereference, bad array indexing, or incorrect aggregate initialization

### 2. Linking Failures (15 tests)
Unresolved symbol errors, primarily related to nested procedure handling.

**Examples:** a35502r, a63202a, a83009a

**Root Causes:**
- `%__frame` undefined: Generic instantiations treated as nested procedures but calling context lacks frame
- Malformed LLVM IR: declare statements in wrong locations
- Duplicate function definitions

**From GNAT Study:**
GNAT uses "Activation Records" for nested procedures:
- Creates activation record type for procedures with nested subprograms
- Passes static link (ptr to activation record) as extra hidden parameter
- Stores captured variables in activation record
- Generic instantiations should NOT receive static links

**Problem in ada83.c:**
Generic instantiations like `PROCEDURE NP IS NEW P(COLOR);` get assigned lv > 0 (nested level), causing code gen to expect/pass static links they don't need.

### 3. Compilation Errors (11 tests)
Diverse compiler bugs in parsing and type checking.

**Breakdown:**
- 5 type mismatches
- 2 undefined identifiers (SYSTEM, G)
- 1 parser error ("exp 'END' got 'id'")
- 1 aggregate gap error
- 1 package scoping error
- 1 unknown

## Key Insights from GNAT Sources

From `gnatllvm-subprograms.adb`:

```ada
-- Activation record parameter handling
Has_S_Link := Has_Activation_Record (E);

-- Get variable from activation record
function Get_From_Activation_Record (E : Evaluable_Kind_Id)
  return GL_Value;

-- Nested function emission
package Nested_Functions is new Table.Table;
```

**Architecture:**
1. Top-level procedures: no activation record needed
2. Procedures with nested subprograms: allocate activation record, store as `%__frame`
3. Nested procedures: receive static link parameter `%__slnk`
4. Generic instantiations: treated as siblings, not nested - NO static link

## Recommended Fixes

### High Priority
1. **Fix frame allocation logic** in N_PB/N_FB cases:
   - Only allocate %__frame when procedure actually has nested procedures
   - Don't pass static links to generic instantiations

2. **Fix aggregate initialization** causing runtime crashes:
   - Review gag() function for array aggregate handling
   - Verify bounds checking and index calculations

3. **Fix type resolution** for the 5 type mismatch cases

### Medium Priority
4. Add SYSTEM package stub for standard environment
5. Fix parser to handle all END statement variations
6. Improve package visibility/scoping logic

## Test Categories

**Passing (100):**
- Basic language features
- Simple procedures
- Package specifications
- Most declarations and types

**Failing - Runtime (18):**
- Complex aggregates
- Nested data structures
- Some array operations

**Failing - Linking (15):**
- Generic instantiations with nested context
- Procedures expecting activation records

**Failing - Compilation (11):**
- Advanced type features
- Special packages (SYSTEM)
- Complex scoping scenarios

## References
- `/home/user/Ada83/reference/gnat-design.md` - GNAT design documentation
- `/home/user/Ada83/reference/gnat/gnatllvm-subprograms.adb` - Activation record implementation
- `/home/user/Ada83/reference/Ada83_LRM.md` - Language Reference Manual
