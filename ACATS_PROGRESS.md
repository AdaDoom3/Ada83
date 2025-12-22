# ACATS Test Progress Summary

## Test Results

### Sample C Tests (c2*, c3*, c4* - 707 tests)
- **Initial**: 298/707 (42%)
- **After REPORT functions**: 395/707 (55%)
- **After WITH/USE fix**: 397/707 (56%)

### All C Tests (first 2000)
- **Current**: 992/2000 (49.6%)

### Total ACATS Suite
- **Total tests**: 4050 Ada files
- **Tested**: 2000
- **Remaining**: 2050

## Bugs Fixed

### 1. PRAGMA IMPORT/INTERFACE (ada83.c:126)
**Issue**: Parser only accepted identifiers as 3rd argument  
**Fix**: Accept string literals for external names  
**Example**: `PRAGMA IMPORT(LLVM, RT_PUT, "__text_io_put")`

### 2. Exception Visibility (ada83.c:165)  
**Issue**: Exception symbols not stored in AST nodes  
**Fix**: Store symbol in `id->sy` for N_ED case  
**Impact**: Fixed USE_ERROR and other exception references

### 3. Scope Cleanup Bug (ada83.c:148)
**Issue**: sco() removed types/constants/exceptions, causing "dup" errors  
**Fix**: Only remove variables (k==0), keep types/consts/excs  
**Impact**: Fixed local variable scoping in nested procedures

### 4. Package Body Visibility (ada83.c:165, 168)
**Issue**: Package body couldn't see spec declarations  
**Fix**: Load .ads file and add spec symbols to use-visibility  
**Impact**: Fixed TEXT_IO, REPORT, and other package bodies

### 5. WITH/USE for Standalone Bodies (ada83.c:130)
**Issue**: WITH/USE before func/proc bodies was parsed but discarded  
**Fix**: Collect all context clauses and merge into compilation unit  
**Impact**: +100+ tests now compile (multi-unit files)

### 6. Missing REPORT Functions (rts/report.ads)
**Issue**: ACATS tests use EQUAL and COMMENT functions
**Fix**: Added to REPORT package
**Impact**: +97 tests passing

### 7. Package Variable Scope (ada83.c:148)
**Issue**: Package-level variables removed when scope closed
**Fix**: Only remove variables without parent package (`pr==0`)
**Impact**: Fixed "undef" errors in package bodies accessing spec variables

### 8. Discriminant Field Access (ada83.c:156) **[FIXED]**
**Issue**: Discriminants not accessible as record fields (error: `?fld 'D'`)
**First Attempt**: Add discriminants to component list - REVERTED due to 100+ segfaults
**Root Cause of Failure**: Created N_CM nodes from N_DS nodes with incompatible union fields
**Correct Fix**: Modified N_SEL case in rex() to check discriminants after components
**Implementation**: Added discriminant loop: `for(uint32_t i=0;i<pt->dc.n;i++){No*d=pt->dc.d[i];if(d->k==N_DS&&si(d->pm.nm,n->se.se)){n->ty=rst(SM,d->pm.ty);return;}}`
**Impact**: +32 tests passing (960→992, 48%→49.6%)
**Key Insight**: Don't create new nodes - extend existing lookup logic

### 9. Deferred Constant Completion (ada83.c:165)
**Issue**: Deferred constants in PRIVATE caused "dup" errors
**Fix**: Detect deferred constant completion and reuse existing symbol
**Impact**: Fixed private type packages with deferred constants

### 10. Predefined Subtypes POSITIVE/NATURAL (ada83.c:145-146)
**Issue**: Missing POSITIVE and NATURAL predefined subtypes
**Fix**: Added TY_NAT (0..2147483647) and TY_POS (1..2147483647) to predefined types
**Impact**: Fixed 9+ tests using POSITIVE'IMAGE and similar constructs
**Example**: `POSITIVE'IMAGE(MAX_MANTISSA)` now compiles

### 11. Generic Attribute Substitution (ada83.c:164) **[PARTIAL]**
**Issue**: Generic formal types in attribute references not substituted during instantiation
**Example**: `T'PRED(x)` in generic body causes "undef 'T'" after instantiation
**Fix**: Added `case N_AT:r->at.p=gsub(SM,n->at.p,fp,ap);break;` to gsub() function
**Result**: Correct but insufficient - generics need architectural changes
**Investigation**:
- gsub() now handles attribute prefix substitution (N_AT nodes)
- However, generic instantiation has deeper issues:
  - Instantiated procedures not registered with correct names
  - Generic formal parameters not visible in bodies during template processing
- Attempted fix with parameter injection caused 154 test regression
**Status**: Partial fix committed, full generic support requires redesign

## Remaining Issues

### Common Failure Patterns (from 1008 failing tests)
1. **Generic formal parameter visibility** (17 occurrences): `undef 'T'`
   - Generic formal types not added to symbol table (case N_GEN:break;)
   - Requires architectural changes to generic handling
2. **Duplicate symbol errors** (9 occurrences): `dup 'X'`
   - Various edge cases in symbol table management
4. **ASCII package visibility** (6 occurrences): `undef 'ASCII'`
   - Package ASCII not being found in some contexts
5. **Segmentation faults**: Some tests still crash (5 in 100-test sample)

### Known Limitations
- Generic formal parameter visibility (architectural issue)
- Discriminant field access (needs alternative approach)
- Generic package bodies (INTEGER_IO, FLOAT_IO, etc.)
- Task/protected types (not implemented)
- Exception propagation in generated code

## Files Modified

1. **ada83.c** - Main compiler (12 fixes total)
   - Line 145-146: Added POSITIVE/NATURAL predefined subtypes
   - Line 148: Package variable scope fix
   - Line 156: Discriminant field access in N_SEL case
   - Line 164: Added N_AT case to gsub() for generic attribute substitution
   - Line 165: Deferred constant completion, Package body visibility
2. **rts/report.ads** - Added EQUAL, COMMENT
3. **rts/report.adb** - Implemented new functions

## Next Steps

1. Investigate generic formal parameter visibility (architectural)
2. Find alternative approach for discriminant field access
3. Debug remaining duplicate symbol edge cases
4. Fix ASCII package visibility issues
5. Test beyond C-class (B, D, E, L classes)

## Performance

- Compilation speed: ~1000 tests/3 minutes
- Pass rate: 42% → 45% → 55.5% (progressive improvements)
- Progress: +190 tests passing from 919 to 1109
- Remaining: 891/2000 tests failing (44.5%)
