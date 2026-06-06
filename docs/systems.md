# Subsystem Survey: Purpose Fidelity, Completeness, and Coupling

An architectural audit of the nineteen subsystems in `ada83.c` (~46k lines).
Three questions per subsystem: is it used for its intended purpose, how
complete is it, and how is it coupled to the rest of the compiler. Line
numbers reference the state of the file at the time of this survey; treat
them as anchors, not contracts.

## 1. Verdict at a glance

| §   | Subsystem        | Purpose fidelity | Completeness | Notes |
|-----|------------------|------------------|--------------|-------|
| §1  | Foundation       | ✓ clean          | 9/10         | No replacement-character provision (RM 2.10), otherwise complete |
| §2  | Measurement      | ✓ clean          | 10/10        | Minimal, fully used |
| §3  | Memory           | ✓ clean          | 10/10        | Universal allocator, correctly isolated |
| §4  | Text             | ✓ mostly         | 9/10         | `Edit_Distance` is dead code — built for "did you mean" diagnostics that were never wired up |
| §5  | Provenance       | ✓ clean          | 8/10         | Works; no source snippets, warnings underused |
| §6  | Arithmetic       | ✓ mostly         | 7/10         | Rational arithmetic is live (constant folding); ~6 `Big_Real` operations are dead |
| §7  | Lexer            | ✓ clean          | 9/10         | Full Ada 83 lexical grammar incl. `%` strings; no replacement characters |
| §8  | Syntax           | ✓ mostly         | 9/10         | 80/81 node kinds live; `NK_CODE` (machine code insertions, RM 13.8) never produced |
| §9  | Parser           | ✓ clean          | 9/10         | Full grammar coverage; basic panic-mode recovery |
| §10 | Types            | ⚠ entangled      | 8/10         | `Type_To_Rep` puts LLVM lowering policy inside the type system |
| §11 | Names            | ✓ clean          | 9/10         | Single overload-resolution pipeline; cleanest subsystem in the file |
| §12 | Semantics        | ✓ clean          | 8/10         | Zero IR emission; rep clauses resolved but not enforced downstream |
| §13 | Code generation  | ✓ mostly         | 8/10         | Strong check/exception/tasking coverage; internal numbering drift; one 6k-line function |
| §14 | Library mgmt     | ✗ write-mostly   | 3/10         | ALI files written in full, consumed only for checksum-gated spec reuse |
| §15 | Elaboration      | ✗ under-wired    | 4/10         | Tarjan machinery solid, but only one edge kind is ever constructed |
| §16 | Generics         | ⚠ fragile        | 5/10         | Clone-then-re-resolve works for the common case; nested/separate cases unhandled |
| §17 | File loading     | ✓ mostly         | 7/10         | Include paths and circular-`with` detection work; subunits are parse-only |
| §18 | SIMD             | ✓ clean          | 8/10         | Genuinely hooked into lexer hot paths; ARM64 NEON coverage thinner than x86 |
| §19 | Driver           | ✓ mostly         | 8/10         | Orchestration sound; global state not reset across fork-per-file workers |

Completeness here means "fraction of the subsystem's own mission accomplished,"
not fraction of the Ada 83 RM. A small complete subsystem (§2) outranks a
large ambitious one (§13) on this axis by design.

## 2. Ranking by completeness

From most to least complete relative to intended purpose:

1. **§2 Measurement, §3 Memory** — small, finished, fully exercised.
2. **§1 Foundation, §4 Text, §7 Lexer, §9 Parser, §8 Syntax, §11 Names** —
   essentially done; each carries one small dead or missing item.
3. **§5 Provenance, §12 Semantics, §13 Code generation, §18 SIMD, §19 Driver** —
   working and load-bearing, with identifiable gaps that do not undermine the
   core mission.
4. **§6 Arithmetic, §10 Types, §17 File loading** — core path works; a
   designed-in capability (dead `Big_Real` ops, clean lowering boundary,
   subunit compilation) was never finished.
5. **§16 Generics** — the macro-expansion model works for flat instantiations
   but the re-resolution strategy is structurally fragile.
6. **§15 Elaboration, §14 Library management** — the most over-built relative
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

