# LLVM IR Output Quality Analysis

## Test Results

### Test 1: Simple Arithmetic (ll_test1_simple.ada)
- **Total lines**: 181
- **Runtime declarations**: ~150 lines (83%)
- **Actual code**: ~10 lines (6%)
- **Unused runtime functions**: ~20 lines (11%)

**Issues**:
1. Declares ALL runtime functions (setjmp, pthread_*, malloc, etc.) even though none are used
2. Defines unused text_io functions as linkonce_odr
3. Only 6% of output is actual user code

### Test 2: Functions (ll_test2_functions.ada)
- **Total lines**: 196
- **Runtime declarations**: ~150 lines (77%)
- **Actual code**: ~30 lines (15%)
- **Unused runtime functions**: ~15 lines (8%)

**Issues**:
1. Same runtime bloat as Test 1
2. Only 15 lines more than Test 1 despite adding a function
3. Actual code generation is clean and efficient

### Test 3: Package (ll_test3_package.ada)
- **Total lines**: 206
- **Runtime declarations**: ~150 lines (73%)
- **Actual code**: ~50 lines (24%)
- **Unused runtime functions**: ~6 lines (3%)

**Issues**:
1. Same runtime bloat
2. Good: Package functions don't get duplicate declarations
3. Bad: Still emitting unused text_io helpers

## Redundancy Classification

### Category 1: ALWAYS EMITTED (High Priority)
These are emitted for EVERY compilation, regardless of use:
- setjmp/longjmp (exception handling runtime)
- pthread_* functions (concurrency runtime)
- malloc/realloc/free (memory management)
- printf/puts/sprintf (I/O runtime)
- strcmp/strcpy/strlen/memcpy/memset (string ops)
- pow/sqrt/sin/cos/exp/log (math ops)
- llvm.memcpy intrinsic

**Impact**: ~50 declarations per file

### Category 2: FREQUENTLY UNUSED (Medium Priority)
Runtime helper functions defined as linkonce_odr even when unused:
- __ada_ss_* (secondary stack functions)
- __ada_setjmp/__ada_raise (exception handling)
- __ada_delay/__ada_powi (utility functions)
- __ada_image_*/__ada_value_* (conversion functions)
- __ada_finalize* (cleanup functions)
- __text_io_* (I/O helpers)

**Impact**: ~100 lines of function definitions

### Category 3: EXCEPTION GLOBALS (Low Priority)
Exception name constants always emitted:
- @.ex.CONSTRAINT_ERROR
- @.ex.PROGRAM_ERROR
- etc. (12 total)

**Impact**: ~12 declarations

## Recommendations

1. **Conditional Declaration Emission** (High Impact)
   - Track which runtime functions are actually called
   - Only emit declarations for used functions
   - Could reduce output by 70-80% for simple programs

2. **Lazy Runtime Loading** (Medium Impact)
   - Move runtime helpers to a separate .ll file
   - Link only used functions
   - Requires build system changes

3. **Dead Code Elimination** (Medium Impact)
   - Run `opt -O2` on output to remove unused linkonce_odr functions
   - Relies on LLVM optimizer
   - Doesn't reduce source .ll size but improves final binary

4. **Minimal Runtime Mode** (Low Impact)
   - Add compiler flag for minimal runtime
   - Only emit core declarations (malloc, free, basic ops)
   - Good for embedded/size-critical applications
