# Ada83 Runtime System and Test Framework

## Runtime System (rts/)

The `rts/` directory contains the Ada83 runtime library packages required for ACATS conformance testing.

### Core Components

**ACATS Test Harness:**
- `report.ads/adb` - ACATS test framework providing:
  - `TEST(name, desc)` - Initialize test case
  - `FAILED(msg)` - Report test failure
  - `RESULT` - Output final PASSED/FAILED status
  - `COMMENT(msg)` - Output informational message
  - `IDENT_*` - Identity functions to prevent constant folding
  - `EQUAL` - Comparison function

**Standard Packages:**
- `system.ads` - System-dependent constants and types
- `calendar.ads/adb` - Date/time operations
- `text_io.ads/adb` - Text input/output (minimal implementation)
- `ascii.ads` - ASCII character constants
- `io_exceptions.ads` - I/O exception definitions

**Generic I/O:**
- `sequential_io.ads/adb` - Sequential file I/O
- `direct_io.ads/adb` - Direct (random access) file I/O
- `low_level_io.ads/adb` - Low-level I/O primitives

**Generic Utilities:**
- `unchecked_conversion.ads` - Type conversions without checking
- `unchecked_deallocation.ads` - Explicit deallocation

### Runtime Compilation

The `test.sh` script automatically compiles runtime packages on demand:

```bash
R(){
  [[ ! -f rts/report.ll || rts/report.adb -nt rts/report.ll ]] &&
    ./ada83 rts/report.adb>rts/report.ll 2>/dev/null||true
}
```

- Runtime packages are compiled to LLVM IR (.ll files)
- Compilation is lazy - only when needed or when source is newer
- report.ll is pre-compiled and linked with every A/C/D test

## Test Framework (test.sh)

### Test Categories

**A-Class (Acceptance):**
- Must compile and run successfully
- Must output "PASSED" to pass
- Currently: 100/100 passing (69% of A-series total)

**B-Class (Illegality):**
- Must be rejected by compiler
- Error coverage must be ≥90% of expected errors
- Uses `ERROR` comments in source to mark expected errors
- Currently: 0/118 passing (0%)

**C-Class (Executable):**
- Must compile, link, and execute
- Must output "PASSED" or "NOT APPLICABLE"
- Tests runtime semantics
- Currently: 0/0 (26 skipped)

**D-Class (Numerics):**
- Tests exact arithmetic
- Must output "PASSED"

**E-Class (Inspection):**
- May output "TENTATIVELY PASSED" requiring manual verification

**L-Class (Post-compilation):**
- Must fail at compile, link, or runtime as appropriate

### Test Execution Flow

```
1. R() - Compile runtime (report.ll) if needed
2. ./ada83 test.ada > test.ll - Compile test to LLVM IR
3. llvm-link test.ll rts/report.ll - Link with runtime
4. lli test.bc - Execute via LLVM interpreter
5. Parse output for PASSED/FAILED/NOT APPLICABLE
```

### Key Files Generated

**Per Test:**
- `test_results/TEST.ll` - LLVM IR for test
- `test_results/TEST.bc` - Linked LLVM bytecode
- `acats_logs/TEST.err` - Compiler errors
- `acats_logs/TEST.out` - Runtime output
- `acats_logs/TEST.link` - Link errors

**Summary:**
- `test_summary.txt` - Overall test results

### Runtime Linkage

The runtime system uses pragma IMPORT(C,...) to link with C runtime functions:

```ada
PROCEDURE P(C:INTEGER);
PRAGMA IMPORT(C,P,"__text_io_put_char");
```

These are implemented in the compiler's built-in runtime or expected to be provided by the C standard library.

### Current Status

**ACATS A-Series:** 100/144 tests passing (69%)
- 100 passing
- 44 failing (mostly due to unimplemented features or semantic bugs)
- Major blocker: Fat pointer handling for constrained strings

**Runtime Coverage:**
- report.adb: Fully functional
- text_io: Basic output only (put_char, new_line)
- Other packages: Compiled but minimally tested
