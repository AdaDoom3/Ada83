# A-Series ACATS Test Findings

## Current Status
**73/102 A-series tests passing (71.6% pass rate)**

## Major Achievements
1. Fixed symbol name generation for deterministic linking by simplifying format from `PACKAGE__NAME.params.sc.el` to `PACKAGE__NAME.params`.
2. Fixed generic instantiation static link handling by setting `inst->sy->lv=0` after instantiation, ensuring they are treated as top-level procedures rather than nested ones.
3. Fixed generic procedure body generation by storing bodies in GT->bd and instantiating them along with specs.
4. Fixed invalid LLVM IR from forward declarations inside function bodies by skipping N_PD/N_FD emission in N_BL blocks.
5. Fixed generic instantiation procedure body emission by adding `mkl0()` function to recursively set `lv=0` for all procedures inside instantiated generic packages, ensuring they are emitted by the code generation loop.
6. Fixed symbol name uniqueness for generic homographs by appending node pointer to procedure names, preventing LLVM redefinition errors.

## Remaining Issues (44 tests)

### 1. Runtime Crashes (18 tests)
Segfaults in generated code during execution. Tests compile and link successfully but crash at runtime.

**Examples:** a21001a, a22002a, a27003a, a35502q, a35801f

**Investigation:**
- LLVM IR is syntactically valid
- Crashes occur in A21001A.0 main procedure
- Native compilation also produces segfaults
- Likely causes: null pointer dereference, bad array indexing, or incorrect aggregate initialization

### 2. Generic Instantiation Code Generation (some tests still failing)
Generic procedure bodies not always being emitted when instantiated.

**Examples:** a35502r (still failing), a63202a (still failing), a83009a (NOW PASSING ✓)

**Status:** MOSTLY FIXED
- ✓ Static link handling corrected: `inst->sy->lv=0` prevents frame parameter passing
- ✓ Procedure body emission fixed for package-level generics: Added `mkl0(bd)` call in N_GINST to recursively set `lv=0`
- ✓ Symbol name uniqueness: Node pointer appended to prevent redefinition errors
- ✗ Some edge cases remain (nested generic procedures)

**Fix Applied (ada83.c:216-217):**
```c
static void mkl0(No*n){
  if(!n)return;
  if(n->sy)n->sy->lv=0;
  switch(n->k){
    case N_PKB:for(uint32_t i=0;i<n->pb.dc.n;i++)mkl0(n->pb.dc.d[i]);break;
    case N_PB:case N_FB:case N_PD:case N_FD:
      for(uint32_t i=0;i<n->bd.dc.n;i++)mkl0(n->bd.dc.d[i]);break;
    default:break;
  }
}

// In N_GINST case:
rdl(SM,bd);
mkl0(bd);  // ← Recursively mark all procedures as top-level
```

**Remaining Issues:**
- Generic procedures instantiated directly (not in packages) may still have issues
- Nested generic instantiations may need special handling

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