- `Edit_Distance` (§4, defined at 3615) has zero callers. It was built for
  spelling-suggestion diagnostics that were never added. Either wire it into
  the undefined-identifier path in §11/§12 or delete it.
- §5 has `Report_Warning` but it is rarely used; most recoverable conditions
  either error or pass silently. No source-line snippets in diagnostics.
- Replacement characters (RM 2.10: `!` for `|`, `:` for `#`, `%` for `"`)
  are partially covered — `%` string delimiters are implemented in §7, the
  other replacements are not.

### §6: Arithmetic

The big-integer layer is complete (Knuth division, GCD, 128-bit conversions)
and live: the lexer parses every numeric literal through it. The rational
layer is **live, not dead**: `Eval_Const_Rational` (§12, ~10859) folds
universal-real expressions through `Rational_Add/Sub/Mul/Div/Pow`
(10918–10929), and codegen uses `Rational_Compare` for exact static float
comparisons (22651).

What is actually dead: `Big_Real_Add_Sub`, `Big_Real_Multiply`,
`Big_Real_Scale`, `Big_Real_Divide_Int`, `Big_Real_Compare`,
`Big_Real_To_Hex` (declarations 582–597, bodies 3939–4070) — roughly 200
lines marked `__attribute__((unused))`. The design intent was apparently
arithmetic directly on decimal-scaled reals; the compiler instead converts
to `Rational` and computes there, which is the better model. The dead
`Big_Real` arithmetic should be deleted; `Rational` is the canonical exact
domain.

One genuine precision gap: based real literals (`16#1.8#E+1`) are folded
through `long double` in `Scan_Number` (~4529) rather than `Big_Real`,
losing exactness for pathological cases.

### §7–§9: Lexer, syntax tree, parser

The cleanest pipeline segment in the compiler. The parser is pure syntax —
no symbol lookups, no type decisions; semantic fields on `Syntax_Node`
(`type`, `symbol`, `apply.resolution`, `folded_value`, master fields) are
written exclusively by §12 and never by §13, so the AST/semantics boundary
holds in practice, not just on paper.

Grammar coverage is effectively total: all declaration forms, all statement
forms including `abort`/`goto`/entry families/all four `select` forms, all
three representation-clause shapes, all three generic formal kinds. The one
hole is `NK_CODE` (enum member at 876): machine code insertions (RM 13.8)
have a node kind but no parse production, no semantics, no codegen. It is a
dead enum value and should either be implemented or removed alongside an
explicit "not supported" diagnostic.

Error recovery is panic-mode with ~20 synchronization points plus a
stuck-loop progress check. Adequate, not sophisticated; class-B ACATS
results back this up.

### §10: Types

`Type_Info` is the single source of structural type truth — semantics and
codegen both read it and neither maintains a shadow representation. The
entanglement is in the other direction: **§10.7 `Type_To_Rep` (7982–8053)
embeds LLVM lowering policy inside the type system**. It decides fat versus
thin pointer representation, resolves generic formals on the fly
(`Resolve_Generic_Formal` call at ~7997), and chases private-type completion
chains — all of which are codegen or semantics concerns executing inside §10.

This is the single most consequential layering wart in the front half of the
compiler. The structural fix: `Type_To_Rep` and the fat-pointer helpers
(§10.8) belong in §13; `Type_Info` should answer "what is this type" and
"how many bytes," never "what LLVM type string does this become." Generic
formal resolution at rep-mapping time means the question "what is this
type's representation" has a context-dependent answer, which is exactly what
type freezing (§10.6) exists to prevent.

### §11: Names

The best subsystem in the file. Overload resolution is one pipeline —
`Collect_Interpretations` → `Filter_By_Arguments` → `Disambiguate` →
`Resolve_Overloaded_Call` (8796–9026) — and every resolution site in §12
funnels through it; no ad-hoc resolution paths exist. Use-clause visibility
is alias-based and RM-conformant; character literals resolve as enumeration
members through both derivation and subtype chains. `MAX_INTERPRETATIONS`
(64) is a pragmatic cap, not a correctness issue.

### §12: Semantics

