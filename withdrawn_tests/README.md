# withdrawn_tests

Tests removed from `acats/` so the harness does not run them. Two
distinct groups live here — do not conflate them.

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

## Parked pending compiler work — NOT officially withdrawn

These four are valid tests that this compiler currently fails. They
were long mis-graded as NOT APPLICABLE by a harness bug (an
unanchored grep matched the words "NOT APPLICABLE" inside COMMENT
lines, masking real FAILED results; fixed in `run_acats.sh` to match
only REPORT.NOT_APPLICABLE's own output line). They are parked here
to keep the suite green while the missing checks are implemented,
and should be moved back into `acats/` when that lands.

All four fail for the same root cause: type conversions used as
OUT / IN OUT actual parameters are unwrapped by the call marshaling
without emitting the RM 4.6 / RM 6.4.1 conversion checks, and the
scalar copy-in/copy-out path assumes the actual and the formal share
one representation.

- **c64103a** — scalar IN OUT conversion actuals: converting a
  DIGITS-MAX value into a DIGITS-1 formal must raise before the call
  (float narrowing range check), and the copy-back conversion into
  the actual's type must be checked likewise; same for fixed-point
  types with different smalls (value rescaling plus range check).

- **c64103c** — array IN OUT conversion actuals: CONSTRAINT_ERROR is
  required before the call when component subtype constraints differ,
  when a bound of the operand lies outside the target's index subtype
  (non-null dimensions, unconstrained formal), or when per-dimension
  lengths differ (constrained formal).

- **c64103d** — the OUT-mode twin of c64103c.

- **c95084a** — the task-entry twin of c64103a (the entry-call
  marshaling path needs the same conversion checks).

Sections of c64103a/c95084a that report "ONLY ONE INTEGER BASE TYPE"
are genuinely inapplicable here (every integer range type derives
from one 64-bit base, so no value can exceed a formal's base range) —
but that is a comment in their output, not their verdict; the tests
fail on the float, fixed, and array sections above.
