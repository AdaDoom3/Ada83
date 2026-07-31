# Ada83

An Ada 83 (ANSI/MIL-STD-1815A) compiler in a single C file, with an LLVM
backend.

## Building

| Platform | Command | Notes |
| -------- | ------- | ----- |
| Linux    | `make`  | GCC or Clang; installs libLLVM via the system package manager if absent |
| macOS    | `make`, or run `make.applescript` | Apple Clang; libLLVM via Homebrew |
| Windows  | `make.bat` | GCC, Clang or Zig, whichever is found first; downloads Zig if none is present |

Prebuilt compilers ship with the repository:

| Archive | Contents |
| ------- | -------- |
| [`bin-linux.zip`](bin-linux.zip) | `ada83`, baseline x86_64, and `ada83-runtime.ada`; needs glibc 2.34 or newer, and libLLVM from the system package manager |
| [`bin-macos.zip`](bin-macos.zip) | `ada83`, universal x86_64 + arm64, and `ada83-runtime.ada`; libLLVM via Homebrew |
| [`bin-windows.zip`](bin-windows.zip) | `ada83.exe`, `ada83-runtime.ada`, and `LLVM-C.dll` with its companion DLLs; unzip them together |

`ada83-runtime.ada` holds the standard library, and the compiler looks for it
beside its own executable, so keep the two together when unpacking. To rebuild
an archive from source:

| Platform | Command | Produces |
| -------- | ------- | -------- |
| Linux    | `make package` | `bin-linux.zip` |
| macOS    | `make package`, or `osascript make.applescript package` | `bin-macos.zip`, both slices lipo'd together |
| Windows  | `make.bat package` | `bin-windows.zip`, DLLs included |

The compiler also cross-builds with [Zig](https://ziglang.org) from any host,
for example:

```sh
zig cc -O2 -std=gnu2x -target x86_64-windows-gnu ada83.c -o ada83.exe -lm
zig cc -O2 -std=gnu2x -target aarch64-macos     ada83.c -o ada83 -lm -lpthread
```

## Conformance

Under ACATS 1.11 all 3561 tests pass

| Suite | Category | Passed | Completion |
|:-----:|----------|-------:|-----------:|
| **A** | Acceptance | `140 / 140` | **100%** ✅ |
| **B** | Illegality | `1350 / 1350` | **100%** ✅ |
| **C** | Executable | `1973 / 1973` | **100%** ✅ |
| **D** | Numerics | `17 / 17` | **100%** ✅ |
| **E** | Inspection | `34 / 34` | **100%** ✅ |
| **L** | Post-compilation | `47 / 47` | **100%** ✅ |
| | **Total** | **`3561 / 3561`** | **100%** ✅ |

## Benchmarks

Run time of the generated code at `-O2`, median of 7 runs after one warmup,
on Linux x86_64 with 4 cpus (Intel Xeon @ 2.10 GHz), against GNAT 13.3.0.

| Program | Stresses | ada83 (s) | gnat (s) | Ratio | Result |
|---------|----------|----------:|---------:|------:|-------:|
| **exceptions** | raise, propagate, handle | `0.027` | `3.889` | `0.01` | **144.0× faster** ✅ |
| **taskselect** | selective wait with an else part | `0.240` | `1.139` | `0.21` | **4.7× faster** ✅ |
| **lu** | LU decomposition, float division | `0.064` | `0.248` | `0.26` | **3.9× faster** ✅ |
| **tasking** | rendezvous throughput | `4.029` | `8.295` | `0.49` | **2.1× faster** ✅ |
| **strings** | slices and character work | `0.056` | `0.058` | `0.97` | **level** |
| **numerics** | fixed point and 12-digit float | `0.119` | `0.092` | `1.29` | 1.3× slower |
| **checks** | range and index checks in a hot loop | `0.258` | `0.175` | `1.47` | 1.5× slower |
| **memory** | allocation and deallocation | `0.414` | `0.262` | `1.58` | 1.6× slower |
| **sieve** | integer arrays, index checks | `0.105` | `0.058` | `1.81` | 1.8× slower |
| **taskflood** | task creation and termination | `0.921` | `0.444` | `2.07` | 2.1× slower |
| **recurse** | call and return | `0.058` | `0.022` | `2.64` | 2.6× slower |
| **matmul** | dense float, nested loops | `0.048` | `0.015` | `3.20` | 3.2× slower |

Ratio is `ada83 / gnat`, so below `1.00` is the faster of the two. Exception
handling and selective wait are where the two differ most. `numerics` prints a
different representation of the same values, as the standard permits.

These figures come from a shared virtual machine, where the tasking and
exception rows varied by tens of percent between runs. The wide margins hold
well outside that spread; the rows close to `1.00` should be read as roughly
level rather than as an exact ordering.

Reproduce with:

```sh
bash bench.sh codegen
```

The harness has further modes, each measuring the compiler rather than the
code it emits:

```sh
bash bench.sh stages            # front end against back end
bash bench.sh parser            # front end against input size
bash bench.sh corpus            # throughput, and the slowest inputs named
bash bench.sh compare /tmp/old  # this build against another, with deltas
bash bench.sh profile           # the functions compiling spends its time in
bash bench.sh codegen           # run time of the generated code
bash bench.sh memory            # peak resident set, compiling and running
bash bench.sh help
```

To check a change, keep the old binary and name it:

```sh
cp ada83 /tmp/before && make && bash bench.sh compare /tmp/before
```

## Use

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

The compiler emits LLVM IR, so the IR can be taken directly:

```sh
./ada83 --ir hello.ada -o hello.ll      # textual LLVM IR
./ada83 --emit-llvm hello.ada -o hello  # native, keeping the optimised IR
./ada83 --ir a.ada b.ada c.ada          # several units, one process each
lli hello.ll                            # interpret the IR
```

## Tests

The ACATS tests are in `tests.zip` and unzipped on first use.

```sh
bash test.sh run all      # every class
bash test.sh run c        # one class
bash test.sh run c45      # one group
bash test.sh check        # run, then diff against the baseline
bash test.sh help
```

Each run writes to its own directory under `test_results/`, with
logs under `acats_logs/`, so concurrent runs don't overwrite eachother.
