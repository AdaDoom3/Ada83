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

## Formerly parked, now restored to acats/

c64103a, c64103c, c64103d, and c95084a lived here while type
conversions used as OUT / IN OUT actuals lacked their RM 4.6 / 6.4.1
checks. That work landed — scalar conversion actuals load and store
in the actual's own representation with checked conversions both
ways (subprogram and entry calls alike), and array conversion actuals
run the conversion's component-constraint, index-subtype, and length
checks before the call — so all four are back in `acats/` and pass.
Only the three AVO-withdrawn tests above remain here.
