# ACATS Test Suite Analysis

## Test Categories

| Category | Count | Description | Status |
|----------|-------|-------------|--------|
| A | 144 | Acceptance tests | 1 PASS, rest blocked by report.ll |
| B | 1515 | Illegality tests (should reject) | ~40% PASS, many WRONG_ACCEPT |
| C | 2119 | Executable tests | All blocked by report.ll |
| D | 50 | Numerics tests | Not analyzed |
| E | 54 | Inspection tests | Not analyzed |
| L | 168 | Post-compilation tests | Not analyzed |

## Critical Issues Blocking Tests

### 1. Report Package Codegen Bug (BLOCKS A & C tests)

**File:** `acats/report.adb`  
**Issue:** Duplicate variable allocation in LLVM IR

```llvm
%v.desc.sc2.20 = alloca ptr    ; First allocation (parameter)
store ptr %p.desc, ptr %v.desc.sc2.20
...
%v.desc.sc2.20 = alloca ptr    ; DUPLICATE allocation (frame slot)
```

**Root Cause:** Frame allocation loop allocates slots for ALL local symbols including parameters that were already allocated

**Impact:** Cannot link report.ll with test files → 2200+ tests blocked

### 2. B-Test Failures (Illegality Detection)

**WRONG_ACCEPT failures** (compiler accepts invalid code):

- **b24007a**: Real literals must have decimal points
  - `FLOAT1 := 14;` should be rejected (14 not 14.0)
  - Missing type checking for literal types
  
- **b24104a**: Integer literals cannot have negative exponents  
  - `10E-1` should be rejected (results in 1.0, not an integer)
  - Lexer accepts invalid exponent syntax

- **b24204e**: Based literal digits must be valid for base
  - `7#7.6#` should be rejected (digit 7 invalid in base 7)
  - No digit range validation

**LOW_COVERAGE failures** (some but not all errors detected):
- Many tests have <50% error detection rate
- Missing semantic checks for various language rules

### 3. Parser Limitations

**Compilation failures:**

- `a28002c.ada:33`: Pragma with expression `PRAGMA OPTIMIZE (A > B);`
  - Parser expects simple pragma syntax
  
- `a28004a.ada:18`: Generic parameter issue
  - Expected `;` got `(`
  
- `c2a021b.ada:11`: Based literals with `%` delimiters
  - `%%%%%345%` not recognized (Ada83 alternate syntax)

- `c32107c.ada:62`: Discriminated generic formal types
  - `TYPE PRIV (D : T) IS PRIVATE;` not parsed

## Recommended Priorities

### High Priority (Unblocks 2200+ tests)
1. Fix duplicate frame allocation bug in gdl() N_PB case
2. Skip parameters when allocating frame slots

### Medium Priority (Improves illegality detection)
3. Add real literal type checking (decimal point required)
4. Validate integer literal exponents (must be non-negative)
5. Validate based literal digits against base

### Low Priority (Nice to have)
6. Support `%` syntax for based literals
7. Support discriminated generic formal types
8. Improve pragma parsing for complex expressions

## Code Locations

- **Frame allocation:** ada83.c:176 (gdl function, N_PB case)
- **Literal parsing:** Lexer/parser in ada83.c
- **Type checking:** Rex/rst functions in ada83.c
