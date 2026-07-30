# Ada83

An Ada 83 (ANSI/MIL-STD-1815A) compiler in a single C file, with an LLVM
backend.

`ada83.c` holds the whole compiler: lexer, parser, semantics, expander, and
LLVM IR emitter. It links against nothing but libm and libpthread. libLLVM is
loaded at run time and used to optimise, generate code, and produce native
executables, so building the compiler needs no LLVM headers and no LLVM
development package.

## Conformance

ACATS 1.11 is the Ada Validation Organization's acceptance suite for Ada 83.
All 3561 of its tests pass.

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

Class B is the largest: 1350 programs that must be rejected, each with the
diagnostic on the line the suite marks. Accepting too much fails class B as
surely as accepting too little.

Three tests were withdrawn by the AVO and are not counted. The reasons are
given at the end.

## Requirements

- GCC or Clang, on a 64-bit host with `__int128`.
- GNU make.
- libLLVM, for producing native executables. `make` installs it through the
  system package manager, or through Homebrew on macOS.

## Building

```sh
make
```

Linux and macOS build from this makefile, on x86-64 and on Apple silicon.

On Windows, run `build.bat`. It unpacks the bundled LLVM and builds with GCC,
Clang, or Zig, whichever it finds first. If no C compiler is installed it
downloads Zig into the build directory and uses that, so nothing has to be
installed beforehand.

## Use

The default output is a native executable, produced through LLVM:

```sh
./ada83 hello.ada -o hello
```

```ada
with Text_IO; use Text_IO;
procedure Hello is
begin
   Put_Line ("Hello, Ada 83!");
end;
```

```
$ ./ada83 hello.ada -o hello
Compiled 'hello.ada' -> 'hello.native.ll'
Generated ALI file 'hello.native.ali'
$ ./hello
Hello, Ada 83!
```

The compiler emits LLVM IR, loads libLLVM, runs the pass pipeline, emits an
object file, and calls the system linker. The IR can be taken directly:

```sh
./ada83 --ir hello.ada -o hello.ll      # textual LLVM IR
./ada83 --emit-llvm hello.ada -o hello  # native, keeping the optimised IR
./ada83 --ir a.ada b.ada c.ada          # several units, one process each
lli hello.ll                            # interpret the IR
```

| Option | Effect |
|---|---|
| `-O0` … `-O3`, `-Os` | Optimisation level, passed to the LLVM pass pipeline (default `-O2`) |
| `--ir` | Stop at textual LLVM IR |
| `--emit-llvm` | Build natively and also write the optimised IR |
| `--suppress=CHECK` | Omit a runtime check, as `pragma SUPPRESS` would |
| `-I DIR` | Add a directory to the source search path |
| `-W CLASS`, `-Wno-CLASS` | Enable or disable a warning class |
| `--bind DIR UNIT` | Bind a precompiled program library |

The rest are under `--help`.

## Runtime library

The standard packages are written in Ada and kept in one multi-unit file,
`runtime.ada`: `TEXT_IO`, `CALENDAR`, `DIRECT_IO`, `SEQUENTIAL_IO`,
`UNCHECKED_CONVERSION`, `UNCHECKED_DEALLOCATION`, `IO_EXCEPTIONS`,
`LOW_LEVEL_IO`, `MACHINE_CODE`. The compiler looks for it beside its own
executable, then on the include path.

`SYSTEM` is not in that file. Its contents are target-dependent, so the
compiler generates its source from the target constants it was built with.

## Tests

The suite ships as `tests.zip` and is unpacked by the harness on first use.

```sh
bash test.sh run all      # every class
bash test.sh run c        # one class
bash test.sh run c45      # one group
bash test.sh check        # run, then diff against the baseline
bash test.sh help
```

Each run writes to its own timestamped directory under `test_results/`, with
logs under `acats_logs/`, so concurrent runs do not overwrite one another.

The harness needs `lli` and `llvm-link` on the path and Bash 4 or newer.
macOS ships Bash 3.2 and no `timeout`, so running the suite there needs
`brew install bash coreutils llvm`. Building the compiler needs none of that.

## Benchmarks

`bench.sh` measures compile time, generated-code speed at each optimisation
level, and throughput over the conformance corpus. Where GNAT is present the
same programs are built with it and the two are reported side by side; if it
is absent, it is installed.

```sh
bash bench.sh            # everything
bash bench.sh codegen    # run time of the generated code
bash bench.sh compile    # time taken to compile
bash bench.sh corpus     # throughput over the suite
bash bench.sh help
```

Every figure is the median of several timed runs taken after a warm-up, and
is printed with the relative standard deviation of its samples, so a number
that moved under measurement is visible as such rather than quoted as fact.
The output of each program is compared between the two compilers, and any
disagreement is reported alongside the timings.

## Deviations

### Scaled delays

Every `DELAY` in the suite is divided by ten so the tests run quickly, and
each is marked where it sits:

```ada
DELAY 0.1 ;  -- TODO: acats-delay-deviation: before was 1.0
```

That covers 104 files. Each marker records the original value, so the change
is reversible: restore each literal to the value in its own comment and run
with `TEST_TIMEOUT=120`, the default cap being sized for the scaled delays.
Timing-sensitive results should not be quoted from a scaled run.

### Withdrawn tests

**c98003b** requires that a MED-priority task make no progress while a
LOW-caller/HIGH-acceptor rendezvous runs. That assumes strict preemptive
priority scheduling on one processor, but RM 9.8 imposes priorities only
among tasks sharing a processor. With tasks as OS threads on a multicore
machine the MED task legitimately runs in parallel, and the test reports its
own failure.

**cc1226b** ends by requiring that two uninitialized variables of a formal
private type compare unequal. Reading them is erroneous, and for the test's
own actual type — a null record with two defaulted discriminants — every
value of the type is equal, so the expectation cannot be met.

**ce3902b** nominally checks `ENUMERATION_IO` parameter names, but closes the
current default output file and reopens that same file object with `IN_FILE`
mode while it remains the default output. That contradicts the default-file
semantics required by ce3208a, a valid test which this compiler implements.
The AVO withdrew ce3902b and kept ce3208a.

## Layout

| Path | Contents |
|---|---|
| `ada83.c` | The compiler |
| `runtime.ada` | The Ada 83 standard packages |
| `tests.zip` | ACATS 1.11, unpacked on demand |
| `Ada83_LRM.md` | Ada 83 language reference manual |
| `test.sh` | Conformance harness |
| `bench.sh` | Benchmarks |
| `build.bat`, `LLVM-C.zip` | Windows build |
