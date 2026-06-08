# Libraries: RM 10 Program Structure, Top to Bottom

A correctness plan for the compilation-unit / program-library system —
§14 (Library management), §15 (Elaboration), §17 (File loading), §19
(Driver) — against the Ada 83 Reference Manual chapter 10
(`reference/Ada83_LRM.md` §10.1–10.6) and the GNAT sources
(`reference/gnat/lib*.ad?`, `bindo*.ad?`, `gnatllvm-codegen.ad?`).

**The bar is production-ready functionality** (standing user direction):
full machinery wired end to end, no half-implementations, no parse-only
features, no silent caps, the refactor discipline of `docs/generics.md` §8
in force throughout — delete-first, compiler as worklist, no old-design
artifacts at landing, suite gates with pass-set diffs and zero losses.

## 1. Ground truth (ACATS)

- **Class CA (compilation/library structure): 164 files.** The harness
  skips every file whose basename ends in a digit (multi-file fragments),
  so only ~20 run today and 13 pass. The skipped 140+ are the actual
  subject matter of this plan.
- **Class L (post-compilation rejection): 337 files** — illegal library
  configurations that must be caught at compile, bind, or link.
- The fragment model is the crux: ACATS multi-file tests list their files
  in a prescribed compilation order, and **file names do not match unit
  names** — `ca1002a1.ada` contains `PACKAGE BODY CA1002A0`,
  `ca1002a4.ada` contains `SEPARATE (CA1002A0)` subunits, `ca1002a8.ada`
  contains a subunit named TEXT_IO. No name-derived lookup can find
  these. Correct behavior requires a **program library** that records,
  at each compilation, which units each file provided.
- Suite state at plan time: class C 1075/1979, class A 140/140. Class B
  excluded by standing direction.

## 2. Current implementation map

What already works (post-W4, see `docs/systems.md`):

- WITH-driven source loading by unit name (`Load_Package_Spec`,
  `Lookup_Path`), recursive for transitive WITHs; spec-only packages
  queue their spec so package state is defined; bodies queue and emit.
- Circular-WITH detection (`Loading_Set`).
- Elaboration graph: EDGE_SPEC_BEFORE_BODY, EDGE_WITH,
  EDGE_ELABORATE(_ALL) from context clauses and pragmas (which the
  parser keeps); library instantiations are graph units; @main emits in
  graph order with a no-silent-drop sweep.
- ALI files: full GNAT-format write (`ALI_Write` — V/P/U/W/D/X lines),
  parse-back (`ALI_Reader`), checksum-gated spec reuse
  (`Try_Load_From_ALI`).
- Generic library templates: loaded, cached one parse per template;
  SEPARATE stubs of generic units complete the template.

The gaps, in dependency order:

| # | Gap | Severity |
|---|-----|----------|
| G1 | No program library: nothing records unit→file or unit→ALI across compilations; every compile re-derives everything from file names | blocking for CA |
| G2 | Unit-name→file lookup is name-derived only (`<name>.ads/.adb/.ada`); ACATS fragments are unfindable | blocking for CA |
| G3 | Standalone subunits (RM 10.2): stub warns and emits nothing; subunit files never compile against their parent context | blocking for CA2x |
| G4 | The harness cannot run multi-file tests at all (digit-suffix skip) | blocking for CA/L measurement |
| G5 | ALI dependency records (D lines, with-elaboration flags) have zero consumers — no staleness detection beyond the one checksum gate, no recompilation-consequence rules (RM 10.3) | required for L |
| G6 | EDGE_INVOCATION never constructed — elaboration-order ignores calls from elaboration code into other units (GNAT bindo's invocation graph) | correctness hole in 10.5 |
| G7 | Loading caps (MAX_LOADED_UNITS, MAX_LOADING_NAMES, ALI caches) silently skip on overflow — violates no-silent-caps |
| G8 | `Has_Precompiled_LL` short-circuits loading with no elaboration-graph contribution — ordering vs precompiled units untested |
| G9 | Multi-unit source files: the "more units in same file" loop handles some shapes; the full matrix (spec+body+subunits+pragmas in one file) is untested |

## 3. Reference survey

### RM chapter 10 (`reference/Ada83_LRM.md`)

- **10.1 Compilation Units — Library Units**: a compilation is a sequence
  of compilation units; library units are subprogram/package/generic
  declarations and instantiations; secondary units are bodies and
  subunits. One name space for library units.
- **10.1.1 Context Clauses**: WITH names library units; visibility rules
  for the named units; USE in context clauses.
- **10.2 Subunits**: `SEPARATE (parent)` bodies; the subunit's visible
  environment is its stub's environment (the parent body at the stub
  point, context clauses of the subunit ADDED); subunit names live in the
  parent's namespace; transitive subunits allowed.
