# Correctness / Detangle Audit — Open Worklist

State as of this revision. Completed items live in the git log, not here.

## A. Correctness (wrong code or wrong acceptance possible)

1. **`Eval_Const_Numeric` double-based folding** (49 call sites). A
   `double` twin of the §6 exact machinery: integer literals above 2^53
   lose exactness, `mod`/`rem` folding rides doubles. Migrate call sites
   to `Big_Integer`/`Rational` folding; delete the double folder last
   (compiler-as-worklist). Sites feeding legality decisions first.
2. **Codegen "assume already fat" fallback** (near
   `Expression_Produces_Fat_Pointer`, see ledger `ada83.c:29815`): a
   pointer assumed to be a fat pointer when classification falls
   through. Classify totally or fail loudly.
3. **Unknown-bounds family**: `'FIRST/'LAST` unknown arm now reports an
   internal error (this revision); the sibling fallbacks (ledger
   `27207` "unset bounds", `31221` "can't determine bounds", `48612`
   "fat pointer with unknown bounds") still substitute silently — same
   treatment needed.

## B. Half-implemented / remaining failures

4. **21 Class C failures** (rep clauses `cd*`, text_io `ce3*`,
   multi-file elaboration `ca*`, `c87b10a` overloading, `c98003b`
   rendezvous ordering, numerics `c45*`). Re-classification lost with
   the container restart; redo serially: compile+run each, classify
   missing-feature vs wrong-semantics, fix by theme.
5. **Terminate-alternative guards**: parser ledger note says "not yet
   enforced"; codegen stores "at terminate iff open", so likely stale —
   verify with a two-line test, then drop the note.

## C. Purity / detangle

6. **Dispatch consolidation**: 496 scattered `->kind == NK_*` if-tests
   vs 49 switches. Convert hot dispatch chains (semantics + codegen) to
   exhaustive, default-free switches in enum order (`-Wswitch` becomes
   the totality check).
7. **`cg->` mode-flag protocols** (`entry_call_try_mode`,
   `entry_call_delay_expr`, `in_accept_body`, …): set-at-distance state;
   convert to explicit parameters where call structure allows.
8. **Duplicate-logic sweep**: not yet run (normalized-window hashing
   over the emitter is the cheap way; do serially, one pass).

## Method notes

- The comment ledger (`EXTRACTED_COMMENTS.md`, pre-extraction line
  keys) is the confession index: grep it for fallback/assume/not-yet
  before trusting any silent path.
- Dead-function checks must run at `-O0` per arch tier; `-O1` inlining
  and host-only `#ifdef` branches both fake death.
- No parallel whole-file compiles on small boxes: one bounded process
  at a time.
