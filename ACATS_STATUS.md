# ACATS A-Series Test Status

## Summary

Current pass rate: **68% (98/144 tests)**

### Results Breakdown
- **PASS**: 98 tests (68%)
- **FAIL**: 18 tests (13%) - Runtime crashes
- **SKIP**: 28 tests (19%) - Compilation/binding issues

## Major Fix Applied

### Qualified Procedure Call Support (N_SEL handling)

**Problem**: Calls like `REPORT.TEST("A", "B")` were not being generated at all.

**Root Cause**: The `gss()` function's N_CLT case only handled simple identifiers (N_ID), 
not qualified names (N_SEL) like PACKAGE.PROCEDURE.

**Solution**: Modified line 250 in ada83.c:
```c
// Before:
case N_CLT:{if(n->ct.nm->k==N_ID){

// After:  
case N_CLT:{S nm=n->ct.nm->k==N_SEL?n->ct.nm->se.se:n->ct.nm->s;
           if(n->ct.nm->k==N_ID||n->ct.nm->k==N_SEL){
```

This enabled the compiler to extract the procedure name from either simple or qualified calls.

**Impact**: Test pass rate increased from 0% to 68%!

## Known Issues

### 1. Array/String Assignment Crashes (13 tests)
Tests: a21001a, a22002a, a27003a, etc.

**Symptom**: Segmentation fault (exit 139) during execution

**Cause**: Type mismatch in variable allocation:
- String variables `S : STRING(1..N)` allocated as `[N x i64]`
- But assignment tries to store `ptr` (fat pointer descriptor)
- Should be either `[N x i8]` or `{ptr,ptr}` throughout

**Example**:
```llvm
%v.s.sc1.110 = alloca [3 x i64]    ; WRONG - should be [3 x i8] or {ptr,ptr}
store ptr %t9, ptr %v.s.sc1.110     ; Type mismatch!
```

### 2. Compilation Errors (28 tests)
- Type mismatches (typ mis)
- Undefined symbols (undef 'SYSTEM')
- Parse errors
- Missing generic support

### 3. Timeout (1 test)
- a55b14a: exit 124 (timeout)

## Successfully Passing Test Categories

- Basic procedure calls with qualified names ✓
- Simple arithmetic and logic ✓  
- Control flow (if/case/loop) ✓
- Record types (basic) ✓
- Nested procedures ✓
- Generic instantiation (some) ✓

## Implementation Notes

Per user directive: **"don't revert shit - just move forward"**

This fix represents substantial progress toward ACATS compliance. The N_SEL handling 
was a critical missing feature that blocked most A-series tests from running at all.

## Next Steps for Full A-Series Compliance

1. Fix array/string variable allocation types
2. Implement SYSTEM package
3. Add missing generic features
4. Fix type constraint checking
5. Resolve parse errors in complex constructs

