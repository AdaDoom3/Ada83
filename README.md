# Ada83 Compiler

Single-file Ada 83 (ANSI/MIL-STD-1815A) compiler targeting LLVM IR.

## Prerequisites

Build the compiler:

- **`gcc`** — the compiler is a single C file built with `-march=native -lm -lpthread`.
  Clang works too (`make CC=clang`).
- **GNU `make`**.
- A 64-bit host with `__int128` support (x86-64 or ARM64).

Run compiled Ada programs and the ACATS test suite (LLVM ≥ 14 recommended):

- **`lli`** — LLVM IR interpreter / JIT used to execute `.ll` and `.bc` files.
- **`llvm-link`** — links the program's `.ll` with `acats/report.ll` and any
  runtime packages.
- **`llc`** — only needed if you go through native assembly (`make %.exe`).

On Debian/Ubuntu: `apt install build-essential llvm` (provides `lli`,
`llvm-link`, `llc`).
On Arch:           `pacman -S base-devel llvm`.
On macOS:          `brew install llvm` (then ensure `$(brew --prefix llvm)/bin`
is on `PATH`).

Test harness (`run_acats.sh`) additionally needs: **`bash`**, GNU **`xargs`**,
**`timeout`**, **`bc`**, **`nproc`**, and standard POSIX utilities (`grep`,
`sort`, `head`, `cut`, `basename`). All ship with `coreutils` /
`findutils` / `bc` on Linux; on macOS install GNU versions
(`brew install coreutils findutils bc`) or expect minor harness breakage.

Quick check that everything is on `PATH`:

```sh
gcc --version && lli --version && llvm-link --version && llc --version
```

## Building

```sh
make all
```

## Usage

```
./ada83 file.ada -o output.ll          # compile one file
./ada83 file.ada                       # output to stdout
./ada83 a.ada b.ada c.ada              # parallel compile (multi-file)
./ada83 -I /extra/path file.ada        # add include search path
```

### Include path auto-discovery

The compiler automatically adds these directories to the include search
path — no `-I` flags are needed for the common case:

1. `<exe_dir>/rts` — the runtime library, located relative to the `ada83`
   binary (resolved via `/proc/self/exe`).
2. The directory containing the input file (e.g. compiling `acats/foo.ada`
   adds `acats/`).
3. `.` — the current working directory.

Explicit `-I` paths are searched first, before the auto-discovered ones.

### Running programs

The compilation pipeline is: `.ada` -> `.ll` (LLVM IR) -> link -> execute.

```
./ada83 program.ada -o program.ll
lli program.ll                         # interpret directly
# or:
llc program.ll -o program.s && gcc program.s -lm -o program
```

Programs that WITH library packages need linking:

```
llvm-link -o program.bc program.ll rts/report.ll
lli program.bc
```

## Testing

Run the ACATS (Ada Compiler Validation Capability) test suite:

```
bash run_acats.sh g a       # class A (acceptance) tests
bash run_acats.sh g b       # class B (illegality) tests
bash run_acats.sh g c       # class C (executable) tests
bash run_acats.sh q c32     # just group c32
bash run_acats.sh f         # full suite (all classes)
NPROC=4 bash run_acats.sh f # limit parallelism
```

### ACATS delay deviation (temporary, development only)

The ACATS sources under `acats/` are modified from pristine: every `DELAY`
literal >= 1.0 seconds (and the named delay constants `WAIT_TIME` /
`DELAY_TIME` and `c96001a`'s delay variables) is **scaled down by 10x**
(`DELAY 5.0` -> `DELAY 0.5`). Each modified line is marked with

```
-- TODO: acats-delay-deviation: before was <original>
```

Rationale: the delay values are 1980s-era scheduling-margin padding; the
semantics they exercise (delay accuracy, master-wait ordering, timed-call
expiry) are preserved at 10x smaller margins on modern hardware, and the
full Class C run drops from ~5 minutes back to under a minute. The harness
cap (`TEST_TIMEOUT` in `run_acats.sh`, default 15 s) is sized to match.

**Before any conformance claim**: revert the deviation (grep for
`acats-delay-deviation`, restore the recorded values) and raise
`TEST_TIMEOUT` back to 120.

## Reference

Reference material is in the `reference/` directory including
`Ada83_LRM.md`, `DIANA.md` (an AST guide), and `gnat-design.md` — as
well as the entire GNAT sources themselves.
