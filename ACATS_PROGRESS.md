# ACATS Test Progress Summary

## Test Results

### Sample C Tests (c2*, c3*, c4* - 707 tests)
- **Initial**: 298/707 (42%)
- **After REPORT functions**: 395/707 (55%)
- **After WITH/USE fix**: 397/707 (56%)

### All C Tests (first 2000)
- **Current**: 919/2000 (45%)

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

## Remaining Issues

### Common Failure Patterns (from 2000-test run)
1. **Segmentation faults**: ~6 crashes during test run
2. **dup 'C1'** (4 tests): Deferred constant handling in PRIVATE sections
3. **undef errors** (8 tests): Various symbol visibility issues
4. **?fld errors** (3 tests): Record field access problems
5. **Generic packages**: TEXT_IO generics not yet working

### Known Limitations
- Generic package bodies (INTEGER_IO, FLOAT_IO, etc.)
- Some discriminant handling
- Deferred constants in PRIVATE parts
- Some attribute handling ('MEMORY_SIZE, etc.)

## Files Modified

1. **ada83.c** - Main compiler (6 fixes)
2. **rts/report.ads** - Added EQUAL, COMMENT
3. **rts/report.adb** - Implemented new functions

## Next Steps

1. Fix segmentation faults (likely parser edge cases)
2. Implement deferred constant handling
3. Improve generic package support
4. Add remaining ACATS support functions
5. Test beyond C-class (B, D, E, L classes)

## Performance

- Compilation speed: ~1000 tests/3 minutes
- Pass rate improving: 42% → 45% (c-class only)
- Still 55% of tests failing, mostly advanced features