- **10.3 Order of Compilation**: a unit must be compiled after the units
  it names; a body after its declaration; a subunit after its parent
  body. **Recompilation consequences**: recompiling a declaration
  obsoletes its body and all units naming it; recompiling a parent body
  obsoletes subunits. These are the rules class L exercises.
- **10.4 The Program Library**: the library holds, for each unit, its
  compilation state — exactly the record our ALI files were built to be.
- **10.5 Elaboration of Library Units**: before main, all needed library
  units elaborate in an order consistent with the partial order of
  dependencies; pragma ELABORATE forces named units' BODIES first.
  PROGRAM_ERROR for calls into unelaborated bodies (the dynamic check
  GNAT's static model proves away — see bindo).
- **10.6**: optimization latitude — nothing to implement.

### GNAT (`reference/gnat/`)

- **`lib.ads` / `lib.adb`**: the in-memory Units table — one row per
  compilation unit: unit name, file name, version checksum, kind, flags
  (preelaborate/pure/elaborate_body), dependency span. Our equivalent
  must become the program library's working set; today we have only the
  scattered `Loaded_*` arrays.
- **`lib-load.adb`**: `Load_Unit` — unit-name driven loading with the
  **file-name mapping as a separate concern** (`Fname.Uname`,
  file-name-from-unit-name as default, source-file-name pragmas as
  override). Models exactly our G2: lookup must consult the library
  FIRST, the naming convention as fallback.
- **`lib-writ.adb`**: the ALI format we already write — confirms our
  V/P/U/W/D/X line usage; the binder is its consumer, never the
  compiler's semantic phase. Lesson: don't reconstruct symbols from ALI
  (we load source); consume ALI for **dependency/staleness/binding**
  only. `Try_Load_From_ALI`'s spec-reconstruction path is the wrong
  growth direction and stays frozen.
- **`bindo*.ad?`** (the modern binder): builds TWO graphs — the
  **library graph** (units, WITH/ELABORATE edges; bindo-graphs.ads) and
  the **invocation graph** (calls/instantiations reachable from
  elaboration code; bindo-augmentors.ads "augment the library graph with
  invocation edges"). Elaborators (bindo-elaborators.adb) topologically
  sort with SCC handling and diagnose cycles (bindo-diagnostics.adb).
  Our §15 is a miniature of this; G6 (EDGE_INVOCATION) is precisely the
  augmentor pass we never wrote.
- **`gnatllvm-codegen.adb` / `gnatllvm-compile.adb`** (GNAT-LLVM): one
  compilation unit per invocation lowered into one LLVM module;
  inter-unit references are externals resolved at llvm-link time; no
  whole-program loading in the code generator. Confirms the target
  shape: per-unit `.ll` + a library that knows the file set, with
  llvm-link as our binder's back half.

## 4. Design decision: the library IS the ALI set

One structural choice drives everything. Two candidate models:

- (a) **Whole-program loading** (today's): the main compilation pulls
  every withed unit's SOURCE into one resolution space and one `.ll`.
  Works for name-derivable files only; G1/G2/G3 are unsolvable inside it
  because there is no record of foreign-named files.
- (b) **Program library** (GNAT's, RM 10.4's): every compilation writes
  its units into a library (our ALI directory); unit→file,
  unit→checksum, unit→dependencies all come FROM the library; loading by
  unit name consults the library first and the naming convention as
  fallback.

**Decision: (b), grown from (a) without breaking it.** Single-file
compiles keep working exactly as today (the library is consulted, finds
nothing for REPORT beyond name-derived lookup, proceeds). Multi-file
ordered builds make each compilation REGISTER its units, so later
compilations resolve foreign-named files through the library. Source
remains the semantic truth (GNAT's model); ALI carries the catalog.

## 5. Findings

- **F1 (G1/G2)**: No unit catalog. `Lookup_Path` derives file names from
  unit names; ALI U-lines already RECORD unit names per file but nobody
  reads them for lookup.
- **F2 (G3)**: Subunit machinery exists only for generic templates
  (`Complete_Instance_Package_Body`). A standalone stub needs: find the
  subunit's source (by catalog, G1), parse it, resolve it inside the
  parent body's scope at the stub point with the subunit's own context
  clauses added (RM 10.2), then emit it as the stub's body. The scope
  surgery is the same shape the generics rebuild already proved out.
- **F3 (G4)**: The harness needs a multi-file protocol: parse the
  `-- SEPARATE FILES ARE:` prologue (or compile `<prefix>*.ada` in
  lexical order — ACATS numbering encodes the order), compile each into
  a shared per-test library dir, run the `m` unit.
- **F4 (G5)**: RM 10.3 obsolescence = compare checksums recorded in ALI
  D-lines against current sources; an obsolete unit triggers reload (we
  recompile from source anyway) — the rule matters for class L
  REJECTION (using a stale unit must be an error at bind time, not a
  silent stale read).
- **F5 (G6)**: Elaboration code calling another unit's subprogram needs
  an EDGE_INVOCATION from that unit's body. Conservative Ada 83
  approximation: scan each unit's elaboration-executed code (package
  ___elab content) for calls that resolve to other library units' bodies
  and add edges; cycles through invocation edges are weak (GNAT: can
  break with a warning) — `Edge_Kind_Is_Strong` already encodes this.
- **F6 (G7)**: Caps must warn when they bite. `Defer_Subprogram_Body`
  set the precedent (arena-grow); the load tables should grow the same
  way; where a fixed cap genuinely remains, overflow reports.
- **F7 (G8)**: A precompiled `.ll` unit contributes no graph vertex; its
  ___elab (if any) is invisible. The catalog must record precompiled
  units (name, no source) and the @main sweep must call their elab
  functions via externs, ordered by their recorded WITHs.
- **F8 (G9)**: Multi-unit files: `Parse_Compilation_Unit` returns ONE
  unit; the "more units" loop exists only inside `Load_Package_Spec`.
  The driver path for a directly-compiled multi-unit file must iterate
  the same way — one loop, shared.

## 6. The plan

Phase gates throughout: full class C + A before/after each phase,
pass-set diff, zero losses; CA pass count is the progress metric for
phases 2–5 (it is the suite section that exercises this work); class L
becomes runnable in phase 6. Honesty ledger appended per phase, same
discipline as `docs/generics.md` §11.

### Phase 0 — Harness: make the subject measurable

Teach `run_acats.sh` the multi-file protocol: group files by test prefix
(`ca1002a*` → fragments `0..9` + main `*m`), compile fragments in lexical
order into a per-test scratch dir (shared ALI library), compile and run
the main last. Class CA stops being skipped; baseline CA/L numbers go in
the ledger. No compiler changes; the harness gate is "no change to
currently-passing tests".

### Phase 1 — The unit catalog (RM 10.4)

One new §14 structure, Ada-convention named (`Library_Catalog`): maps
unit name → {source file, ALI file, checksum, kind spec/body/subunit,
provider units}. Populated two ways: (a) in-process, every unit this
compilation loads or compiles registers immediately; (b) on startup,
scan `-L <libdir>` (new flag; defaults to the output directory) ALI
U-lines. `Lookup_Path`/`Lookup_Path_Body` consult the catalog FIRST,
name-derivation second. `Try_Load_From_ALI`'s reconstruction stays
frozen; the catalog only resolves names to files — source remains the
semantic truth (GNAT lib-load model). Delete-first: the scattered
`Loaded_Body_Names` array folds INTO the catalog (one tracking
structure, not two).

### Phase 2 — Subunits (RM 10.2)

`SEPARATE (parent)` for plain units, the F2 design: at stub resolution,
record the stub scope (the same mechanism generic body completion uses);
when the subunit's compilation unit arrives (same file, later file in
the ordered build, or catalog lookup), re-enter the parent body's scope
at the stub point, add the subunit's context clauses, resolve, splice as
the stub's body, emit through ordinary paths. Transitive subunits
recurse. The W5 stub warnings DELETE in the same change (no stale
diagnostics for implemented features). CA2x is the gate group.

### Phase 3 — Order and obsolescence (RM 10.3)

Consume ALI D-lines: at catalog load, compare recorded checksums against
current sources; an inconsistent set is a hard error naming the stale
unit ("X was compiled against an older Y") — never a silent stale read.
In-process ordered builds keep checksums consistent by construction.
This is the class-L substrate.

### Phase 4 — Elaboration completeness (RM 10.5)

EDGE_INVOCATION (F5): after a unit's ___elab body is generated, scan its
emitted call set (collected during generation, not by re-parsing) for
targets in other library units; add weak invocation edges; bindo-style
cycle diagnostics distinguish "true ELABORATE_ALL cycle" (error) from
"invocation cycle" (warning + dynamic order). Honor Pure/Preelaborate
flags already recorded in ALI (they relax edge requirements). Precompiled
`.ll` units (F7) enter the graph from their ALI records.

### Phase 5 — Driver: ordered library builds (RM 10.1)

`ada83 a.ada b.ada main.ada` compiles IN ORDER sharing one catalog: each
file's units register before the next file compiles (sequential when
order matters; the fork-per-file parallel mode remains for independent
files via a flag or heuristic — order-sensitive iff any later file
withs/completes an earlier unit, decidable from the catalog).
Multi-unit files (F8): one shared multi-unit loop for driver and loader.
Worker resets (W7) extend to the catalog.

### Phase 6 — Class L wiring + caps

Run class L under the phase-0 harness (its pass criterion — compile OR
bind OR link rejects — already exists). Grow-or-warn every load-path cap
(F6). Ledger the L baseline and the no-silent-caps proof (grep for
remaining fixed caps with silent skips: zero).

## 7. Refactor discipline (binding)

`docs/generics.md` §8 applies verbatim. Specifics for this work:

- End-state sentences per phase before editing (e.g. phase 1: "When
  done, the compiler has ONE unit catalog consulted by every name→file
  lookup, and does not have Loaded_Body_Names or any second tracking
  array.")
- No parse-only features remain anywhere in §17 at the end (subunits
  were the last).
- Spec/body § harmony for every new declaration; § numbering stays
  content-keyed.
- Full identifier names; no magic numbers — named limits or growth.
- The suite is a landing constraint; CA/L counts are progress signals
  only after phase 0 makes them real.

## 8. Risks

- **Scope coupling with the harness** (phase 0): a harness bug can
  masquerade as compiler progress. Keep harness and compiler changes in
  separate landings; gate each independently.
- **Subunit scope re-entry** (phase 2) touches the same scope machinery
  generics lean on — the generics golden test (`--clone-check`) and the
  cc/ca groups both gate it.
- **Sequential driver mode** (phase 5) must not regress the parallel
  path's isolation; the catalog is the only shared state and is
  explicitly handed off, never implicitly inherited (W7 lesson:
  fork-inherited globals rot silently).
- **ALI trust** (phases 1/3): the catalog trusts ALI only after checksum
  verification against the actual source bytes; a hand-edited or stale
  ALI must degrade to name-derived source lookup, loudly.


## 9. Honesty ledger

- *Phase 0 (harness)*: `run_acats.sh` rebuilt around ONE pipeline —
  `gather_files` (ACATS naming: fragments `<base><digit>.ada`, main
  `<base><digit>m.ada`, base ends in a letter; singletons like c35502m
  do not match) + `compile_set` (each file in lexical order, `-o` into
  the per-run results dir so `.ll` and `.ali` land together, stdout
  silenced) feeding every class's link/run stage. No second compile
  path; class b untouched (excluded by standing direction).
  - Two harness defects caught by the C gate during landing: the family
    regex over-matched singleton names ending in `m` (base "c" globbed
    half the suite), and compiler stdout leaked into result lines under
    `-o`. Both fixed before baseline.
  - Baselines under the protocol: class C 1072, class A 138, class L
    27/47 runnable mains, CA 12/13 runnable mains (46 CA families now
    COMPILE their members and fail honestly on catalog/subunit gaps).
  - Honest reclassifications (returning at L1/L2): c39006c0m,
    c39006f3m, ca1012b4m, ad7001c0m, ad7001d0m previously "passed" as
    incomplete solo programs — their SEPARATE stubs were never compiled
    (the stub warning fired) and never executed. Under the protocol
    their family members compile and fail honestly on the missing
    catalog/subunit machinery. Correctness over pass count.

- *Phase 1 (unit catalog)*: `Library_Catalog` lives in §14.6 — arena-grown
  entries {unit name, source file, kind spec/body/subunit, checksum,
  loaded}. Populated from the output directory's ALI files at every
  `Compile_File` start (the harness's per-test `.lib` directory IS the
  program library; a shared directory proved to be 32-way cross-talk and
  the per-test granularity cured a long-standing ce-family shared-CWD
  flake class along the way). `Lookup_Path`/`Lookup_Path_Body` consult
  the catalog first, naming convention second. ALI U-lines now record
  the ACTUAL source file (the old name-derived guess was a lie for
  foreign-named files) plus an SB flag and PARENT.CHILD naming for
  subunit units. `Loaded_Body_Names` died into the catalog's `loaded`
  flag (one tracking structure); the fork-worker reset covers the
  catalog. Directed proof: foreign-named fragments (frag1/frag2.ada
  providing CAT_PKG spec/body) compile first, a main WITHing CAT_PKG
  finds both through the library and runs.
  - Harness protocol refined twice on gate evidence: the main's module
    is whole-program, so ONLY the main's .ll links — plus the .lls of
    fragments the ACATS order places AFTER the main (the main holds
    externs for those).
  - The honest path exposed a silent-drop codegen bug: slice assignment
    through a non-symbol prefix (`FCBs(Idx).Name(1..N) := NAME` — a
    record field of an array element) emitted NOTHING (`if (not
    array_sym) return`). TEXT_IO.CREATE never stored file names; DELETE
    called `remove("")`; ce3114a's prior PASS was false (the bogus
    ALI-export path masked it). Fixed: non-symbol prefixes compute the
    destination through Generate_Lvalue / the fat-pointer expression.
  - Gate after L1 (merged with the concurrent string-bounds work):
    class C 1110 (+49/−14 vs the 1075 pre-plan baseline; the −14 ce21xx
    /c52103h cluster is owned by the concurrent fix stream per user
    direction and excluded from this plan's regression accounting),
    class A 138, CA mains passing under the protocol, class L 14/47
    (L's drop is EXPECTED: the catalog makes formerly-unfindable units
    findable, so illegal configurations now compile — phase 3
    obsolescence is what rejects them again).

- *Phase 2 (subunits, IN PROGRESS — checkpoint at C=1136/A=136/L=15)*:
  Landed machinery, all gated:
  - Stable cross-module naming: `Symbol_Is_Subunit_Stub` (stubs and their
    completions mangle without the per-compilation _s suffix) and
    `Symbol_Has_Stable_Library_Name` (PROGRAM UNITS whose LEXICAL scope
    chain is package-only up to the library unit — the symbol-parent
    chain cannot see DECLARE blocks; two regressions taught that: every
    direct local of a library subprogram first lost its suffix, then
    sibling-block procedures collided — both caught by gate diffs and
    fixed by walking defining_scope owners).
  - Parent chain loading: `Load_Subunit_Parent` materializes dotted
    SEPARATE parents level by level through the catalog (subunit BODY
    materialization keyed on the catalog entry's `loaded` flag — the
    spec symbol existing is NOT enough, RM 10.2's environment is the
    parent BODY).
  - `Symbol_Manager_Enter_Scope`/`Leave_Scope`: enter a stub's scope
    WITHOUT reparenting (the old Push_Existing reparenting destroyed the
    lexical chain — a subunit lost sight of its grandparent's locals).
  - Subunit compilations emit ONLY subunit code (`subunit_compilation`
    mode: no loaded-body re-emission, no @main).
  - SEPARATE-boundary externs: `Note_Separate_Boundary_Callee` at both
    call emitters + `Append_Separate_Callee` from stub emission; a
    post-generation flush declares noted callees that never got a define
    (full call ABI including the static-link parameter). In subunit
    modules every stable external callee is declared.
  - Package-body stubs: parent declares and calls
    `@<name>___elab(ptr)` at the stub's elaboration point; the subunit
    module defines it as a function over the parent's frame —
    `Emit_Parent_Frame_Aliases` extracted from the task-body emitter
    (one implementation) and shared.
  - ALI source paths recorded as given (bare basenames join the library
    directory; anything with a slash is CWD-relative).
  Remaining defects (exact, next session):
  - `%__frame.ca2004a1__j` undefined in a subprogram-subunit module:
    frame aliases for PACKAGE-nested variables of the parent frame are
    incomplete in the subunit-compiled module (ca2004).
  - ca2007 links but lli crashes in module init — package-body-subunit
    elab over parent frame, runtime-debug next.
  - ca1002: loader's body-context resolution misses USE visibility when
    loading a library package body through the catalog ("cannot resolve
    FAILED"), file label CA1002A0.adb.
  - c39006f3m, ca2009a, ca2011b lost vs the 1110 run (subunit-machinery
    interaction; diff-listed, undiagnosed).
  - ca2001h needs phase 3 (unit replacement semantics).
  - A at 136: ad7001c0m/d0m + 2 fails pending the same tails.

- *Phase 2 continuation (checkpoint at C=1143/A=136/L=15)*: ca2007 (the
  package-body-subunit-over-parent-frame family) PASSES end to end.
  Landed since the prior checkpoint:
  - Depth-aware frame aliases: the nested-subprogram alias block in
    Generate_Subprogram_Body now walks EVERY enclosing subprogram level,
    chasing the static chain (each nested frame stores its parent's frame
    pointer at its own frame_size offset) — depth-2 uplevel access was
    broken even in-module before this.
  - `Has_Nested_Subprograms` counts SEPARATE package-body stubs: the
    subunit's elaboration runs over THIS frame, so the frame must exist.
  - Frame-nested subunit package bodies emit as ONE
    `@<name>___elab(ptr %__parent_frame)` with `%__frame_base` aliasing
    the parent frame (shared offsets) — BODY declarations and statements
    only (the spec elaborated at its declaration point in the parent's
    module, RM 7.2).
  - **The frame-layout publication contract**: ALI F-lines (emitted name
    + byte offset, deduplicated last-wins) and an FS line (frame size)
    publish each unit's frame layout; `Apply_Published_Frame_Layout`
    adopts it for the root parent AND at every materialized subunit chain
    level. This replaced the doomed implicit-determinism model (offsets
    diverged the moment any load-order side effect differed).
  - `Symbol_Add` assigns frame offsets ONCE (`frame_offset_assigned`);
    re-installation used to re-number, producing duplicate slots even
    within one module.
  - String literal constants are `private` (were `linkonce_odr` — same
    `@.strN` names with different contents merged across modules, so a
    subunit's FAILED printed the parent's TEST banner).
  - `Mark_Package_Level_Objects` only for packages with no enclosing
    subprogram (a nested package's state is frame-resident, and its
    subunit must agree).
  NEXT (designed, not yet built): subunit-ADDED package state (ca2004's
  K — body-level variable invisible to the parent's module) cannot live
  in the parent frame (the parent sized its alloca first; K@FS
  collides). Rule: a frame-class symbol WITH a published F-slot is
  frame-resident at that offset; WITHOUT one it belongs to the subunit's
  own module as a stable-named GLOBAL. Requires: extending
  Symbol_Has_Stable_Library_Name to package-chained DATA (with a
  depth>0 guard so plain subprogram locals stay suffixed — that exact
  regression already happened twice), a global-classification override
  in subunit compilations, and extending the SEPARATE-boundary flush to
  external GLOBAL declarations for data. Then: ca1011 lli crash, ca1002
  loader USE-visibility, ca2001h (phase 3 semantics), A stragglers
  (ad7001c0m/d0m + 2).

- *Phases 2–6 complete (final state C=1142 / A=140 / L=47-of-47)*: every
  phase landed; suite green with zero pass-set regressions against the
  pre-phase-3 baseline (the ce21xx/c52103h cluster is owned by a
  separate fix stream and excluded throughout).
  - *Phase 2 close*: the "NEXT" design above was built exactly as
    written (`owned_by_subunit_module` globals, scope-walk
    `Symbol_Has_Stable_Library_Name`, SEPARATE-boundary data externs).
    ca2004/ca2007/ca1011/ca2001/ca1003/ca1002 all pass. The last two
    ca1002 defects: loaded library procedures overwrote
    `main_candidate` (now guarded by `generating_loaded_unit`), and the
    loader resolved unrelated units sharing a WITH'd unit's file (now
    skipped unless they complete the loaded unit). A recovered to
    140/140: ad7001c/d fixed by the same loader-scope fix plus
    package-elab uplevel access (a subunit package elaboration function
    has no frame of its own — every framed symbol is uplevel), and
    aa2010a by recognizing in-file parents (subunit mode applies only
    when a subunit's parent is NOT compiled by the same invocation).
  - *Phase 3 (obsolescence)*: the catalog records per-unit W lines
    (per UNIT, not per file — a spec must not inherit its body's
    context), an RB flag (spec requires body, RM 7.1), and ST lines
    (the body's stub names, RM 10.2). Rules: a REQUIRED body older than
    its current spec is a hard error; an OPTIONAL body that became
    obsolete (own spec or any W-dep recompiled later) is silently
    DROPPED by the loader (ca3006c/d's own test titles demand
    deletion); subunit staleness is judged only against the current
    body's ST set (a replacement body may have dropped the stub —
    ca2001h). Missing bodies: WITH'd library subprograms get volatile
    ptrtoint presence anchors in @main (a bare ptrtoint is dead code
    the JIT never resolves); required-body packages get their
    elaboration call emitted against an external definition; generics
    are checked at compile time (catalog body or in-file body or
    error). `--bind <libdir> <main-unit>` is the gnatbind analogue: a
    closure walk over catalog W/ST records run by the harness after
    all compiles — it catches what main-compile-time cannot (units
    recompiled AFTER the main, la5007p/q/s/t). Class L: 10 → 47/47.
  - *Phase 4 (elaboration)*: pragma Pure/Preelaborate land on symbols,
    flow into graph vertices and ALI PR/PU flags; EDGE_INVOCATION
    (weak) edges are noted at Generate_Apply when emission is inside a
    package's library elaboration; precompiled-.ll units' ALI W sets
    enter the graph; an inconsistent order is now a hard error
    (RM 10.5 — la5001a's Elaborate cycle) instead of a warning plus a
    fabricated source order. The `???` enum comments in §15 are gone.
  - *Phase 5 (ordered builds)*: multi-file invocations pre-scan
    (parse-only) each file's defined/referenced unit names; any
    later-file dependency on an earlier file forces sequential
    compilation in command-line order (children share the library via
    the on-disk ALI set); independent files keep fork-per-file
    parallelism. Scan-table overflow forces the conservative
    (sequential) order. Spec globals: a unit's own compilation emits
    the authoritative STRONG definition; loaded-unit re-emissions are
    linkonce_odr copies; a body compile re-emitting its spec's
    declarations owns them (strong) only when the spec REQUIRES a body
    — llvm-link drops a linkonce definition whose own module never
    references it, which broke `ada83 dep.ada main.ada` builds.
  - *Phase 6 (caps)*: load-path caps are grow-or-warn — gone entirely
    (arena-grown): MAX_LOADED_UNITS, MAX_LOADING_NAMES,
    MAX_ELAB_FUNCTIONS, ALI_Info.units[8], ALI exports[256] (writer
    and cache), published frame slots[256]; warned: include paths
    (32); documented-benign: the ALI parse cache (overflow = parse
    fallback, correct but slower). Removing units[8] exposed that
    task-body subunits were recorded as UNKNOWN in ALIs (aa2010a) —
    NK_TASK_BODY is now a first-class unit kind in ALI_Collect_Unit.
    Dead Dependency_Info struct deleted.
  - Known out-of-scope: direct TEXT_IO.PUT_LINE without REPORT crashes
    lli (CONSTRAINT_ERROR in text_io at runtime) — reproduced
    identically with the HEAD compiler, pre-existing, not a library
    defect.
  - *Post-plan tail (final: C=1143/1976, A=140/140, L=47/47)*: gathering
    letter-suffixed fragments (d64005ea-style) into their families
    exposed three latent codegen defects, all fixed: (1) a slice
    assignment whose source is a concatenation trusted the context
    type's predicted rep (thin pointer for a constrained array) over the
    value's actual fat-pointer rep — Generate_Expression's returned
    LLVM_Value.rep is ground truth; (2) Emit_Memcpy_To_Symbol and
    Emit_Fat_Pointer_Copy_To_Name printed a hardcoded `%name`
    destination, bypassing Emit_Symbol_Storage's uplevel/global logic
    (an uplevel array assignment in a subunit referenced a frame alias
    that was never in scope); (3) Emit_Parent_Frame_Aliases capped its
    chain walk at depth 16 — the 18-level d64005g chain silently lost
    its root-frame aliases (cap removed; lexical nesting cannot cycle).
    c64005c passes for the first time. The d64005e/f/g mains
    themselves still fail at runtime — a cross-module static-chain
    defect at depth, beyond RM 10 scope and predating this plan.
