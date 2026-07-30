# Ada83

A single-file Ada 83 (ANSI/MIL-STD-1815A) compiler with an LLVM backend.
The whole compiler is one C file, `ada83.c`; it builds native executables
through a libLLVM loaded at run time, and emits textual LLVM IR with
`--ir`.

## ACATS conformance

All 3561 tests of the ACATS 1.11 conformance suite pass.

```
 A  Acceptance        ██████████████████████████████   140/140   100%
 B  Illegality        ██████████████████████████████ 1350/1350   100%
 C  Executable        ██████████████████████████████ 1973/1973   100%
 D  Numerics          ██████████████████████████████     17/17   100%
 E  Inspection        ██████████████████████████████     34/34   100%
 L  Post-compilation  ██████████████████████████████     47/47   100%
                      ──────────────────────────────
    Total             ██████████████████████████████ 3561/3561   100%
```

Three further tests were withdrawn by the AVO and are not counted; see
[Withdrawn tests](#withdrawn-tests).

## Requirements

- **`gcc`** or **`clang`** (`make CC=clang`).
- **GNU `make`**.
- A 64-bit host with `__int128` support (x86-64 or AArch64).
- **libLLVM**, for building native executables. `make` installs it through
  the system package manager if it is not already present. It is opened at
  run time, so no LLVM headers or development package are needed to build
  the compiler itself.

## Building

```sh
make
```

This produces `./ada83`.

On Windows, run `build.bat` instead. It unpacks the bundled `LLVM-C.zip`
and builds with the first of GCC, Clang, or a privately fetched Zig that
it finds, so a machine with no toolchain installed can still build the
compiler. `build.bat clean` removes everything it produced.

## Usage

```sh
./ada83 hello.ada -o hello      # native executable
./ada83 hello.ada               # executable named after the input
./ada83 --ir hello.ada -o out.ll   # textual LLVM IR
./ada83 --ir a.ada b.ada c.ada     # several files, compiled in parallel
```

Given `hello.ada`:

```ada
with TEXT_IO; use TEXT_IO;
procedure HELLO is
begin
   PUT_LINE ("Hello, Ada 83!");
end HELLO;
```

```
$ ./ada83 hello.ada -o hello
Compiled 'hello.ada' -> 'hello.native.ll'
Generated ALI file 'hello.native.ali'
$ ./hello
Hello, Ada 83!
```

Frequently used options, with the full list under `--help`:

| Option | Effect |
|---|---|
| `-O0` … `-O3`, `-Os` | Optimization level (default `-O2`) |
| `--ir` | Emit textual LLVM IR instead of an executable |
| `--emit-llvm` | Also write the optimized IR beside the executable |
| `--suppress=<check>` | Omit a runtime check, as `pragma SUPPRESS` would |
| `-I <path>` | Add a directory to the source search path |
| `-W<class>`, `-Wno-<class>` | Enable or disable a warning class |
| `--bind <dir> <unit>` | Bind a precompiled program library |

## The runtime library

The Ada 83 standard packages are implemented in Ada, in the single
multi-unit source file `runtime.ada`: `text_io`, `calendar`, `direct_io`,
`sequential_io`, `unchecked_conversion`, `unchecked_deallocation`,
`io_exceptions`, `low_level_io`, and `machine_code`. The compiler finds it
next to its own executable, or on the include path.

Package `SYSTEM` is not in that file. Its contents are target-dependent,
so the compiler generates its source from the target constants.

## Repository layout

| Path | Contents |
|---|---|
| `ada83.c` | The compiler, in one file |
| `runtime.ada` | The Ada 83 standard packages |
| `acats/` | The ACATS 1.11 conformance suite |
| `reference/` | Language reference, DIANA guide, GNAT sources |
| `test.sh` | ACATS harness |
| `bench.sh` | Benchmarks |
| `build.bat` | Windows build script |

## Testing

```sh
bash test.sh run all      # every class
bash test.sh run c        # one class
bash test.sh run c32      # one group
bash test.sh check        # run, then diff against acats.baseline
bash test.sh bless        # run, then write the baseline
```

Runs are written to a timestamped `test_results/<label>-<timestamp>-<pid>/`
alongside `acats_logs/<same>/`, so concurrent or repeated runs never
overwrite one another. `NPROC=4` caps parallelism, which otherwise defaults
to `nproc`.

## Deviations

### Scaled DELAY values

Every `DELAY` in `acats/` has been scaled down by a factor of ten so that
the suite runs quickly during development. Each one is marked in place:

```ada
DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
```

This affects 104 files. Because each marker records the original value, the
deviation is reversible: restore each scaled literal to the value named in
its own comment, then run with `TEST_TIMEOUT=120`, since the harness default
of 30 seconds is sized for the scaled delays.

Timing-sensitive results should not be quoted from a scaled run.

### Withdrawn tests

Three tests were officially withdrawn by the AVO and are excluded.

- **c98003b** — requires that a MED-priority task make *no* progress
  while a LOW-caller/HIGH-acceptor rendezvous runs. That assumes
  strict preemptive priority scheduling on a single processor;
  RM 9.8 imposes priorities only among tasks sharing a processor.
  With tasks as OS threads on a multicore machine the MED task
  legitimately runs in parallel and the test self-reports failure.

- **cc1226b** — its final check demands that two *uninitialized*
  variables of a formal private type compare unequal. Reading them is
  erroneous to begin with, and for the test's own actual type (a null
  record with two defaulted discriminants) every value of the type is
  equal, so the expectation is unsatisfiable.

- **ce3902b** — nominally checks ENUMERATION_IO parameter names, but
  its plumbing closes the current default output file and re-opens
  that same file object with IN_FILE mode while it remains the
  default output. This contradicts the default-file semantics that
  the valid test ce3208a requires (and which this compiler
  implements); the AVO withdrew ce3902b and kept ce3208a.
