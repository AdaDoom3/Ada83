# Subsystem Survey: Purpose Fidelity, Completeness, and Coupling

An architectural audit of the subsystems in `ada83.c` (~46k lines). Three
questions per subsystem: is it used for its intended purpose, how complete
is it, and how is it coupled to the rest of the compiler. Line numbers
reference the state of the file at the time of this survey; treat them as
anchors, not contracts.

§18 (SIMD) is deliberately out of survey scope: it is not a correctness
concern and no work is planned there.

## 0. Ground rules (standing user direction)

These govern every change made under this survey's recommendations:

- **Correctness is the metric, not pass count.** Judge a change by RM 83
  conformance, valid well-typed IR, and honest representation. A pass-count
  drop can mean a falsely-passing test now fails honestly; that is progress.
  The suite is a landing constraint, not a direction.
- **Class C (and A) are the gates. Class B is excluded** until the
  executable substrate matures — B legality results only matter once the
  code that would run is itself trustworthy.
- **Never revert or delete work without asking.** Every time, no exceptions.
- **No kludges, no parking, no deferring.** When a choice splits into a
  hack and a structural fix, take the structural fix; fix issues when they
  are found.
- **Spec/body harmony.** Forward declarations, types, and globals go in the
  §N spec block; definitions in the §N body block. The § numbers are the
  navigation system; keep them whole.
- **Full identifier names** in new code — no abbreviations.
- **Batch edits before suite runs.** Per-edit full-suite runs waste time;
  single-test sanity checks are fine mid-stream.

## 1. Verdict at a glance

| §   | Subsystem        | Purpose fidelity | Completeness | Notes |
|-----|------------------|------------------|--------------|-------|
| §1  | Foundation       | ✓ clean          | 9/10         | No replacement-character provision (RM 2.10), otherwise complete |
| §2  | Measurement      | ✓ clean          | 10/10        | Minimal, fully used |
| §3  | Memory           | ✓ clean          | 10/10        | Universal allocator, correctly isolated |
| §4  | Text             | ✓ clean          | 10/10        | `Edit_Distance` now powers "did you mean" diagnostics (W2) |
| §5  | Provenance       | ✓ clean          | 8/10         | Works; no source snippets, warnings underused |
| §6  | Arithmetic       | ✓ clean          | 9/10         | Dead `Big_Real` ops deleted (W2); based real literals now fold exactly through Rational (W8) |
| §7  | Lexer            | ✓ clean          | 9/10         | Full Ada 83 lexical grammar incl. `%` strings; no replacement characters |
| §8  | Syntax           | ✓ mostly         | 9/10         | 80/81 node kinds live; `NK_CODE` (machine code insertions, RM 13.8) never produced. The `SYNTAX_TREE_SHAPE_LIST` X-macro is now the total child-edge authority (`_Static_assert` over all kinds) |
| §9  | Parser           | ✓ clean          | 9/10         | Full grammar coverage; basic panic-mode recovery; formal-array definitions now build real `NK_ARRAY_TYPE` detail |
| §10 | Types            | ⚠ entangled      | 8/10         | `Type_To_Rep` puts LLVM lowering policy inside the type system |
| §11 | Names            | ✓ clean          | 9/10         | Single overload-resolution pipeline; cleanest subsystem in the file. Creation-sequence visibility cutoffs added for declaration-point resolution |
| §12 | Semantics        | ✓ clean          | 8/10         | Zero IR emission; rep clauses resolved but not enforced downstream |
| §13 | Code generation  | ✓ mostly         | 9/10         | Strong check/exception/tasking coverage; § numbering re-keyed to the spec scheme (W1) |
| §14 | Library mgmt     | ✗ write-mostly   | 3/10         | ALI files written in full, consumed only for checksum-gated spec reuse |
| §15 | Elaboration      | ✓ mostly         | 8/10         | EDGE_WITH + EDGE_ELABORATE(_ALL) now constructed from context clauses and pragmas; instances join the graph; EDGE_INVOCATION still unbuilt |
| §16 | Generics         | ✓ clean          | 9/10         | Rebuilt as GNAT-model tree-copy instantiation; substitution machinery compiler-proven extinct. Audit/plan/ledger in `docs/generics.md` |
| §17 | File loading     | ✓ mostly         | 8/10         | Include paths, circular-`with` detection, transitive WITH emission, and spec-only package emission all work; standalone subunit compilation is still parse-only |
| §19 | Driver           | ✓ clean          | 9/10         | Orchestration sound; fork workers reset cross-compilation globals (W7) |

