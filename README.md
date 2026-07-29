# Ada83 Compiler

Single-file Ada 83 (ANSI/MIL-STD-1815A) compiler with an LLVM backend.

## Building

```sh
make
```

- **`gcc`** or **`clang`** (`make CC=clang`) — the compiler is a single C
  file built with `-O3 -march=native -lm -lpthread`.
- **GNU `make`**.
- A 64-bit host with `__int128` support (x86-64 or AArch64).

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

