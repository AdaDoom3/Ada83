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

Run time of the generated code at `-O2`, against GNAT 13.3.0 (GCC
`13.3.0-6ubuntu2~24.04.1`), on Linux x86_64 with 4 cpus (Intel Xeon @ 2.10 GHz),
with libLLVM `20.1.2` behind ada83's back end.

Both binaries are timed **alternately** — one run of each, and which goes first
swaps every repetition, so drift in the machine's load falls on both equally —
**pinned to the same core** at `nice -5`, `25` timed repetitions after `3`
warmup runs that are thrown away. Each figure is the **median ± median absolute
deviation** of those repetitions. The whole suite is then measured a **second
time**, and a row is given a ratio only where the two medians differ by more
than the sum of their MADs *and* the two passes agree to within 10%. Rows that
fail either test are marked, not ordered.

| Program | Stresses | ada83 (s) | gnat (s) | Ratio | Result |
|---------|----------|----------:|---------:|------:|-------:|
| **exceptions** | raise, propagate, handle | `0.029 ± 0.001` | `3.806 ± 0.042` | `0.01` | **131.2× faster** ✅ |
| **lu** | LU decomposition, float division | `0.058 ± 0.001` | `0.205 ± 0.004` | `0.28` | **3.5× faster** ✅ |
| **strings** | slices and character work | `0.057 ± 0.001` | `0.055 ± 0.001` | — | *indistinguishable* |
| **numerics** | fixed point and 12-digit float * | `0.122 ± 0.002` | `0.090 ± 0.001` | `1.36` | 1.4× slower |
| **memory** | allocation and deallocation | `0.382 ± 0.009` | `0.257 ± 0.009` | `1.49` | 1.5× slower |
| **checks** | range and index checks in a hot loop | `0.258 ± 0.004` | `0.164 ± 0.004` | `1.57` | 1.6× slower |
| **sieve** | integer arrays, index checks | `0.087 ± 0.003` | `0.051 ± 0.001` | `1.71` | 1.7× slower |
| **recurse** | call and return | `0.057 ± 0.001` | `0.023 ± 0.000` | `2.48` | 2.5× slower |
| **matmul** | dense float, nested loops | `0.049 ± 0.001` | `0.017 ± 0.001` | `2.88` | 2.9× slower |

Ratio is `ada83 / gnat`, so below `1.00` is the faster of the two. **strings** is
the row where `0.057` against `0.055` is smaller than the two spreads put
together, so no ordering is claimed for it. Every program above prints output
identical to GNAT's, byte for byte, except `numerics` (`*`), where the two
totals genuinely differ — `132` against `720` — because Ada 83 leaves an
implementation free to pick its own `small` for a fixed point type, and 40
million iterations of accumulation make that visible. That row therefore
compares two computations that are alike rather than identical.

### The programs with tasks

Tasks cannot be squeezed onto one core honestly: a task that polls — which is
what `select ... else` does — burns its whole timeslice while the task it is
waiting for sits behind it in the run queue, so a single core measures the
scheduler rather than the code. These three get a fixed *pair* of cores, the
same pair for both compilers, and are otherwise measured exactly as above.

| Program | Stresses | ada83 (s) | gnat (s) | Ratio | Result |
|---------|----------|----------:|---------:|------:|-------:|
| **taskselect** | selective wait with an else part | `0.210 ± 0.011` | `1.120 ± 0.073` | `0.19` | **5.3× faster** ✅ |
| **taskflood** | task creation and termination | `0.702 ± 0.025` | `0.333 ± 0.013` | `2.11` | 2.1× slower |
| **tasking** | rendezvous throughput | `3.699 ± 0.632` | `6.836 ± 0.485` | — | *not reproducible* |

**`tasking` is the row to be careful with.** Its ratio came out `0.54` in the
first pass and `0.73` in the second, a 35% move and far outside its own spread,
so no ratio is published for it. The previous version of this table claimed
`2.1× faster` there; that claim is withdrawn.

How much these three depend on the cores they are given, same machine, same
binaries — the one-core column is 5 repetitions, enough to show the size of it:

| Program | Ratio on cpus 2,3 | Ratio on cpu 3 alone |
|---------|------------------:|---------------------:|
| **tasking** | `0.54` / `0.73` | `3.38` |
| **taskflood** | `2.11` | `1.66` |
| **taskselect** | `0.19` | `0.01` |

On one core `tasking` reverses outright, and `taskselect` goes from GNAT taking
`1.120 s` to GNAT taking `86.984 s` — its polling loop spending a full timeslice
per rendezvous. Neither number is wrong; they are answers to different
questions, which is exactly why the allocation is fixed and stated rather than
left to the scheduler.

