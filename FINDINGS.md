# A-Series ACATS Test Findings

## Current Status
**101/145 A-series tests passing (70% pass rate)**

## Major Achievements
1. Fixed symbol name generation for deterministic linking by simplifying format from `PACKAGE__NAME.params.sc.el` to `PACKAGE__NAME.params`.
2. Fixed generic instantiation static link handling by setting `inst->sy->lv=0` after instantiation, ensuring they are treated as top-level procedures rather than nested ones.
3. Fixed generic procedure body generation by storing bodies in GT->bd and instantiating them along with specs.
4. Fixed invalid LLVM IR from forward declarations inside function bodies by skipping N_PD/N_FD emission in N_BL blocks.

## Remaining Issues (44 tests)

### 1. Runtime Crashes (18 tests)
Segfaults in generated code during execution. Tests compile and link successfully but crash at runtime.

**Examples:** a21001a, a22002a, a27003a, a35502q, a35801f

**Investigation:**
- LLVM IR is syntactically valid
- Crashes occur in A21001A.0 main procedure
- Native compilation also produces segfaults
- Likely causes: null pointer dereference, bad array indexing, or incorrect aggregate initialization

### 2. Generic Instantiation Code Generation (15 tests)
Generic procedure bodies not being emitted when instantiated inside blocks.

**Examples:** a35502r, a63202a, a83009a

**Status:** PARTIALLY FIXED
- ✓ Static link handling corrected: `inst->sy->lv=0` prevents frame parameter passing
- ✗ Procedure body code generation missing: Instantiated procedures declared but not defined

**Root Cause:**
Code generation loop in main() only emits code for procedures where `s->lv==0` AND the procedure is in the symbol's overload list (`s->ol`). Generic instantiations created inside blocks may not be getting added to the global code generation queue properly.

**From main() line 350:**
```c
for(uint32_t i=0;i<sm.eo;i++)
  for(uint32_t j=0;j<4096;j++)
    for(Sy*s=sm.sy[j];s;s=s->nx)
      if(s->el==i&&s->lv==0)  // Only top-level procedures
        for(uint32_t k=0;k<s->ol.n;k++)
          gdl(&g,s->ol.d[k]);  // Generate code for each overload
```

**Next Steps:**
- Ensure generic instantiations inside blocks are added to symbol overload list
- OR add special handling to emit code for all procedures with lv==0 regardless of context

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