Verified zero `Emit` calls across the entire section (9274–16836): semantic
analysis never emits IR. Constant folding lives here, memoized on the node
(`folded_valid`/`folded_value`), and codegen consumes the cache instead of
re-folding. Derived-type operation inheritance (§12.4.1), discriminant
component fill-in, and access-type master assignment all happen here, with
codegen reading the results.

The completeness gaps are about enforcement, not analysis:

- **Enumeration representation clauses**: values are parsed and stored into
  `enumeration.rep_values` (16744–16754) and then **never read by any other
  code**. Codegen uses position numbers throughout. The clause is accepted
  and silently ignored — worse than rejection.
- **Record representation clauses and address clauses**: same shape —
  parsed, not enforced, no diagnostic.
- Renaming follows objects but the subprogram-rename chain is only partially
  peeled (codegen's `Operator_Call_Target` does some of this work that
  semantics could finish).

### §13: Code generation

At ~26k lines this is over half the compiler. The headline finding is
positive: codegen does **not** re-do semantic analysis. Dispatch trusts
`node->symbol`/`node->type` set by §12; the §13.0.1 predicates
(`Is_Uplevel_Access`, `Subprogram_Needs_Static_Chain`,
`Operator_Call_Target`) are genuinely used at 10–20 sites each to keep
inline logic deduplicated. The hardcoded `i8`/`i32` instances that exist are
the legitimate ones (byte-addressed `getelementptr`, struct index operands);
data widths route through `Type_To_Rep`/`LLVM_Rep_To_String`. No dead emit
functions found.

Runtime check coverage maps onto all RM 11.7 suppress categories
(`CHK_OVERFLOW/DIVISION/INDEX/LENGTH/ACCESS/DISCRIMINANT/RANGE`), each
suppressible and each implemented with the appropriate LLVM intrinsic or
explicit branch. Exception handling is setjmp/longjmp-based with correct
master unwinding on the longjmp path — a defensible design choice for Ada
semantics, not a gap. Tasking codegen (activation lists, entry calls,
master enter/exit) is present and post-RM-9-work deterministic.

Two structural problems:

1. **Sub-section numbering has drifted.** Body has two sections labeled
   §13.2.1 (18890, 19470) and two labeled §13.2.2 (19703, 20155); the
   build-in-place protocol is §13.10 in the spec half (3078) but §13.17 in
   the body (42321). Since the § numbers are the file's navigation system,
   this is a real maintenance hazard, and cheap to fix.
2. **`Emit_Binary_Op_Predefined` is ~6k lines** — one switch covering every
   predefined operator across every type class. The decomposition elsewhere
   in §13.3 (separate `Generate_Lvalue`, `Generate_Aggregate`,
   `Generate_Selected`, …) shows the file knows how to factor; this function
   missed the treatment.

Minor: generic-formal lvalue substitution logic is duplicated between
`Generate_Lvalue` (~20314) and the assignment path (~31344) instead of being
one predicate-style helper.

### §14: Library management

**The largest gap between machinery and use.** `ALI_Write` (42847+) emits
faithful GNAT-format ALI files — V/P/U/W/D/X lines, pragma flags,
elaboration markers, export metadata. `ALI_Reader` (43015+) parses them back
correctly into a cache. But the only consumer is `Try_Load_From_ALI`
(44355–44392), which reuses a parsed spec only when the checksum matches,
and nothing else reads the dependency or elaboration data:

- Pragma `Elaborate`/`Elaborate_All` flags are recorded into ALI
  (42904–42905) and never consulted when building the elaboration graph.
- `LIMITED WITH` detection is an open TODO (42610) — correctly so, since
  Ada 83 has no limited with; the field is speculative.
- No incremental-build story: a stale ALI means a full reparse, which is
  also what no ALI means.

As intended-purpose fidelity goes, §14 is write-mostly infrastructure. Either
the elaboration and dependency data should gain consumers (see §15) or the
unconsumed lines should stop being written.

### §15: Elaboration

The algorithmic core — Tarjan SCC (43387+), vertex predicates, topological
order extraction — is well built and the computed order **is** consumed: the
driver calls `Elab_Compute_Order` and `@main` emission elaborates units in
graph order with a source-order fallback on cycles (45840–45870).