Completeness here means "fraction of the subsystem's own mission accomplished,"
not fraction of the Ada 83 RM. A small complete subsystem (§2) outranks a
large ambitious one (§13) on this axis by design.

Suite state at this revision: **class A 140/140 (100%)**, class C 1075/1979
(net +4 across the W-plan: ca-multifile gains from transitive-with, zero
losses at every landing), with the regression discipline that every landing
must diff pass-sets and show zero losses.

## 2. Ranking by completeness

From most to least complete relative to intended purpose:

1. **§2 Measurement, §3 Memory** — small, finished, fully exercised.
2. **§1 Foundation, §4 Text, §7 Lexer, §9 Parser, §8 Syntax, §11 Names,
   §16 Generics** — essentially done; each carries at most one small dead
   or missing item.
3. **§5 Provenance, §12 Semantics, §13 Code generation, §19 Driver** —
   working and load-bearing, with identifiable gaps that do not undermine
   the core mission.
4. **§6 Arithmetic, §10 Types, §17 File loading** — core path works; a
   designed-in capability (dead `Big_Real` ops, clean lowering boundary,
   standalone subunit compilation) was never finished.
5. **§15 Elaboration, §14 Library management** — the most over-built relative
   to what is actually wired up. Both contain serious machinery (Tarjan SCC,
   GNAT-format ALI read/write) whose outputs are mostly unconsumed.

## 3. Per-subsystem findings

### §1–§5: Foundation layers

Used exactly as intended. Every typedef, limit constant, and predicate in §1
has downstream consumers; §2's `Bits_For_Range` / `Bits_For_Modulus` are the
sole authorities on representation width; §3's `Arena_Allocate` is the
universal allocator with `Arena_Free_All` called only at shutdown as
documented; §5 anchors every diagnostic.

Gaps:

- `Edit_Distance` is now LIVE (W2): "did you mean" suggestions fire on
  undefined identifiers (scope-chain scan), unknown record components,
  unknown task entries, and unresolved package members
  (`Closest_Name_Search` accumulator over each site's candidate set;
  distance ≤ 2, short names ≤ 1).

- §5 `Report_Warning` is now the single warning channel. **Directive: use
  warnings for real** — a recoverable condition must neither hard-error
  nor pass silently. Landed as: accept-and-ignore features warn at their
  frontier (W5); the five raw stderr warnings route through
  `Report_Warning` with locations (W10; the `-g`-only internal honesty
  checks stay on stderr by design). The error-side audit (W9) found no
  demotions warranted: every surviving `Report_Error` is an RM-mandated
  legality rejection, and the 16 `Fatal_Error` sites are codegen-time
  internal invariants where continuing would emit wrong IR. (Several
  attribute-arity Fatal_Errors belong in §12 as ordinary semantic errors
  — an improvement, not a demotion.) No source-line snippets in
  diagnostics yet.
- Replacement characters (RM 2.10: `!` for `|`, `:` for `#`, `%` for `"`)
  are partially covered — `%` string delimiters are implemented in §7, the
  other replacements are not.

### §6: Arithmetic

The big-integer layer is complete (Knuth division, GCD, 128-bit conversions)
and live: the lexer parses every numeric literal through it. The rational
layer is **live, not dead**: `Eval_Const_Rational` folds universal-real
expressions through `Rational_Add/Sub/Mul/Div/Pow`, and codegen uses
`Rational_Compare` for exact static float comparisons.

