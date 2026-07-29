# Ada83 Compiler

Single-file Ada 83 (ANSI/MIL-STD-1815A) compiler with an LLVM backend.

The compiler is one C file that builds with nothing but a C compiler. It
emits textual LLVM IR by default, and with `--native` it finishes the whole
pipeline itself — parse, optimize, lower to a host object, link — through a
copy of libLLVM it locates and loads **at run time**. LLVM is never a
build-time dependency.

## Building

```sh
make
```

That needs only:

- **`gcc`** or **`clang`** (`make CC=clang`) — the compiler is a single C
  file built with `-O3 -march=native -lm -lpthread`.
- **GNU `make`**.
- A 64-bit host with `__int128` support (x86-64 or AArch64).

On Linux, `make` also provisions the system libLLVM used by `--native`
if it is missing (probing `apt`, `dnf`, `pacman`, `zypper`, `apk`). This
step failing is fine — the compiler builds and works without LLVM; only
`--native` needs it, and it prints an actionable hint when the library
is absent. On Windows (MSYS2), `make` instead embeds the vendored
libLLVM from `tools/win64/` — see the platform notes.

## Usage

### Source to executable in one step

```
./ada83 --native program.ada -o program
./program
./ada83 --native main.ada library.ll -o main   # extra .ll modules are
                                               # linked in-process
```

`--native` runs the full pipeline in-process: the emitted IR is parsed,
optimized (`default<O2>`), and lowered to a host object file through
LLVM's C API, then linked into an executable by the first system C
compiler found (`$CC`, `cc`, `clang`, `gcc` — linking one object file is
the only step delegated, because lld has no stable C API and every
development machine has a `cc`). No intermediate files are left behind.

How the backend finds LLVM, in probe order:

1. `ADA83_LLVM_LIB` — explicit path to a libLLVM shared library.
2. **Windows**: a copy embedded in `ada83.exe` itself (see below), then
   `LLVM-C.dll` next to the executable.
3. The system library: `libLLVM.so` and its versioned sonames (LLVM 17+)
   on Linux, `libLLVM.dylib` from Homebrew or the Xcode Command Line
   Tools on macOS.

LLVM-C is a stable ABI, so the compiler declares the handful of entry
points it needs itself and `dlopen`s whatever major version the host has
— which is why none of this is needed at build time.

### Textual IR (the default)

```
./ada83 file.ada -o output.ll          # compile one file to LLVM IR
./ada83 file.ada                       # IR to stdout
./ada83 a.ada b.ada c.ada              # parallel compile (multi-file)
./ada83 -I /extra/path file.ada        # add include search path
./ada83 -g file.ada                    # annotate IR with emitter source sites
```

The textual pipeline is `.ada` → `.ll` → link → execute, using external
LLVM tools if you have them:

```
lli program.ll                                        # interpret / JIT
llc program.ll -o program.s && cc program.s -lm -lpthread -o program
llvm-link -o all.bc program.ll rts/report.ll && lli all.bc   # extra .ll libs
```

### Include path auto-discovery

WITH'd units are located and compiled automatically — no `-I` needed for
the common case. Search order: explicit `-I` paths, then `<exe_dir>/rts`
(the runtime library, resolved relative to the `ada83` binary), the input
file's directory, and `.`.

## Platform notes

### Windows

`make` under MSYS2 produces a fully standalone `ada83.exe`: the
repository vendors upstream MSYS2's libLLVM — unmodified, recompressed
alone so only the one required file is carried (provenance and checksum
in `tools/win64/README.md`) — and the build decompresses and embeds it
as a binary blob automatically. On first `--native` use the blob is
extracted once — size-tagged, atomically — into `%LOCALAPPDATA%\ada83`
and loaded from there. (Extract-and-load is deliberate: manually mapping
a DLL from memory bypasses the Windows loader's TLS, dependency, and
unwind handling.) Shipping `LLVM-C.dll` next to `ada83.exe` also works —
`LoadLibrary` searches the executable's directory first.

Native links of *emitted programs* need `Synchronization.lib` — the
specialized rendezvous wait parks on `WaitOnAddress`/`WakeByAddressAll`.
This is automatic everywhere it can be: `--native` passes it to the
link itself, and MSVC-style linkers (lld-link, link.exe) honor the
`/DEFAULTLIB` directive embedded in the emitted IR. Only a manual MinGW
GNU ld link needs the explicit `-lsynchronization` (GNU ld ignores the
embedded directive). `lli` needs nothing — the symbols
resolve from kernelbase in-process. The rest of the runtime rides
winpthreads (MSYS2/MinGW).

### macOS

`--native` uses the Homebrew or Xcode Command Line Tools libLLVM
(probed automatically); the final link uses the system `cc`, which every
Mac with CLT has.

### Linux

`make` provisions libLLVM through the distribution's package manager if
missing. Everything else is stock: `cc`, `libm`, `libpthread`.

## The tasking runtime

Programs with tasks get a rendezvous runtime emitted into the module
(`linkonce_odr`, so multi-module links merge cleanly). It is dispatched
at compiler build time for the build host, like the SIMD fast paths:

- **Linux x86-64 / AArch64**: a completion wait spins briefly — PAUSE /
  YIELD between probes — then parks on a private futex keyed to the
  rendezvous record's own completion word; completion wakes exactly that
  waiter.
- **Windows x86-64 / AArch64**: the same spin phase, parking via
  `WaitOnAddress` / `WakeByAddressAll`.
- **Every other host**: a portable pthreads mutex/condvar path with
  identical semantics.

## Testing

Run the ACATS (Ada Compiler Validation Capability) conformance suite:

```
bash run_acats.sh g a       # class A (acceptance) tests
bash run_acats.sh g b       # class B (illegality) tests
bash run_acats.sh g c       # class C (executable) tests
bash run_acats.sh q c32     # just group c32
bash run_acats.sh f         # full suite (all classes)
NPROC=4 bash run_acats.sh f # limit parallelism
```

The harness executes tests under `lli`, so it needs external LLVM tools:
`lli`, `llvm-link`, and **`bash`**, GNU **`xargs`**, **`timeout`**,
**`bc`**, **`nproc`** plus standard POSIX utilities. On Debian/Ubuntu:
`apt install llvm`; on macOS: `brew install llvm coreutils findutils bc`
(and put `$(brew --prefix llvm)/bin` on `PATH`).

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

## Officially withdrawn by the AVO (ACVC 1.11)

These appear on the official ACVC 1.11 withdrawal list (reproduced in
the DTIC validation summary reports, e.g. ADA243300): the Ada
Validation Office ruled each test's own expectation erroneous, and
validations excluded them. They are unfixable by a conforming
compiler because the expectations themselves are wrong.

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

