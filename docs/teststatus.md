# ACATS Class C — Failure & Skip Taxonomy

Baseline snapshot: run `c-20260607-000616-7845` (2026-06-07).

**Baseline totals: 1075 pass / 658 fail / 246 skip of 1979 processed (54%).**

> **Update (same day, post-H1):** H1 and its fallout are FIXED — suite now
> **1126 pass / 590 fail / 263 skip (56%)**, +51, zero regressions in the
> final run. Landed:
>
> 1. The `{ i64, i64 }` bounds emit at the global string-constant site now
>    derives the bounds struct from the array's index rep via
>    `Bounds_Type_For` (was hardcoded i64 while consumers read i32).
> 2. Slice assignment no longer silently no-ops when the array prefix is
>    not a plain identifier — the old code `return`ed on
>    `prefix->symbol == NULL`, so `R.NAME(1..N) := S` and
>    `BUFFERS(I)(P) := ITEM` were dropped entirely; destination addressing
>    now goes through `Generate_Lvalue`.
> 3. The REPORT stub's `LEGAL_FILE_NAME` is now the real ACATS function
>    form (`FUNCTION LEGAL_FILE_NAME (X : FILE_NUM := 1; NAM : STRING :=
>    "") RETURN STRING`), generating per-test unique file names from the
>    test name — the old constant made all parallel CE tests collide on
>    one shared `test_file.tmp`.
> 4. `rts/sequential_io.adb` buffer positions are 1-based as the
>    `POSITIVE`-indexed buffer requires (CREATE/OPEN/RESET set POS=0, so
>    every WRITE raised CONSTRAINT_ERROR once bug 2 stopped eating the
>    statement), and `END_OF_FILE` is position-derived with the RM 14.2.2
>    MODE_ERROR check.
>
> New compiler bug found while fixing: chained `&` concatenation with a
> CHARACTER operand (`'X' & D & NAM & ".TMP"`) miscompiles — the last
> operand arrives as garbage. Worked around in the stub with a build
> buffer; needs its own fix in the n-ary concatenation lowering.
>
> The CE-family counts below describe the PRE-fix state. Remaining ce
> failures are genuine text_io/sequential_io/direct_io semantic gaps
> (STATUS_ERROR on re-open of an open file — needs FILE_TYPE to become a
> default-initialized record instead of `NEW INTEGER` — RESET/mode rules,
> temp-file semantics, enumeration/float/fixed IO formatting).

Raw harness buckets (baseline):

| Bucket | Count | Meaning |
|---|---|---|
| FAILED | 430 | ran to completion, REPORT printed FAILED |
| RUNTIME | 228 | nonzero exit (unhandled exception or crash) |
| BIND | 129 | `llvm-link` rejected the emitted IR |
| COMPILE | 114 | compiler rejected the source |
| N/A | 3 | NOT APPLICABLE by the test's own report |

RUNTIME death signatures (from `.out` logs):

| Signature | Count |
|---|---|
| Unhandled `TEXT_IO.NAME_ERROR` | 81 |
| Unhandled `CONSTRAINT_ERROR` | 75 |
| lli stack dump (segfault) | 52 |
| Silent death / empty output | 9 |
| Unhandled `DIRECT_IO.END_ERROR`, `MODE_ERROR`, `TASKING_ERROR`, misc | 11 |

BIND reject kinds (from `.link` logs):

| Kind | Count |
|---|---|
| `use of undefined value '%local'` | 54 |
| `use of undefined value '@global'` | 33 |
| `%tN defined with type X but expected Y` | 25 |
| `redefinition of global` | 9 |
| other (void result, integer-constant type, parse) | 8 |

The groups below cut across these buckets by root cause, split into the two
requested categories.

---

## Category A — High-leverage fixes

Small, well-scoped changes whose blast radius is a large test group.

### H1. Bounds-struct ABI bug in global string constants — FIXED

**Was the single biggest lever in the suite.**

The global string-constant emitter hardcoded the bounds struct width:

```llvm
@report__legal_file_name.bounds = linkonce_odr constant { i64, i64 } { i64 1, i64 13 }
```

while every consumer reads bounds through `Bounds_Type_For` — `{ i32, i32 }`
for STRING (INTEGER index) — and does `getelementptr { i32, i32 }` +
`load i32`. On little-endian, `i64 1` read as two i32 yields FIRST=1, LAST=0
→ computed length 0.

Consequence: `REPORT.LEGAL_FILE_NAME` arrived **empty** in every test that
imported it: 81 unhandled `NAME_ERROR`, ~38 ce `CONSTRAINT_ERROR`, a large
slice of the 82 ce FAILED. Fixed together with items 2–4 in the update note
above; net +51 with zero regressions.