What is actually dead: `Big_Real_Add_Sub`, `Big_Real_Multiply`,
`Big_Real_Scale`, `Big_Real_Divide_Int`, `Big_Real_Compare`,
`Big_Real_To_Hex` — roughly 200 lines marked `__attribute__((unused))`. The
design intent was apparently arithmetic directly on decimal-scaled reals;
the compiler instead converts to `Rational` and computes there, which is the
better model. The dead `Big_Real` arithmetic should be deleted; `Rational`
is the canonical exact domain.

Precision directive (W8/W11) landed: based REAL literals build an exact
`Rational` in the lexer (carried on token and NK_REAL, preferred by
`Eval_Const_Rational`); integer literals with exponents accumulate through
`Big_Integer` (no int64 wrap) and reject minus exponents (RM 2.4.1);
universal integer literals emit at a width covering their VALUE; the
integer `**` runtime helpers run at 64-bit width with an explicit
destination-width overflow check on narrowing (INTEGER'LAST**2 raises,
c45613a). `long double` is extinct from the file. Known remaining gap:
resolution types a qualified `BIG'(2)**60` as universal, so a >32-bit
runtime `**` on a 64-bit type through the universal path raises instead of
computing — a `**` result-typing fix in §12 is the follow-up.

### §7–§9: Lexer, syntax tree, parser

The cleanest pipeline segment in the compiler. The parser is pure syntax —
no symbol lookups, no type decisions; semantic fields on `Syntax_Node`
(`type`, `symbol`, `apply.resolution`, `folded_value`, master fields) are
written exclusively by §12 and never by §13, so the AST/semantics boundary
holds in practice, not just on paper.

§8 gained the `SYNTAX_TREE_SHAPE_LIST` X-macro during the generics rebuild:
a total child-edge table over all node kinds, `_Static_assert`-checked, that
drives `Clone_Subtree` and any future total tree traversal. It is the single
source of truth for node shape.

Grammar coverage is effectively total: all declaration forms, all statement
forms including `abort`/`goto`/entry families/all four `select` forms, all
three representation-clause shapes, all three generic formal kinds (formal
array definitions now construct real `NK_ARRAY_TYPE` detail — index marks,
box/range constraints, component subtype — feeding the RM 12.3.4 matching).
The one hole is `NK_CODE` (enum member at ~876, shape entry at ~1520):
machine code insertions (RM 13.8) have a node kind but no parse production,
no semantics, no codegen. It is a dead enum value and should either be
implemented or removed alongside an explicit "not supported" diagnostic.

Error recovery is panic-mode with ~20 synchronization points plus a
stuck-loop progress check. Adequate, not sophisticated. (Class-B results
would quantify this, but B is out of scope until the executable substrate
matures.)

### §10: Types

`Type_Info` is the single source of structural type truth — semantics and
codegen both read it and neither maintains a shadow representation. The
entanglement is in the other direction: **§10.7 `Type_To_Rep` embeds LLVM
lowering policy inside the type system**. It decides fat versus thin pointer
representation and chases private-type completion chains — codegen concerns
executing inside §10. (The generic-formal resolution it used to perform died
with the substitution machinery; the remaining wart is the lowering policy
itself.)

This is the most consequential layering wart in the front half of the
compiler. The structural fix: `Type_To_Rep` and the fat-pointer helpers
(§10.8) belong in §13; `Type_Info` should answer "what is this type" and
"how many bytes," never "what LLVM type string does this become."

### §11: Names

The best subsystem in the file. Overload resolution is one pipeline —
`Collect_Interpretations` → `Filter_By_Arguments` → `Disambiguate` →
`Resolve_Overloaded_Call` — and every resolution site in §12 funnels through
it; no ad-hoc resolution paths exist. Use-clause visibility is alias-based
and RM-conformant; character literals resolve as enumeration members through
both derivation and subtype chains. `MAX_INTERPRETATIONS` (64) is a
pragmatic cap, not a correctness issue.

Added during the generics rebuild: **creation-sequence visibility cutoffs**
(`Symbol.creation_sequence`, `sm->visibility_cutoff`,
`sm-> cutoff_exempt_scope`) — the `Save_Global_References` equivalent that
gives instance clones declaration-point visibility (RM 12.1(13), 12.3(79)).
Honored in `Symbol_Find`, `Symbol_Find_By_Type`, and
`Collect_Interpretations`.

