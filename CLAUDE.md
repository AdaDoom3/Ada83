# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview

Single-file Ada 83 (ANSI/MIL-STD-1815A) compiler that emits LLVM IR. The entire
compiler lives in `ada83.c` (~42k lines). Runtime packages live in `rts/`
(Ada source), and the ACATS conformance suite lives in `acats/`.


## Note to claude.

This is not some test suite. You (the claude model) are not being tested you're in the real
world solving a real problem. 

Do not be obsessed with metrics going up or with having every test passing. 
Sometimes it's ok for the program not to compile for a while. 
Sometimes it's ok for some test regressions to appear if you're making real and correct progress.
Just be sure you know you are making correct progress.
Don't chase the `number go up` at all costs. You aren't being graded on that metric anymore.
The metric you should want to improve is the user's satisfaction. 
The user is satisfied when the code improves and gets closer to being correct/high quality.
The final goal of the user are:
1. the minimum lines of code needed are used to solve the problem.
2. there is as little code duplication as possible. 
3. every engineering decision was made to improve the quality of the solution and the final product.
4. the code is reasonably preformant.
5. every time a simple solution is possible and we don't have a preformance reason to make it more complicated, we choose the simple solution.
6. the code is readable due to following proper coding styles and conventions. (see #behavior-dos-and-don'ts)

## Common commands

Build the compiler:

```
make                       # gcc -O3 -Wall -lm -lpthread -march=native -> ./ada83
make clean                 # also removes test_results/, acats_logs/, acats/report.ll
make clean-test            # keeps the compiler binary
```

Compile and run a program (pipeline is `.ada` → `.ll` → link → execute):

```
./ada83 file.ada -o out.ll
./ada83 file.ada                 # to stdout
./ada83 a.ada b.ada c.ada        # parallel multi-file compile (fork per file)
./ada83 -I /extra/path f.ada     # explicit -I; searched before auto paths
./ada83 -g a.ada                 # add debug Emit locations in the IR ex: `; @ Emit_Function_Header:35289`
lli out.ll                       # interpret directly
llvm-link -o prog.bc a.ll rts/report.ll && lli prog.bc   # for WITH'd packages
```

Include-path auto-discovery (no `-I` needed for the common case): `<exe_dir>/rts`
(resolved via `/proc/self/exe`), the input file's directory, then `.`. Explicit
`-I` paths search first.

ACATS test harness (parallel via `xargs`, written to `test_results/` and
`acats_logs/`):

```
bash run_acats.sh g a            # all class A (acceptance) tests
bash run_acats.sh g b            # class B (illegality)
bash run_acats.sh g c            # class C (executable) - default if blank
bash run_acats.sh g d|e|l        # numerics / inspection / post-compilation
bash run_acats.sh q c32          # one group (e.g. c32, c34)
NPROC=4 bash run_acats.sh g c    # cap parallelism (defaults to `nproc`)
```

`run_acats.sh` rebuilds `ada83` when `ada83.c` is newer, and always recompiles
`acats/report.adb` → `acats/report.ll` before running. Tests whose basename ends
in a digit (not `m`) are treated as multi-file fragments and skipped.

Per-class pass criteria the harness applies:
- A: compiles, links, and `lli` exit 0
- B: compiler **rejects** the file and ≥90% of `-- ERROR` lines are within ±1
  line of an emitted diagnostic
- C: output contains `PASSED` (`NOT APPLICABLE` → skip, `FAILED` → fail)
- D: numeric pass needs `PASSED` in output
- E: `TENTATIVELY PASSED` counts as pass (manual inspection class)
- L: post-compilation; passes if the compile **or** link **or** run is rejected

## Behavior dos and don'ts

### Do
 
 1. **DO** The Right Thing at all times. Do not use half measures.
 2. **DO** use full names. Avoid abbreviation and acronyms.
 3. **DO** prefer Ada convention naming. For example enumerations should be `_Kind`. First, Last, Item, Name, ... etc.
 4. **DO** prefer Ada aggregate styling.

### Don't

 1. **DON'T EVER** Revert or delete work without asking the user first.
 2. **DON'T EVER** Be afraid of large edits. Don't be afraid of breaking things that need breaking.
 3. **DON'T EVER** Be afraid of being in compilation failure state. For example Compiler errors can be important during refactors.
 4. **DON'T EVER** Reference ephemeral context in comments - they must be evergreen
 5. **DON'T EVER** take  shortcuts in the expander - you MUST handle complex Ada types and use predicates instead of duplicating functionality. For example, emitting directly hard-coded types like "i8" and "i32" are ALMOST ALWAYS WRONG.

## Code architecture (`ada83.c`)

The file is split into a **specification** half (forward decls, types, comments)
and a **body** half, both indexed by the same `§1…§19` numbers. Jump targets:

| §   | Subsystem            | Notes |
|-----|----------------------|-------|
| §1  | Foundation           | Sizes, ctype wrappers, target constants. `__int128` is required (Ada literal arithmetic). |
| §2  | Measurement          | Bit/byte morphisms (`To_Bits` total; `To_Bytes` ceil); `Llvm_Int_Type` / `Llvm_Float_Type`. |
| §3  | Memory               | Single bump arena (`Arena_Allocate` 16-byte aligned, 16 MiB chunks). **Nothing is freed until `Arena_Free_All` at shutdown** — do not `free()` arena pointers. |
| §4  | Text                 | Non-owning `String_Slice`; FNV-1a case-folded hashing for the case-insensitive symbol table. |
| §5  | Provenance           | Source locations + diagnostic accumulator. |
| §6  | Arithmetic           | Big integers, big reals, exact rationals — Ada's universal numeric types demand this, IEEE doubles are not sufficient. |
| §7  | Lexer                | Token kinds, keyword lookup, scanners. |
| §8  | Syntax               | Node kinds, syntax tree, node lists. |
| §9  | Parser               | Hand-written recursive descent for the full Ada 83 grammar. |
| §10 | Types                | The Ada type lattice and `Type_Info`. |
| §11 | Names                | Symbol table, scopes, overload resolution (cap = `MAX_INTERPRETATIONS`). |
| §12 | Semantics            | Name res, type checking, constant folding. |
| §13 | Code generation      | LLVM IR emission. Sub-sections: §13.1 primitives, §13.2 runtime checks (gated by `CHK_*` flags, suppressible via `pragma Suppress`), §13.3 fat-pointers (`{ptr, ptr}`, 16 B) for unconstrained arrays, §13.5 exceptions, §13.10 build-in-place. |
| §14 | Library mgmt         | ALI files, checksums, dependency tracking. |
| §15 | Elaboration          | Topological order across compilation units. |
| §16 | Generics             | Macro-style instantiation. |
| §17 | File loading         | Include-path search; `Loading_Set` detects circular `with` chains. |
| §18 | SIMD                 | Compile-time dispatch via `SIMD_X86_64` (AVX-512BW / AVX2) / `SIMD_ARM64` (NEON) / `SIMD_GENERIC` scalar fallback. AVX-512/AVX2 availability is runtime-detected into `Simd_Has_Avx512` / `Simd_Has_Avx2`. |
| §19 | Driver               | Argument parsing; multi-file mode forks a worker per file. |

When editing the C file, keep the **spec/body split** intact: add a forward
declaration in the §N spec block and the definition in the §N body block. The
§ numbers in the table of contents (top of the file) drive navigation; keep them
in sync if you add a section.

### Runtime library (`rts/`)

Ada 83 standard packages implemented in Ada itself: `text_io`, `calendar`,
`direct_io`, `sequential_io`, `system`, `unchecked_conversion`,
`unchecked_deallocation`, `io_exceptions`, `low_level_io`. These are picked up
automatically via the `<exe_dir>/rts` include path. The `.gitignore`
whitelists `rts/rt.ll` and `rts/rt_wrappers.ll` against the global `*.ll`
ignore.

### Reference material (`reference/`)

`Ada83_LRM.md` (language reference), `DIANA.md` (AST guide), `gnat-design.md`,
and the GNAT sources themselves. Consult these for spec questions before
guessing.