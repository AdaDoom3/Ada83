# Ada83 Compiler

Single-file Ada 83 (ANSI/MIL-STD-1815A) compiler targeting LLVM IR.

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

## Reference

Reference material is in the `reference/` directory including
`Ada83_LRM.md`, `DIANA.md` (an AST guide), and `gnat-design.md` — as
well as the entire GNAT sources themselves.
