# ACATS Test Suite Analysis

## Test Results Summary

### B-Tests (Illegality Detection) - Group B2
- **Pass Rate**: 38% (38/100)
- **Failures**: 62 tests
  - WRONG_ACCEPT: Compiler accepts illegal code
  - LOW_COVERAGE: Catches only some of the required errors

### C-Tests (Executable) - Group C3
- **Pass Rate**: 0% (0/349)
- **Primary Issues**:
  - Symbol resolution failures ("undef" errors)
  - Package/module loading issues
  - Missing BIND capabilities

## Critical Bugs Identified

### 1. Pragma-Only Statement Sequences (WRONG_ACCEPT)
**Tests**: b28001f, b28001g, b28001h, etc.

**Issue**: Compiler accepts statements sequences containing ONLY pragmas, when Ada requires at least one executable statement.

**Example**:
```ada
IF CHOICE THEN
    PRAGMA LIST (ON);      -- ERROR: This is not a valid statement sequence
END IF;
```

**Fix Needed**: Add validation in `rss()` to ensure statement sequences have at least one non-pragma statement.

### 2. Reserved Word Usage (LOW_COVERAGE)
**Test**: b29001a (1/63 errors = 1%)

**Issue**: Compiler stops on first error instead of reporting all 63 uses of reserved words as type names.

**Fix Needed**: Continue parsing after errors to report multiple issues.

### 3. Symbol Resolution Failures
**Many C-tests**: "undef 'PROC'", "undef 'EQUAL'", etc.

**Issue**: Compiler can't find symbols that should be available through WITH/USE clauses or package visibility.

**Possible Causes**:
- Package loading not working correctly
- Symbol table scope issues
- Missing symbol inheritance from parent packages

### 4. Access Type Validation
**Test**: c39202a ".all non-ac"

**Issue**: Dereference operation applied to non-access type not properly rejected.

## Recommended Fix Priority

1. **HIGH**: Pragma-only sequence validation (easy fix, multiple test impacts)
2. **HIGH**: Continue-on-error for better coverage reporting
3. **MEDIUM**: Symbol resolution debugging (complex, affects many C-tests)
4. **MEDIUM**: Access type semantic checks
5. **LOW**: Better error messages

## Implementation Notes

### Pragma-Only Fix Location
File: `ada83.c`, Function: `rss()` (line 157)

Add helper:
```c
static bool has_real_stmt(NV*stmts){
    for(uint32_t i=0;i<stmts->n;i++)
        if(stmts->d[i]->k!=N_PG) return true;
    return false;
}
```

Add validation in IF case:
```c
case N_IF:
    rex(SM,n->if_.cd,TY_BOOL);
    if(!has_real_stmt(&n->if_.th))
        die(n->l,"stmt seq needs real stmt, not just pragma");
    // ... rest of processing
```

### Error Continuation
Current: `die()` calls `exit(1)` immediately
Needed: Track errors, continue parsing, exit at end if errors > 0

## Build Command
```bash
cc -o ada83 ada83.c -lm
```

## Test Commands
```bash
# Run specific test groups
./test.sh q b2      # B-tests group 2
./test.sh q c3      # C-tests group 3

# Run all B-tests (takes ~5 minutes)
./test.sh b

# Run specific test
./ada83 acats/b28001f.ada 2>&1
```