### The same numbers, twice

Two full passes, about twenty minutes apart. This is the evidence that the
ratios above are properties of the code rather than of the afternoon:

| Program | Ratio, pass 1 | Ratio, pass 2 | Drift |
|---------|--------------:|--------------:|------:|
| **sieve** | `1.71` | `1.65` | `-3.4%` |
| **matmul** | `2.88` | `2.82` | `-2.0%` |
| **lu** | `0.283` | `0.289` | `+2.2%` |
| **recurse** | `2.48` | `2.43` | `-1.8%` |
| **strings** | — | — | *no ratio claimed* |
| **numerics** | `1.36` | `1.34` | `-0.8%` |
| **checks** | `1.57` | `1.55` | `-1.2%` |
| **exceptions** | `0.00762` | `0.00737` | `-3.3%` |
| **memory** | `1.49` | `1.49` | `+0.2%` |
| **tasking** | `0.541` | `0.732` | `+35.2%` |
| **taskflood** | `2.11` | `2.11` | `+0.2%` |
| **taskselect** | `0.187` | `0.193` | `+3.1%` |

Ten of the eleven ratios move by less than 4%. `tasking` moves by 35%, and is
the one row published without a ratio. A third pass at `CODEGEN_REPEATS=11`
agreed with both on every row that carries one — and gave `tasking` a third
answer, `0.53`, which is the point.

### Caveats

This is a **shared virtual machine**. The load average was `1.28` when the run
started and reached `2.03` during it; the harness samples it throughout and
prints what it saw, and refuses to start above `2.0` unless told otherwise.
The seconds are seconds *on this host* — another machine will not reproduce
them, and only the ratios travel, and only between machines running the same
two compilers.

Reproduce with:

```sh
bash bench.sh codegen                                # two passes, about 20 minutes
SUITES=1 CODEGEN_REPEATS=11 bash bench.sh codegen    # one quicker pass
```

For figures comparable with someone else's, run it in the pinned environment,
which fixes the GNAT, the libLLVM the back end loads, and the flags ada83 itself
is built with. The image prints its own toolchain versions before any number, so
a pasted result carries its provenance:

```sh
docker build -f bench.Dockerfile -t ada83-bench .
docker run --rm --cpuset-cpus=0-3 --cap-add=SYS_NICE ada83-bench
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

## Editor

`ada83 --lsp` serves the Language Server Protocol on stdin and stdout, so the
compiler itself answers the editor. Diagnostics come from the same lexer,
parser and resolver that build the program, and cannot disagree with it. Only
the front end runs, so libLLVM is not needed and the server starts instantly.

![The Ada 83 extension for VS Code](screenshot.gif)

[`ada83-extension.js`](ada83-extension.js) is the whole extension, with no
dependencies: the protocol is a Content-Length header and JSON, written out in
one file rather than pulled in as a library, and the manifest, grammar,
language configuration, snippets, README and chat instructions ride at the end
of it as line comments. `make vsix` splits them back out into `ada83.vsix`,
alongside `manual.md`, and the archives carry it:

```sh
code --install-extension ada83.vsix
```

Then point `ada83.compilerPath` at the compiler.

| | |
| --- | --- |
| Diagnostics | errors and warnings as you type, from the compiler |
| Go to definition | any identifier, including into `ada83-runtime.ada` |
| Hover | what a name is, and its profile, in the compiler's own words |
| Find references | occurrences of a name, and highlight as the caret moves |
| Related information | the compiler's notes as links inside the error |
| Quick fixes | the compiler's own "did you mean" offered as an edit |
| Outline | what a file declares, from the resolver's symbol table |
| Workspace symbols | Ctrl+T over the Ada sources in the workspace root |
| Completion | every visible name, and the 63 reserved words |
| Signature help | the profile of the call being written, argument by argument |
| Folding | `is`/`begin`/`end`, records, cases, loops, ifs, selects, comments |
| Tasks and commands | build or check the open file, with the `ada83` matcher |
| Manual search | `#ada83Manual` reads the standard for Copilot |
| Syntax highlighting | the 63 Ada 83 reserved words, and no later ones |
| Snippets | the shapes of the language |

The grammar is generated from the compiler's own token table, so it cannot
drift from what the compiler accepts, and `ada83-extension-test.js` tokenizes a
battery of designed fragments through the engine VS Code uses to check it —
that `end record` closes a record, that a keyword inside an identifier is not
a keyword, and that the ten words Ada gained after 1983 stay ordinary
identifiers.

Any other editor can use the same server:

```sh
ada83 --lsp
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