Single-task name semantics (RM 9.1) are now explicit:
`Single_Task_Object_Denoted` redirects lookup that lands on a single task's
type symbol (the carrier for entries and the body completion) to the
implicit object variable, at both package-member lookup paths.

### §12: Semantics

Verified zero `Emit` calls across the section: semantic analysis never emits
IR. Constant folding lives here, memoized on the node
(`folded_valid`/`folded_value`), and codegen consumes the cache instead of
re-folding. Derived-type operation inheritance, discriminant component
fill-in, and access-type master assignment all happen here, with codegen
reading the results.

The completeness gaps are about enforcement, not analysis:

- **Enumeration representation clauses**: values are parsed and stored into
  `enumeration.rep_values` (~17847) and then **never read by any other
  code**. Codegen uses position numbers throughout. The clause is accepted
  and silently ignored — worse than rejection.
- **Record representation clauses and address clauses**: same shape —
  parsed, not enforced, no diagnostic.
- Renaming follows objects; `Resolve_Subprogram_Rename` now returns terminal
  non-subprogram targets (entries, enumeration literals) instead of stopping
  one link short.

### §13: Code generation

At over half the compiler, the headline finding is positive: codegen does
**not** re-do semantic analysis. Dispatch trusts `node->symbol`/`node->type`
set by §12; the §13.0.1 predicates (`Is_Uplevel_Access`,
`Subprogram_Needs_Static_Chain`, `Operator_Call_Target`) are genuinely used
at 10–20 sites each to keep inline logic deduplicated. The hardcoded
`i8`/`i32` instances that exist are the legitimate ones (byte-addressed
`getelementptr`, struct index operands); data widths route through
`Type_To_Rep`/`LLVM_Rep_To_String`. No dead emit functions found.

Runtime check coverage maps onto all RM 11.7 suppress categories
(`CHK_OVERFLOW/DIVISION/INDEX/LENGTH/ACCESS/DISCRIMINANT/RANGE`), each
suppressible and each implemented with the appropriate LLVM intrinsic or
explicit branch. Recent additions extend this to instantiation-elaboration:
RM 12.3(81) subtype-agreement checks (shape-dispatched over scalar /
fixed-mantissa / float domains, array `Index_Info` constraints, access
designated recursion, record/private discriminants) and generalized
RM 3.5(4)/3.6.1 range-constraint compatibility checks at type and subtype
elaboration (`Emit_Range_Constraint_Compat_Check`). Exception handling is
setjmp/longjmp-based with correct master unwinding on the longjmp path — a
defensible design choice for Ada semantics, not a gap. Tasking codegen
(activation lists, entry calls, master enter/exit) is present and
post-RM-9-work deterministic; task functions now establish their own
nesting context (locals are allocas; enclosing-scope variables arrive via
`%__frame.` aliases) instead of inheriting the enclosing function's frame
flag.

The deferred-bodies queue (nested subprogram/task/instance bodies emitted
after the enclosing function closes) is arena-grown without bound through
one `Defer_Subprogram_Body` helper; the old fixed 64-slot array overflowed
into inline emission — a `define` nested inside the still-open function.

Remaining structural notes:

1. ~~Sub-section numbering drift~~ — FIXED (W1): body §13.x headers are
   content-keyed to the spec scheme (state 13.0, primitives 13.1, checks
   13.2.x, fat pointers 13.3.x, bounds 13.4, exceptions 13.5, expressions
   13.6.x, statements 13.7, declarations 13.8, aggregates 13.9, BIP
   13.10.x, compilation units 13.11); duplicates and the 13.10/13.17
   mismatch are gone.
2. `Emit_Binary_Op_Predefined` (~1,350 lines) stays as is by user
   direction: no splitting without a functional reason.

### §14: Library management

**The largest gap between machinery and use.** `ALI_Write` (§14.5, ~43364)
emits faithful GNAT-format ALI files — V/P/U/W/D/X lines, pragma flags,
elaboration markers, export metadata. `ALI_Reader` (§14.7) parses them back
correctly into a cache. But the only consumer is `Try_Load_From_ALI`, which
reuses a parsed spec only when the checksum matches, and nothing else reads
the dependency or elaboration data:

