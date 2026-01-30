# Ada83 Compiler

Single-file Ada 83 (ANSI/MIL-STD-1815A) compiler targeting LLVM IR.

## Building

```
make
```

Requires GCC (or a C11 compiler), LLVM tools (`lli`, `llvm-link`, `llc`),
and pthreads. The build links with `-lm -lpthread`.

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

### Parallel compilation

When multiple input files are given, the compiler spawns one thread per
logical CPU (`sysconf(_SC_NPROCESSORS_ONLN)`) and compiles files in
parallel using fork-per-file for full isolation. Each output `.ll` file
is derived from its input name (`foo.ada` -> `foo.ll`). The `-o` flag
cannot be used with multiple inputs.

### ALI files and staleness detection

Every compilation produces a `.ali` (Ada Library Information) file
alongside the `.ll` output. ALI files record:

- **Compiler build timestamp** — the `V` line embeds `__DATE__` and
  `__TIME__` from the compiler build. When the compiler is rebuilt,
  all existing ALI files become stale and the compiler will re-parse
  from source rather than reuse cached metadata.
- **Source checksums** — CRC32 of the source text, used to detect
  edits without relying on file timestamps.
- **WITH dependencies** — unit dependency graph for separate compilation.
- **Exported symbols** — types, variables, subprograms, and their LLVM
  mangled names, enabling compilation against a package without its source.

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
bash test.sh g a       # class A (acceptance) tests
bash test.sh g b       # class B (illegality) tests
bash test.sh g c       # class C (executable) tests
bash test.sh q c32     # just group c32
bash test.sh f         # full suite (all classes)
NPROC=4 bash test.sh f # limit parallelism
```

## Project structure

```
ada83.c          Single-file compiler source (~25K lines)
Makefile         Build rules
test.sh          Parallel ACATS test harness
rts/             Runtime support packages (system, text_io, calendar, ...)
acats/           ACATS test suite (~4700 tests)
reference/       Ada83 LRM, DIANA AST guide, GNAT design notes
```

## Reference

Reference material is in the `reference/` directory including
`Ada83_LRM.md`, `DIANA.md` (an AST guide), and `gnat-design.md` — as
well as the entire GNAT sources themselves.