The problem is the graph's edge set. `Elab_Add_Edge` is called from exactly
**one** site (43766), constructing only `EDGE_SPEC_BEFORE_BODY`. The edge
kinds `EDGE_WITH`, `EDGE_ELABORATE`, `EDGE_ELABORATE_ALL`, and
`EDGE_INVOCATION` are declared (3285–3289, their comments literally read
`// ???`), participate in cycle classification (43471), and are never
created. So the "dependency graph" currently encodes only the spec-before-
body rule; `with`-driven ordering works only insofar as source order happens
to be right, and pragma `Elaborate`/`Elaborate_All` are no-ops end to end
(parsed in §9, recorded in §14, never reaching §15).

This is half an implementation wearing a whole implementation's type
definitions. The wiring work — add `EDGE_WITH` edges when resolving context
clauses, add `EDGE_ELABORATE*` edges from the pragmas — is small relative to
the machinery already in place.

### §16: Generics

Macro-style expansion as advertised: `Node_Deep_Clone` (43855+) copies the
template substituting formals via `generic_formal_index` slots, then the
clone is re-resolved through the ordinary §12 entry points. This creates the
one genuine cycle in the dependency graph (§12 → §16 → §12), which is
inherent to the macro model and acceptable — but it makes the clone-state
contract critical: symbols are nulled on cloned nodes and everything must be
re-derivable. Cases where it is not:

- Nested generic instantiations (a generic instantiating another generic)
  have no recursion handling.
- Generic units with `separate` bodies are not connected post-clone.
- Formal subprogram defaults (`is <>`) resolve names but are not exercised
  deeply; the two known `unsupported: generic formal object substitution
  cycle` diagnostics (20322, 20663) mark where the substitution model hits
  its limits.

The deeper architectural note: `generic_formal_index` is stamped at parse
time, substituted at clone time, and consulted at codegen time
(`Generate_Lvalue`, `Underlying_Record_Type`). Three phases sharing one
integer slot on the node is workable but means generic knowledge is smeared
across §9, §12, §13, and §16 rather than contained in §16.

### §17: File loading

Include-path search (explicit `-I`, then exe-relative `rts`, input dir, `.`)
and circular-`with` detection via `Loading_Set` both work as designed.
Subunits are the gap: `separate` bodies parse (`is_separate` is set) but
there is no mechanism to compile a subunit against its parent context — the
stub and the separate body are never connected. Parse-only support for a
language feature is the same accept-and-ignore pattern as the rep clauses.

### §18: SIMD

Genuinely used, not decorative: `Simd_Skip_Whitespace`, `Simd_Find_Newline`,
and `Simd_Scan_Identifier` sit directly in the lexer hot loops (4396, 4410,
4429), and `Simd_Parse_Digits_Avx2` **is** live — it is the batch path
inside `Big_Integer_From_Decimal_SIMD` (45712), parsing 8/16-digit chunks.
Runtime AVX-512/AVX2 detection gates the x86 paths. The asymmetry: ARM64
gets NEON for the scanning functions but the decimal batch path is
x86-only, and the scalar fallback's 4-byte unrolling is unbenchmarked
folklore. Low risk either way.

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
§6 Arithmetic ──→ §3, §18
§18 SIMD (leaf; called by §6, §7)
§7 Lexer ──→ §4 §5 §6 §18
§8 Syntax ──→ §3
§9 Parser ──→ §7 §8 §3 §5
§10 Types ──→ §2 §6        ⚠ also reached *from* §13 via Type_To_Rep
§11 Names ──→ §4 §10
§12 Semantics ──→ §8 §10 §11 §6 §15(register) §16(instantiate) §17(load)
§16 Generics ──→ §8(clone) §12(re-resolve)   ⚠ CYCLE with §12
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
- **§18 is a textbook leaf**: dispatched at compile time, detected at
  runtime, callable only from §6/§7.

### Coupling problems, in priority order