- Pragma `Elaborate`/`Elaborate_All` flags are recorded into ALI and never
  consulted when building the elaboration graph.
- `LIMITED WITH` detection is an open TODO — correctly so, since Ada 83 has
  no limited with; the field is speculative.
- No incremental-build story: a stale ALI means a full reparse, which is
  also what no ALI means.

As intended-purpose fidelity goes, §14 is write-mostly infrastructure. Either
the elaboration and dependency data should gain consumers (see §15) or the
unconsumed lines should stop being written.

### §15: Elaboration

The algorithmic core — Tarjan SCC, vertex predicates, topological order
extraction — is well built and the computed order **is** consumed: the
driver calls `Elab_Compute_Order` and `@main` emission elaborates units in
graph order with a source-order fallback on cycles.

**Wired (W4).** `Elab_Register_Context_Dependencies` turns every
compilation unit's context clause into edges: `EDGE_WITH` from each WITH'd
unit's spec, `EDGE_ELABORATE`/`EDGE_ELABORATE_ALL` from context pragmas
(which the parser now KEEPS — `context.pragmas` — instead of discarding).
It runs at load time for WITH-loaded specs and bodies (those CUs never
pass through code generation) and at code generation for main-file units.
Library-level generic instantiations register as graph units too, and
@main's graph-ordered elaboration has a no-silent-drop sweep: any
registered `___elab` the graph did not cover still runs, in source order,
after the ordered units. `EDGE_INVOCATION` remains unconstructed (the one
remaining `// ???`).

Within-unit binding elaboration for generic instances is, by contrast,
fully wired: library-level instantiations elaborate their bindings in the
instance's `___elab` function (registered in `cg->elab_funcs`), nested ones
at the instantiation point.

### §16: Generics

(Out of scope for the current work plan — just rebuilt; no further §16
work until something new surfaces.)

**Rebuilt.** The macro-expansion / substitution-at-emission model described
by earlier revisions of this survey is gone — compiler-proven extinct
(declaration deleted, clean build, zero survivors for every old entry
point: `Node_Deep_Clone`, `generic_formal_index`, `g_generic_type_map`,
`Resolve_Generic_Formal`, the `substitution cycle` diagnostics, and the
rest of the disposition table in `docs/generics.md` §8.10).

The current model is GNAT-style tree-copy instantiation:

- `Clone_Subtree` (driven by the §8 shape table) clones the template
  spec+body with full semantic reset.
- Formal bindings become ordinary symbols: formal types are actual-VIEW
  subtypes carrying RM 12.1.2(41–42) predefined-operator rebinding; IN
  objects are per-instance constants; IN OUT objects are per-instance
  pointer cells; formal subprograms are renames or synthesized wrapper
  bodies ("/=", attribute actuals, builtin operators).
- The clone re-resolves under declaration-point visibility
  (creation-sequence cutoffs) and emits through the ordinary paths —
  instance package-level state is per-instance globals even when nested.
- `Validate_Generic_Actuals` enforces RM 12.3.1–12.3.6 at resolution;
  RM 12.3(81) agreement pairs are captured at resolution and emitted at the
  instantiation point; `Instantiating_Set` rejects recursive instantiation.

The §12 ↔ §16 cycle is inherent to instantiation-by-re-resolution and is
contained: the clone-state contract is explicit (the shape table is total;
`Clone_Subtree` resets every semantic field), and codegen has no
generic-specific read paths — emission sees ordinary resolved trees.

Full audit, RM-12 checklist, wiring matrix, and honesty ledger live in
`docs/generics.md`.

### §17: File loading

Include-path search (explicit `-I`, then exe-relative `rts`, input dir, `.`)
and circular-`with` detection via `Loading_Set` both work as designed.
Generic library templates parse through the loader and are cached one parse
per template (`generic_body_cache`); SEPARATE stubs are correctly NOT
treated as template bodies — the subunit completes the template
(`Complete_Instance_Package_Body`). Transitive WITH works end to end:
loaded specs recursively load their own WITHs, bodies queue for emission,
and a SPEC-ONLY package (no body file, or the `.ada` fallback returning
the spec itself) queues its spec so its package-level state is DEFINED in
the module rather than left an undefined extern. The remaining gap:
standalone subunit compilation against a parent context is still
parse-only — the stub and the separate body of a non-generic unit are
never connected.

