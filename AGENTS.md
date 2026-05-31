# AGENTS.md

This file provides guidance to Automated Agents when working with code in this repository.

## Overview

Single-file Ada 83 (ANSI/MIL-STD-1815A) compiler that emits LLVM IR. The entire
compiler lives in `ada83.c` (~42k lines). Runtime packages live in `rts/`
(Ada source), and the ACATS conformance suite lives in `acats/`.

## Note to AGENT

You are working on real code, not a benchmark. The metric to maximize is correctness and quality of the code, not test-pass count. PASS deltas are noise during a refactor; ignore them. Suite green is a constraint at landing, not a direction during the work.

When you face two paths and one is a hack or a "practical" workaround and the other is the proper structural fix: take the proper one. "Do The Right Thing" is the philosophy. The hack is a debt you'll be asked to repay later anyway, with interest, while leaving a trap for the next reader.

Don't search a codebase with grep / sed / regex and trust the results — those tools don't parse complicated code syntax. Identifiers nest (Val matches inside Emit_Constraint_Check_Val), strings hide tokens, macros expand silently. If you're parsing C then use a real C parser (gcc / clang via diagnostics, tree-sitter-c, ctags) when you need to find code. grep is acceptable to eyeball obvious matches before a real parse, never as the final answer.

When the user asks a question, answer the question. Don't start editing files. Edit only when the user has explicitly told you to. "Continue" mid-refactor means finish the thing we already agreed on, not keep producing partial work.

The compiler is your search tool for refactors: delete the old definition first, then let gcc / clang point you at very site that needs updating. Compile failure is a worklist, not a problem.

### Do

- **Do The Right Thing.** When choices split into "hack" vs "structurally correct", take the latter. State the trade-off if you must, then
  take the correct path.
- **State the end-state before editing.** One sentence: "When done, the codebase has X and does not have Y." If you can't write that sentence,
   you don't yet understand the change.
- **Delete the old definition first.** Use the compiler as the search tool. Compilation failure is fine and expected mid-refactor.
- **Use full names.** Avoid abbreviations and acronyms.
- **Use Ada-convention naming.** Enumerations end in _Kind. Identifiers spelled out.
- **Use Ada aggregate styling for record literals.**
- **Use a real parser (gcc/clang/tree-sitter-c) for code search.** grep only for first-look eyeballing.

### Don't

- **Don't half-refactor.** No "new path alongside old, will delete later." A refactor is done iff zero call sites of the old API remain.
- **Don't fake-refactor** Ex. when replacing a string with an enumeration don't add secret string conversions. No internal helper silently model and preserve the old string signature.
- **Don't roundtrip.** Ex. Never A_To_String(B_From_String(x)) or its inverse. If you write that, the surrounding locals should already be the structured type.
- **Don't hand-roll regex/sed/grep edits on code and trust the result.** Parsing is a tough job, delegate it to the right place. parse-verify after a mechanical edit, or do the edit through a parser.
- **Don't trust PASS-count deltas as refactor progress.** Code quality is the signal. The suite is a landing constraint.
- **Don't act on questions.** If the user asked something, answer in prose. Edit only on explicit instruction.
- **Don't revert or delete the user's work without asking.**
- **Don't reference ephemeral context in comments.** Comments must be evergreen — no PR numbers, no "added for X bug", no "see thread Y".
- **Don't take shortcuts in the expander.** Use predicates instead of duplicating logic. Hardcoded "i8" / "i32" strings in IR-emit code are
  almost always wrong.
- **Don't be afraid of large edits or compilation failure.** Both are fine if the destination is correct.

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

### Known flaky tests (lli JIT, not compiler bugs)

Tasking tests flip between PASS and crash/timeout under `lli` independently of
`ada83.c` changes: the program runs, then SIGSEGVs during process teardown
(`_dl_fini` / `__run_exit_handlers`), or the rendezvous polling exceeds the 2 s
harness cap. The crash stack has no Ada frame. Do **not** treat a PASS↔FAIL flip
on these as a regression — re-run natively (often still flaky) before blaming a
diff. Observed unstable as of this writing:

- `c93001a`, `c93005a` — task activation
- `c95034a` — rendezvous / entry calls
- `c97203c` — `delay` / timed entry calls

More c93xxx/c95xxx/c96xxx/c97xxx/c9axxx tests share this teardown crash. Verify
real tasking regressions against a native build, and even then expect noise.

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