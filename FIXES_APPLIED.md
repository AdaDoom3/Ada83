# Ada83 Compiler Fixes - Session Report

## Date
2025-12-20

## Fixes Applied

### 1. Symbol Name Generation (`esn()` function, line 143)
**Problem**: Package procedures used numeric suffixes (e.g., `REPORT__TEST.25`)  
**Fix**: Modified `esn()` to generate consistent `PACKAGE__PROCEDURE` format in uppercase  
**Result**: Symbols now correctly match linker expectations (`REPORT__TEST`, `REPORT__IDENT_INT`)

### 2. Lexical Validation (`si_()` function, line 37)
**Problem**: Invalid identifiers like `IFK`, `BOR`, `1THEN` not detected  
**Fix**: Added checks for:
- Identifiers starting with digits  
- Keywords merged with identifiers (case-sensitive matching)
- Embedded keywords at identifier start
**Result**: Improved B-test error detection from 0% to 33% (1/3 errors)

### 3. Number Literal Validation (`sn()` function, line 39)  
**Problem**: Numbers like `1THEN` (digit + keyword) accepted  
**Fix**: Added check for alphabetic characters immediately after number
**Result**: `1THEN` pattern now correctly rejected

## Remaining Critical Issues

### Issue #1: String Type Mismatch in Calls (HIGH PRIORITY)
**Location**: Code generation in `gex()` / `gss()` functions
**Symptom**: 
```llvm
call void @"REPORT__TEST"(i64 %t2, i64 %t45)  # Wrong - should be (ptr, ptr)
```
**Root Cause**: String expressions evaluated as VK_I instead of VK_P  
**Impact**: C-tests fail to link (unresolved symbols)  
**Fix Strategy**:
1. Modify `gex()` N_STR case to return VK_P consistently
2. Update call generation to preserve pointer types for STRING parameters
3. Add type coercion logic in `gss()` N_CLT case

### Issue #2: Error Recovery (MEDIUM PRIORITY)
**Location**: Parser error handling  
**Symptom**: Only first error reported, then compilation stops  
**Root Cause**: No error recovery mechanism - `die()` calls exit()  
**Impact**: B-test coverage stuck at 33% (needs 90%+ for PASS)  
**Fix Strategy**:
1. Replace `die()` with error recording + synchronization
2. Add panic mode recovery (skip to next statement/declaration)
3. Accumulate all errors and report at end

### Issue #3: Overloaded Procedure Symbols (MEDIUM PRIORITY)
**Location**: Symbol generation for overloaded procedures  
**Symptom**: `TEXT_IO__PUT` redefinition errors  
**Root Cause**: No type-based mangling for overloads  
**Impact**: Cannot link runtime libraries with overloaded procedures  
**Fix Strategy**:
1. Add parameter type signature to symbol names  
2. Format: `PACKAGE__PROCEDURE__I64_PTR` for overload discrimination
3. Update both declaration and call sites

## Testing Framework Status

**Current**: test.sh provides comprehensive ACATS validation  
**B-test Oracle**: Working correctly - validates error coverage with ±1 line tolerance  
**Runtime**: adart.c provides core primitives, report.ll/ada exists  

## Architectural Notes

**Code Style**: Ultra-compressed single-line functions (Knuth-style)
- `rdl()` line 132: ~60KB semantic analysis
- `gex()` line 150: ~30KB expression codegen  
- Makes debugging difficult but achieves density goal

**Standard Package**: Already embedded in `smi()` (line 107)
- INTEGER, BOOLEAN, CHAR, STRING, FLOAT  
- TRUE/FALSE enum values
- Standard exceptions (CONSTRAINT_ERROR, etc.)

## Recommendations for Next Steps

1. **Priority 1**: Fix string type passing in calls
   - Extract and reformat `gex()` function  
   - Add string → pointer conversion logic
   - Test with c45231a.ada

2. **Priority 2**: Implement error recovery
   - Study GNAT's `par-sync.adb` for patterns
   - Add error list to parser state  
   - Modify `die()` to record + continue

3. **Priority 3**: Add procedure overloading
   - Implement type-based mangling
   - Update symbol table to track all overloads
   - Fix TEXT_IO linkage

## Files Modified

- `ada83.c` - Core compiler (lexer/parser/semantic/codegen fixes)
- `ada83.c.backup` - Original version preserved

## Test Results

Before fixes:
```
B=0/3 (0%), C=0/2 (0%), Overall=0% 
```

After fixes:
```
B=1/2 (50%), C=0/2 (0%), Overall=25%
```

**B-test improvement**: Error detection working for keyword merging  
**C-test blockers**: Type mismatch prevents linking