### §19: Driver

Single-file orchestration (parse → resolve → generate → ALI) is sound.
Multi-file mode forks a worker per file, which gives memory isolation for
free — but the pre-fork globals are not reset per worker: `ALI_Cache`
entries populated by the parent are inherited stale, and `g_elab_graph`
accumulates vertices across whatever the parent loaded. Because each child
is a fresh fork of a parent that has not compiled anything, this is latent
rather than active today, but it is the kind of invariant that breaks
silently when someone adds pre-fork work. Cross-file elaboration is
explicitly out of scope (the user links `.ll` files manually), so per-file
elaboration order is correct behavior, not a bug.

## 4. Dependency and coupling analysis

### The dependency picture

Layering, lowest to highest, with arrows meaning "calls into":

```
§1 Foundation ──────────────┐ (typedefs/limits: used by everyone)
§3 Memory ──────────────────┤ (Arena_Allocate: used by everyone)
§5 Provenance ──────────────┤ (Report_Error: used by everyone)
§2 Measurement ──→ §1
§4 Text ──→ §3
§6 Arithmetic ──→ §3
§7 Lexer ──→ §4 §5 §6
§8 Syntax ──→ §3
§9 Parser ──→ §7 §8 §3 §5
§10 Types ──→ §2 §6        ⚠ also reached *from* §13 via Type_To_Rep
§11 Names ──→ §4 §10
§12 Semantics ──→ §8 §10 §11 §6 §15(register) §16(instantiate) §17(load)
§16 Generics ──→ §8(clone) §12(re-resolve)   (contained cycle with §12)
§17 Loading ──→ §9(parse) §14(ALI read)
§13 Codegen ──→ §8 §10 §12(folded results, suppression) §2 §6
§14 Library ──→ §8 §12(symbol walk)
§15 Elaboration (graph lib; fed by §12, consumed by §19)
§19 Driver ──→ everything (orchestrator)
```

### What is healthy

- **The front pipeline is strictly layered.** §7→§9→§12→§13 has no back
  edges: the parser never looks up symbols, semantics never emits IR
  (verified by scan), codegen never mutates the AST or re-resolves
  overloads. The `Syntax_Node` semantic fields are a disciplined one-way
  handoff: parser writes syntax, §12 writes annotations, §13 reads both.
- **§11 is a clean service.** Overload resolution has one entry point and no
  callers outside §12.
- **Foundation sections (§1–§5) have zero upward dependencies.** Nothing in
  the bottom five sections knows that types, semantics, or codegen exist.
- **§16's cycle with §12 is contained.** Instantiation-by-re-resolution is
  inherently cyclic, but the clone contract is explicit (total shape table,
  full semantic reset) and codegen sees only ordinary resolved trees — no
  generic knowledge smeared into §13.

### Coupling problems, in priority order

1. **§10 → codegen leakage (`Type_To_Rep`, §10.7–§10.8).** LLVM
   representation policy — fat versus thin pointer choice, integer width
   strings — lives inside the type system and is invoked from ~50 codegen
   sites. This inverts the intended relationship: §13 should consume §10's
   structural facts and own all lowering decisions. *Fix: move
   `Type_To_Rep` and the §10.8 fat-pointer helpers into §13.1; `Type_Info`
   keeps sizes in bytes/bits only.* Mechanical (the compiler-as-worklist
   method applies) and removes the only downward-facing codegen knowledge
   in the front half.

2. **§14 remains write-mostly.** §15 now consumes context clauses and
   pragmas directly (W4), so the ALI elaboration records still have no
   consumer; either an incremental-build story grows around them or the
   unconsumed lines stop being written.