1. **§10 → codegen leakage (`Type_To_Rep`, §10.7–§10.8).** LLVM
   representation policy — fat versus thin pointer choice, integer width
   strings, generic-formal resolution at lowering time — lives inside the
   type system and is invoked from ~50 codegen sites. This inverts the
   intended relationship: §13 should consume §10's structural facts and own
   all lowering decisions. *Fix: move `Type_To_Rep` and the §10.8 fat-pointer
   helpers into §13.1; `Type_Info` keeps sizes in bytes/bits only.* This is
   mechanical (the compiler-as-worklist method applies) and removes the only
   downward-facing codegen knowledge in the front half.

2. **§12 ↔ §16 instantiation cycle.** Inherent to macro-style generics, so
   the goal is containment, not elimination: the clone-state contract (which
   node fields must be null, which survive cloning, who re-stamps
   `generic_formal_index`) is currently implicit across four sections. The
   two `substitution cycle` diagnostics are symptoms. *Fix: document and
   assert the clone invariant in §16; move the codegen-side formal
   substitution (duplicated in `Generate_Lvalue` and assignment) into one
   §13.0.1-style predicate so §16's model has exactly one consumer in
   codegen.*

3. **§14/§15 are connected on paper, not in code.** ALI files carry
   elaboration pragmas that the elaboration graph never sees; the graph
   declares four edge kinds it never builds. This is not coupling that
   exists and shouldn't — it is coupling that *should* exist and doesn't.
   *Fix: construct `EDGE_WITH` edges from resolved context clauses and
   `EDGE_ELABORATE`/`EDGE_ELABORATE_ALL` from the pragmas; until then the
   `// ???` comments on the edge kinds are accurate documentation.*

4. **Accept-and-ignore features blur the §9/§12/§13 contract.** Enum rep
   clauses, record rep clauses, address clauses, and subunits are all parsed
   and then silently dropped at different depths (§12 stores enum
   rep_values nobody reads; address clauses register markers nobody emits).
   Every such feature should either be implemented through to codegen or
   rejected with a diagnostic at its current frontier. Silent acceptance is
   the worst of the three options for a conformance-tested compiler.

5. **Driver global state versus fork-per-file.** `ALI_Cache` and
   `g_elab_graph` are process globals in a design that forks workers.
   *Fix: reset both at the top of the worker, or make compilation state a
   struct the worker owns.* Cheap insurance.

## 5. Dead code inventory

Confirmed by call-site verification (not grep-trusted; each checked):

| Item | Location | Disposition |
|------|----------|-------------|
| `Edit_Distance` | decl 469, body 3615 | Wire into undefined-name diagnostics or delete |
| `Big_Real_Add_Sub` / `_Multiply` / `_Scale` / `_Divide_Int` / `_Compare` / `_To_Hex` | decls 582–597, bodies 3939–4070 | Delete; `Rational` is the canonical exact domain |
| `NK_CODE` | enum 876 | Implement RM 13.8 or remove + diagnose |
| `EDGE_WITH`, `EDGE_ELABORATE`, `EDGE_ELABORATE_ALL`, `EDGE_INVOCATION` | 3285–3289 | Not dead types, but no constructor calls — see §15 finding |
| `enumeration.rep_values` | written 16744–16754, zero readers | Implement enum rep clauses in §13 or reject the clause |

Explicitly **not** dead, despite appearances: `Rational_*` arithmetic
(consumed by `Eval_Const_Rational` and static float comparison) and
`Simd_Parse_Digits_Avx2` (the AVX2 batch path of decimal literal parsing).

## 6. Recommended order of work

1. Fix §13 sub-section numbering (duplicate §13.2.1/§13.2.2; §13.10 vs
   §13.17) — zero-risk, restores the navigation invariant.
2. Delete the dead `Big_Real` arithmetic and `Edit_Distance` (or wire the
   latter into diagnostics).
3. Move `Type_To_Rep` + fat-pointer type helpers from §10 to §13 —
   the highest-value structural fix; mechanical via delete-first refactor.
4. Build the missing elaboration edges (`EDGE_WITH`, `EDGE_ELABORATE*`) —
   small wiring job, completes §15's mission, and gives §14's elaboration
   records a consumer.
5. Decide each accept-and-ignore feature (enum/record rep clauses, address
   clauses, subunits, `NK_CODE`): implement or diagnose. No silent drops.
6. Split `Emit_Binary_Op_Predefined` along type-class lines.
7. Reset driver globals per fork worker.