### H2. Report stub deficiencies + address clauses — MOSTLY FIXED

- ~~`LEGAL_FILE_NAME : CONSTANT STRING`~~ — now the real function form (see
  update note).
- ~~`VARIABLE_ADDRESS : CONSTANT INTEGER := 0`~~ — removed from the report
  stub (it homograph-poisoned cd5 resolution against SPPRT13's). SPPRT13's
  address constants are now the addresses of real backing variables in
  FCNDECL (`fcndecl.ads`/`.adb`), so overlays land on storage that exists.
- ~~`FOR X USE AT addr` not applied~~ — **implemented** (RM 13.5): the
  overlaid object owns no storage; a hidden `X__at` cell (an access value)
  holds the clause address, evaluated at the clause's elaboration point,
  and the object redirects through `renamed_object` to `CELL.ALL`, so
  reads, writes, `'ADDRESS`, and the declared initialization all flow
  through the established dereference paths. Verified: true aliasing both
  directions, local and package-level objects (ov/ov2 repros), and
  **cd5014a FAILED → PASSED**.
- Found and fixed along the way: USE-visible aliases did not follow
  `renamed_object` redirects (peel `aliased` chains at the redirect
  funnels); `SYSTEM.ADDRESS` was 32-bit (`NEW INTEGER`) while `'ADDRESS`
  emits i64 — now a 64-bit range type.

REMAINING (owned by the in-flight library/package-loading rework): a
loaded **bodyless spec** with non-static initializers (SPPRT13's constants
call FCNDECL functions) gets no `___elab` — the pending-global-initializer
queue is only drained by package-*body* codegen — so the address constants
stay 0 and cd5011\*/cd5012\*/cd5013\* overlay NULL and crash. Fix shape: in
the loaded-units loop, wrap `Emit_Pending_Global_Initializers()` in a
spec-owned `@<pkg>___elab` and register it with the elaboration graph when
the loaded unit is `NK_PACKAGE_SPEC` with pending inits.

### H3. Nested-frame up-level addressing — LARGELY FIXED (same session, suite 1142→1148 then confounded by concurrent ABI churn)

Landed, each verified on its member tests with zero pass-regressions:

1. **Access sites used `Emit_Symbol_Ref` (raw name) instead of
   `Emit_Symbol_Storage` (uplevel-aware)** — record-assignment memcpy +
   discriminant-check GEP + dynamic-array store + `'ACCESS`/
   `'UNCHECKED_ACCESS`. Fixed c41303m/u and kin.
2. **Implicit parameterless calls (RM 4.1.3) treated as storage**: `F.X`
   (component of function result), `F(I)`/access-returning prefixes, `F.E`
   and `F.ALL.E` (entry call on function-result task). Fixed by routing
   prefix evaluation through `Generate_Identifier`/`Generate_Expression`
   in `Generate_Lvalue`, `Generate_Composite_Address` (incl. deleting the
   symbol fast-paths in the implicit-deref/.ALL/access-to-array arms —
   `Generate_Expression` handles variables, renames, aliases, and implicit
   calls uniformly), and `Emit_Task_Pointer_From_Selected`. Fixed
   c41301a/c41306b/c41306c and advanced the rest of c413\* into runtime.
3. **Multi-level up-level references from task bodies / package-body
   subunits**: `Emit_Parent_Frame_Aliases` only aliased ONE parent level;
   the static-chain walk lived duplicated inside `Emit_Function_Header`.
   Unified: the helper now walks every enclosing level chasing the static
   chain; the header calls it. Fixed c94007a/b (task body referencing a
   grandparent-frame task object).

Remaining in this family: task bodies referencing the task OBJECT of
their own type when no alias was emitted at all (c91004c, c95067a —
the object symbol may not be kind-VARIABLE, or enclosing-subprogram
resolution misses), and `%__frame_base` referenced in a function that
never allocated one (c85016a, via a renamed-subprogram call carrying a
static-link argument into a frameless function).

(Original finding below, kept for reference.)

#### Original: 54 BIND rejects

All 54 `use of undefined value '%local'` link errors. Confirmed mechanism in
c41303m: a nested procedure assigns up-level `REC_VAR0`; the nested function
correctly materializes the frame slot

```llvm
%__frame.rec_var0_s158 = getelementptr i8, ptr %__parent_frame, i64 104
```

but the record-assignment memcpy then references the **outer function's**
register name:

```llvm
call void @llvm.memcpy...(ptr %rec_var0_s158, ...)   ; undefined here
```

Two sub-shapes: (a) up-level reference emitted with the outer register name
instead of the `__frame.` slot (record/composite assignment path confirmed);
(b) the `__frame.X` slot itself never emitted (`%__frame.s`,
`%__frame.rec_oops`, `%__frame.flow_count` undefined — likely two-level
nesting or declare-block frames).

Families: c413\* (9), c850\* (6, renamings of up-level objects), c641\* (6),
c340\* (6), c950\* (accept bodies are nested scopes), c372\*, c940\*, c640\*.
One fix in the up-level address-resolution path likely clears most of the 54.

Related fixed instance: the slice-assignment path's silent symbol-gate drop
(update note item 2) was the same "non-identifier prefix" blind spot on the
assignment side.

### H4. NK_RANGE (node kind 15) reaching expression codegen — 9 tests

`INTERNAL ERROR: unsupported expression kind 15 in codegen` = `NK_RANGE`
arriving at `Generate_Expression`. All are slice contexts: c85006a–e
(renamed slices, e.g. `XAI1 : ARR_INT RENAMES AI1(1..3)`), c41201d,
c41203a/b (slices of function results / renamed arrays), c87b24a. One
missing slice/range arm in the apply-or-rename lowering.

### H5. Copy-back constraint check after RETURN — 10 tests, one mechanism

c64104h/j/k/l + c95085h/j/k/l/n/o, all "EXCEPTION NOT RAISED AFTER RETURN":
the copy-back of `out`/`in out` actuals must constraint-check **at the call
site after normal return** (RM 6.4.1), and the same applies to entry-call
copy-back. The check is currently absent. One mechanism in the call-return
sequence; the c95085 members also overlap P5.

### H6. Enum-rep-clause array indexing — 11 tests

cd3014a/b/d/e + cd3015a/b/d/g/i/j/l, all "INDEXING ARRAY FAILURE": arrays
indexed by an enumeration type with a representation clause. Index
computation mixes representation values with positions. Single mechanism in
index lowering.

### H7. Mid-size coherent groups worth a sweep each

- **Array comparison semantics** (~8): c45262b/d, c45264a/b/c, c45265a —
  lexicographic `<`/`<=` on arrays, bounds-insensitive `=` for
  same-length/different-bounds, null-array edge cases.
- **Access equality** (3): c45281a, c45282a/b — null access and access
  comparison results wrong.
- **`'SIZE`/`'SMALL` attribute results** (part of P3 but cheap subset):
  cd1009l, cd1c03a/f — attribute *queries* returning default rather than
  clause-specified values, even before layout is honored.
- **Chained `&` with CHARACTER operand** (new, found post-H1): last operand
  of `'X' & D & NAM & ".TMP"` miscompiles to garbage. Repro is trivial;
  scope unknown — audit the n-ary concat lowering.

---

## Category B — Real hard problems

Structural gaps where one subsystem owns a large family. Fixing any of these
is a design-level effort but moves a big block.

### P1. Derived types (c34\*) — ~45 tests across every bucket

15 BIND (incl. most of the 25 `defined with type X but expected Y`
mislabels), 8 unhandled CONSTRAINT_ERROR, 7 segfaults (c34005g/j/p/r/s/u,
c34007m), 19 FAILED, 1 internal error (c34007d, kind 15). Two distinct roots
visible:

- **Inherited subprograms not emitted**: `use of undefined value
  '@p__f_s162'` across c34014a/c/e/g/h/l — a derived type's inherited
  operation references the parent's function symbol, which is never
  emitted under the derived name (or at all).
- The known **dual-`Type_Info` size discrepancy** (tracked as blocked in
  the type-threading notes) — derived scalar/composite reps disagree
  between the two `Type_Info`s for parent/derived.

This is the derivation model itself: inherited-op symbol strategy + one
representation per derivation chain.

### P2. Discriminated-record semantics (c37\*, c52009/011) — ~35 tests

Missing *checks*, not codegen lies: elaboration-time discriminant checks on
types/subtypes with access components (c37211d/e), assignment
discriminant-compatibility ("RECORD ASSIGNED VALUE WITH DIFFERENT
DISCRIMINANT VALUE" c52009a/b, c37207a; "NON-NULL ASSIGNMENT MADE BETWEEN
TWO VARIABLES OF DIFFERENT CONSTRAINTS" c52011a/b), attempted discriminant
change (c37206a), `'CONSTRAINED` attribute (c37208b, c45273a), default
discriminant values (c37208a), discriminant-dependent component bounds
(c37010a/b, c37102b). Plus 3 segfaults (c37213g/h, c37215d). Needs a
coherent discriminant-check pass in assignment/elaboration rather than
per-site patches.

### P3. Representation clauses (cd1\*–cd4\*) — ~54 FAILED

Rep clauses largely parsed-and-ignored. Test evidence: "CHECK ON
REPRESENTATION OF TYPE … FAILED" (unchecked-conversion-based bit checks),
"INCORRECT REPRESENTATION FOR R1.B2" (record component clauses),
`DERIVED_TYPE'SIZE /= 5. ACTUAL SIZE IS 8` (size clauses), `'SMALL` clauses
(cd1c03f), enum rep (cd2a\*, plus H6's indexing). This is layout control in
§13 codegen: honoring `'SIZE`, record rep specs, and enum rep in actual
storage, not just in attribute answers.

### P4. Overload resolution contexts (c87\*) — 21 tests

RM 8.7 context-driven resolution completeness: operands of `'POS`/`'VALUE`/
`'IMAGE`/`'PRED`/`'SUCC` (c87b07a/c/d/e), overloaded enumeration literals
(c87b04c), number declarations (c87b03a), infix operators (c87b02a),
general "RESOLUTION INCORRECT" (c87a05a). §11/§12 resolution-engine work —
the wrong interpretation is being picked, not rejected.

### P5. Tasking semantics residue (c95\*, c9a\*, c94021a) — ~25 tests

Post-RM-9-implementation tail: conditional/timed entry-call semantics
(c95065a/b/e "ACCEPT E1 EXECUTED" — the call proceeded when it must not),
entry out-param copy-back on exception (c95072b), parameter association in
entries (c95082g, c95071a), exception propagation out of rendezvous
(c95040d), constraint checks in entry calls (c95008a), premature termination
(c9a\* "TN PREMATURELY TERMINATED"), and the known c94021a function-result
task master migration segfault.

### P6. Big-array / slice-assignment torture (c52103\*, c52104\*) — 6 segfaults + slice fails

c52103k/l/m/r/x, c52104x are the giant-array tests (bounds near
`INTEGER'LAST`, multi-GB objects). Need length arithmetic carried in i64
end-to-end and degenerate/null-range handling; byte-level fixes won't hold.
Related correctness (not crash): c52103c "SLICE ASSIGNMENT NOT CORRECT",
c52102a/c overlapping-slice values.

### P7. Separate compilation / multi-file units (ca\*) — ~15 compile rejects

`undefined identifier 'CA1013A3'`, `'CA1020D_PROC1'`, `'C86004C01'` …:
SEPARATE subunits and multi-fragment library-unit scenarios beyond what
single-file compilation + include-path discovery resolves. Part harness
(fragments are skipped by design), part compiler (subunit support).

### P8. text_io / sequential_io / direct_io semantic depth (post-H1 residue)

What remains of the CE family after H1: STATUS_ERROR on operations against
already-open or never-opened files (needs `FILE_TYPE` to become a
default-initialized record — currently `NEW INTEGER`, so a fresh file
variable is indistinguishable from a valid index), RESET mode-transition
rules, temp-file (empty-name CREATE) semantics, enumeration/float/fixed IO
formatting (ce38\*/ce39\* blocks), DIRECT_IO positioning edge cases.
Steady per-package rts work; the harness now reports these honestly.

---

## Crosscutting notes

- ~25 remaining BIND rejects are the known **type-threading long tail**
  (`i8↔i32`, `ptr↔{ptr,ptr}` mislabels concentrated in c34\*/c36\*/c41325a)
  — matches the standing "steady per-site work, no big lever" verdict.
- COMPILE-reject leftovers outside the groups above are a long tail of
  name-resolution one-offs (selected components through renames/use-clauses:
  c86004\*, c83\*, c94008c/d "no entry in task type" for entry families) and
  two genuine parser gaps (`expected ), got RANGE` ×3 — `'RANGE` inside an
  index constraint).
- CE tests create their external files in the harness CWD (repo root) as
  `X<n><TESTNAME>.TMP`; `.gitignore` covers the pattern.

## Suggested order

1. ~~**H1**~~ — DONE (+51 with the fallout fixes).
2. **H2 (VARIABLE_ADDRESS) + H3** — independent of each other; H3 is pure
   compiler, H2 mostly harness + the `USE AT` clause.
3. Then pick a Category B subsystem by appetite: P1 (derived types) and P2
   (discriminant checks) have the highest test counts; P3 (rep clauses) is
   the most self-contained; P8 (rts I/O depth) is the lowest-risk steady
   grind.