3. **Accept-and-ignore features blur the §9/§12/§13 contract.** Enum rep
   clauses, record rep clauses, address clauses, and standalone subunits
   are all parsed and then silently dropped at different depths (§12 stores
   enum rep_values nobody reads; address clauses register markers nobody
   emits). Every such feature should either be implemented through to
   codegen or rejected with a diagnostic at its current frontier. Silent
   acceptance is the worst of the three options for a conformance-tested
   compiler.

4. **Driver global state versus fork-per-file.** `ALI_Cache` and
   `g_elab_graph` are process globals in a design that forks workers.
   *Fix: reset both at the top of the worker, or make compilation state a
   struct the worker owns.* Cheap insurance.

## 5. Dead code inventory

Confirmed by call-site verification (not grep-trusted; each checked, and
re-verified at this revision):

| Item | Location | Disposition |
|------|----------|-------------|
| ~~`Edit_Distance`~~ | — | LIVE as of W2: powers the "did you mean" suggestions |
| ~~`Big_Real` dead ops~~ | — | DELETED (W2), `Big_Real_Fits_Double` included |
| `EDGE_INVOCATION` | edge-kind enum | Constructed nowhere (EDGE_WITH/ELABORATE/ELABORATE_ALL are live as of W4) |
| `enumeration.rep_values` | written at enum rep resolution, zero readers | Clause now WARNS (W5); values still recorded for an eventual implementation |

Explicitly **not** dead, despite appearances: `Rational_*` arithmetic
(consumed by `Eval_Const_Rational` and static float comparison).

Resolved since the original survey: the entire generics substitution layer
(`Node_Deep_Clone`, `generic_formal_index` and every stamper,
`g_generic_type_map`, `Resolve_Generic_Formal`, the `substitution cycle`
diagnostics) — deleted during the §16 rebuild, ~1000 lines, each removal
compiler-verified.

## 6. Recommended order of work

1. ~~Fix §13 sub-section numbering~~ — DONE (W1).
2. ~~Delete dead `Big_Real` arithmetic; decide `Edit_Distance`~~ — DONE
   (W2): Big_Real ops deleted; Edit_Distance wired into "did you mean"
   suggestions at undefined identifiers, record components, task entries,
   and package members (Closest_Name_Search).
3. ~~Move `Type_To_Rep` + fat-pointer type helpers from §10 to §13~~ —
   DONE (W3): Type_To_Rep, Array_Bound_LLVM_Rep, Bounds_Type_For,
   Bounds_Alloc_Size, Float_LLVM_Rep_Of, Float_Is_Single now live in
   §13.0.2; Float_Effective_Digits de-layered to a size test so §10 makes
   no lowering calls.
4. ~~Build the missing elaboration edges~~ — DONE (W4): EDGE_WITH +
   EDGE_ELABORATE(_ALL) from context clauses/pragmas, instances in the
   graph, no-silent-drop sweep, transitive-with and spec-only emission.
5. ~~Decide each accept-and-ignore feature~~ — DONE (W5): record rep
   clauses turned out to be LIVE (component byte_offsets consumed by
   codegen) as are 'SIZE/'ALIGNMENT/'STORAGE_SIZE/'SMALL attribute
   clauses; enum rep clauses, address clauses, any other attribute
   clause, and standalone SEPARATE bodies now WARN at their frontier and
   continue; `NK_CODE` (never producible) deleted from the node-kind enum
   and shape list.
6. ~~Reset driver globals per fork worker~~ — DONE (W7): each fork child
   resets ALI_Cache_Count, the elaboration graph, Loaded_Body_Count, and
   Loading_Packages before compiling. (A former item — splitting
   `Emit_Binary_Op_Predefined` — is dropped by user direction: no code
   splits without a functional reason.)

All numeric capacity caps in the touched subsystems are now named
constants (MAX_ELAB_FUNCTIONS, MAX_LOADED_UNITS, MAX_LOADING_NAMES,
MAX_AGREEMENT_CHECKS, MAX_INSTANCE_REMAPS, MAX_INSTANTIATION_DEPTH,
MAX_ALI_CACHE_ENTRIES) per the no-magic-numbers directive; the
deferred-bodies queue is unbounded.
