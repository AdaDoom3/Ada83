# The Generic Package System — Full Correctness Pass and Implementation Plan

A top-to-bottom audit and implementation plan for generic units (§16 plus its
tendrils into §9, §11, §12, §13, §14, §15, §17) against the Ada 83 Reference
Manual chapter 12 and the GNAT sources (`reference/gnat/sem_ch12.*`,
`exp_ch12.adb`, `par-ch12.adb`).

This document is the single source of truth for the generics work. It fully
subsumes the earlier `docs/generics-instantiation.md` (the tree-copy rewrite
plan): every piece of evidence, the design, the staging, the risks, and the
verification criteria from that document are incorporated below (primarily in
§3, §7 Phase 3, §10, and §11). It also incorporates the whole-compiler
subsystem survey (`docs/systems.md`) in full — its generics-relevant
findings are woven into §2.11, F11, F15, §8.10, §9, and §10, and the
complete survey is preserved as the §12 appendix, because the generic
system's production readiness depends on the health of the subsystems it
wires into (§10 Types, §14 Library, §15 Elaboration in particular). The plan's goal is not "more tests pass" — it
is a generic system whose every required piece of machinery exists, is wired
in, and is production ready, with the ACATS suite as the landing constraint —
and a refactor that lands complete: no new code alongside old dead code, no
artifacts of the old design, no halfway state (§8).

Contents:

1. Ground truth — ACATS chapter 12 results
2. Current implementation, top to bottom (the full map)
3. Reproduced evidence of the structural defect
4. Normative requirements — the RM 12 checklist
5. GNAT reference architecture
6. Findings (F1–F15)
7. The plan — end state and phases
8. Refactor discipline and disposition table
9. Machinery wiring matrix
10. Risks
11. Production-readiness gates and verification
12. Appendix — subsystem survey (full ada83.c audit, merged from docs/systems.md)

---

## 1. Ground truth: ACATS chapter 12 (run 2026-06-06)

| Group | Pass | Fail | Skip | Total | Rate |
|-------|------|------|------|-------|------|
| cc (class C, executable) | 15 | 40 | 55 | 110 | 13% |
| bc (class B, illegality) | 22 | 153 | 0 | 175 | 12% |

- bc failure breakdown: **141 WRONG_ACCEPT** (compiler accepted an illegal
  instantiation), 12 LOW_COVERAGE (rejected but <90% of `-- ERROR` lines
  matched a diagnostic).
- cc skip breakdown: 37 BIND (unresolved symbols at llvm-link), 18 COMPILE
  (resolution gaps — "cannot resolve selected component", "incompatible
  types", "numeric operands required" — plus two internal errors: cc1308a,
  cc3601a "unhandled binary operator 33").
- Dominant cc failure family: missing instantiation-time constraint checks
  (cc3406/cc3407/cc3408/cc3504 families, ~23 tests, "CONSTRAINT_ERROR not
  raised / raised inappropriately").

Failure-mode samples (from `acats_logs/`):

| Test | Mode | Detail |
|------|------|--------|
| cc1010a | Wrong output | "BINDINGS INCORRECT" — generic formal binding wrong |
| cc1221b | Wrong output | `'BASE'FIRST`/`'BASE'LAST` wrong in instance |
| cc1222a | Crash | SIGSEGV at runtime (lli, both ORC and mcjit) |
| cc1302a | Wrong output | `'PRED`/`'SUCC` wrong result in instance |
| cc1311a | Wrong output | Wrong default value for formal subprogram |
| cc3121a | Wrong output | "INCORRECT RESULTS FROM VAR" (formal object) |
| cc3208a | Wrong output | CONSTRAINT_ERROR not raised for private actual |
| cc3305d | Wrong output | CONSTRAINT_ERROR raised inappropriately |
| cc3406a | Wrong output | CONSTRAINT_ERROR not raised |
| cc3606a | Wrong output | Actual subprogram's parameter defaults unused at call |

The 141 WRONG_ACCEPT number is the single clearest statement of where the
system stands: the legality half of RM 12.3 is essentially absent.

---

## 2. Current implementation, top to bottom

Three mechanisms coexist; only one is load-bearing. The §16 clone machinery
is half-dead (see §3). The full map, by compiler stage:

### 2.1 Syntax (node kinds and payloads)

| Area | Location | Summary |
|------|----------|---------|
| Node kinds | ada83.c:883-888 | `NK_GENERIC_DECL`, `NK_GENERIC_INST`, `NK_GENERIC_TYPE_PARAM`, `NK_GENERIC_OBJECT_PARAM`, `NK_GENERIC_SUBPROGRAM_PARAM` |
| Formal type classes | ada83.c:896-900 | `Generic_Def_Kind`: PRIVATE, LIMITED_PRIVATE, DISCRETE, INTEGER, FLOAT, FIXED, ARRAY, ACCESS, DERIVED |
| Formal object modes | ada83.c:902-903 | `Generic_Mode_Kind`: GEN_MODE_IN / OUT / IN_OUT |
| Node payloads | ada83.c:1346-1384 | `generic_decl{formals,unit}`, `generic_inst{generic_name,actuals,...}`, the three `generic_*_param` payloads |
| Formal-index decoration | ada83.c:930-933, 1734, 1915 | `generic_formal_index` on `Syntax_Node`, `Type_Info`, and `Symbol`: ≥0 = slot in enclosing generic's actuals, −1 = not a formal, −2 = undecided (set at node creation, ada83.c:4788) |

### 2.2 Parser (§9.15)

| Area | Location | Summary |
|------|----------|---------|
| Dispatch | ada83.c:7065-7073 | `Parse_Declaration` sees TK_GENERIC → `Parse_Generic_Declaration` |
| Formal part | ada83.c:6744-6932 | `Parse_Generic_Formal_Part`: type formals (6751-6843, incl. discriminants, all `GEN_DEF_*` forms), object formals (6846-6879, name list + mode + type + default), subprogram formals (6882-6931, spec + `IS name` / `IS <>` default) |
| Generic declaration | ada83.c:6933-6950 | `NK_GENERIC_DECL` with formals list + unit (spec or body) |
| Package instantiation | ada83.c:7178-7194 | `PACKAGE name IS NEW g(actuals)` → `NK_GENERIC_INST` |
| Subprogram instantiation | ada83.c:7089-7113 | `PROCEDURE/FUNCTION name IS NEW g(actuals)` → `NK_GENERIC_INST` |

This layer is in decent shape; the gaps are downstream.

### 2.3 Semantics — generic declaration

| Area | Location | Summary |
|------|----------|---------|
| `NK_GENERIC_DECL` resolution | ada83.c:15770-15806 | Creates SYMBOL_GENERIC; stores formals + unit on the symbol; **does not resolve the unit** (it contains formals); spec-only generics are never analyzed |
| Generic body completion | ada83.c:14777-14995 | When a body matches a generic symbol: pushes a scope with formal symbols (types 14807-14865 incl. discriminated formal privates, subprograms 14867-14917, objects 14920-14943), maps `GEN_DEF_*` → `TYPE_*` stand-ins (PRIVATE→TYPE_PRIVATE, DISCRETE→TYPE_ENUMERATION, INTEGER→TYPE_INTEGER, …), stamps `generic_formal_index` slots, resolves the body |
| Formal subprogram name defaults | ada83.c:8157-8167, decl 1876-1882 | `Resolve_Generic_Formal_Subprogram_Defaults` (RM 12.1.3): resolves `IS name` defaults when the generic *body* is analyzed (formals then in scope) |

### 2.4 Semantics — instantiation

| Area | Location | Summary |
|------|----------|---------|
| `NK_GENERIC_INST` resolution | ada83.c:15809-16641 | The main handler: find template, create instance symbol (SYMBOL_FUNCTION/PROCEDURE/PACKAGE), build the slot array, three resolution passes, export cloning, then `Expand_Generic_Package` for packages (16638-16642) |
| Slot mapping | ada83.c:15884-15898 | Stamps each formal symbol's `generic_formal_index` once; multi-name object formals take consecutive slots |
| Actual placement | ada83.c:15905-15956 | Positional or `NK_ASSOCIATION` named placement into `slot_actuals[]`; checks all formal names matched |
| Pass 1: type actuals | ada83.c:15962-15983 | Resolves type actuals → `generic_actuals[slot].actual_type` |
| Pass 2: object actuals | ada83.c:15985-16075 | Resolves object types (by slot or `Generic_Actual_Type_By_Name`), resolves actual expressions against the formal type, character literals as enum literals, applies defaults; stores `actual_expr` |
| Pass 3: subprogram actuals | ada83.c:16077-16250 | Symbol lookup, `"/="` negation (RM 6.7), operator-string actuals (user-defined or `builtin_operator`), character/enum-literal actuals, attribute defaults (`T'SUCC` → `actual_attribute`, 16239-16244), `IS <>` box defaults via visibility at instantiation point |
| Profile substitution | ada83.c:16254-16342 | For subprogram instances: copies template parameter/result info, substituting formal types by slot or by name (return type 16310-16342) |
| Export cloning | ada83.c:16345-16487 | For package instances: counts and creates exported symbols from the template spec with type formals substituted; nested task specs get entry symbols and single-task objects (16600-16627) |

### 2.5 The instantiation environment (Symbol fields and helpers)

| Area | Location | Summary |
|------|----------|---------|
| `generic_actuals[]` | ada83.c:2001-2013 | Array of `{formal_name, actual_type, actual_subprogram, actual_expr, actual_attribute, builtin_operator, actual_negated}`; length `generic_actual_count` |
| Template/instance links | ada83.c:1993-2000 | `generic_formals`, `generic_unit`, `generic_body`, `generic_body_cache` (disk-parsed body AST, one per template), `generic_template` back-link |
| Expanded trees | ada83.c:2014-2015 | `expanded_spec` / `expanded_body` — clone results (see §2.7) |
| Codegen instance context | ada83.c:2219 | `cg->current_instance` — the active instance during emission |
| `Resolve_Generic_Formal` | ada83.c:8202-8213 (decl 1824) | Formal `Type_Info` → actual via `cg->current_instance->generic_actuals[generic_formal_index]`; walks to parent for nested subprograms in generic packages |
| `Resolve_Generic_Actual_Type` | ada83.c:17865-17879 (decl 1826) | Same lookup, used from 40+ emission sites |
| `Generic_Actual_Type_By_Name` | ada83.c:14057-14064 | Case-insensitive formal-name → actual-type lookup |
| `Substitute_Generic_Formal_Subprogram` | ada83.c:23293-23307 (decl 2484) | Formal subprogram symbol → `actual_subprogram` during emission |
| Substitution re-entrancy depth | ada83.c:2450-2454 | `Generic_Formal_Substitution_Depth` |
| Formal profile helpers | ada83.c:14069-14094, 14102-14144 | `Generic_Formal_Parameter_Types` (formal's profile with actuals substituted); `Generic_Formal_Equality_Actual` (resolves the `"="` behind a `"/="` actual per RM 6.7) |

### 2.6 Code generation

| Area | Location | Summary |
|------|----------|---------|
| `Generate_Generic_Instance_Body` | ada83.c:37670-37847+ (decl 2957) | Packages: sets `cg->current_instance`, resets template `extern_emitted` flags (per-instance copies), emits globals, recurses into exported subprogram bodies via `Find_Homograph_Body` matching (38983-38995). Subprograms: emits header, frame aliases for uplevel access, parameters with substituted types, body |
| Deferred instances | ada83.c:37361-37373, 38973-38979 | `NK_GENERIC_INST` queues to `deferred_bodies[64]`; processed when `template->generic_body` exists |
| Instance body statements (nested) | ada83.c:38930-38971 | Emitted inline at the declaration point per RM 7.2/12.3 — but only from the **resolved template body**; the `expanded_body` clone "has its symbols nulled by Node_Deep_Clone and never re-resolved" (comment at 38938-38944); no resolved body → statements silently skipped ("honest gap") |
| Instance body statements (library) | ada83.c:38997-39019+ | Same story: `__elab` function emitted from `template->generic_body` only; comment at 39001-39005 repeats the clone-is-unusable admission; registered in the §15 elaboration graph |
| Formal subprogram calls | ada83.c:23337-23407 | `Generate_Apply` routes through `Substitute_Generic_Formal_Subprogram` / `builtin_operator`; attribute actuals apply the attribute to the call's arguments |
| Formal array storage | ada83.c:36520 | `Emit_Bind_Bounded_Array_Storage` ("generic formal array") |

### 2.7 §16 cloner and expansion

| Area | Location | Summary |
|------|----------|---------|
| `Node_List_Clone` | ada83.c:43868-43877 | Clones each list item |
| `clone_kids` shape table | ada83.c:43887-43914 | Per-kind child-edge table (`KID`/`KIDS` offsets); covers **25 kinds**; zero rows = shallow copy (aliasing) |
| `Node_Deep_Clone` | ada83.c:43918-43974 | Shallow-copies all fields, nulls `symbol`, substitutes formal *types* via `generic_formal_index` on `n->type` (43940-43944), substitutes formal *object* identifiers by case-insensitive **name match** against `generic_actuals[].formal_name`, splicing in the actual expression tree (43949-43964); depth limit 500 with a real diagnostic |
| `Expand_Generic_Package` | ada83.c:43989-44036 | Clones spec (renamed to instance), finds + parses the body **by filename convention** (`Lookup_Path_Body`, `<name>.adb`/`.ada`, 44011-44028), caches the parsed body AST on the template (`generic_body_cache`), clones per instance → `expanded_spec`/`expanded_body`. Called from one site: ada83.c:16641 |

### 2.8 Library management and loading (§14, §17)

| Area | Location | Summary |
|------|----------|---------|
| ALI serialization | ada83.c:42789 | `NK_GENERIC_DECL` → `is_generic = true` on the unit entry |
| ALI deserialization | ada83.c:43138 | "GE" token → `entry->is_generic = true` |
| Unit flags | ada83.c:3166, 3229 | `is_generic` on both unit-table structs |
| Body discovery | ada83.c:44011-44028, 44105-44107 | `Lookup_Path_Body` filename search; one parse per template, cached |

### 2.9 Missing clone_kids rows (every kind with children absent from the table)

These node kinds have child fields but no shape-table row, so instances
shallow-alias the template's subtrees:

| Kind | Aliased children |
|------|------------------|
| NK_ALLOCATOR | subtype mark, initialization expression |
| NK_SUBTYPE_INDICATION | subtype mark, constraint |
| NK_RANGE_CONSTRAINT | range |
| NK_INDEX_CONSTRAINT | index ranges |
| NK_DISCRIMINANT_CONSTRAINT | associations |
| NK_DIGITS_CONSTRAINT / NK_DELTA_CONSTRAINT | precision, range |
| NK_ARRAY_TYPE | indices list, component type |
| NK_RECORD_TYPE | discriminants, components, variant part |
| NK_ACCESS_TYPE | designated type |
| NK_DERIVED_TYPE | parent type, constraint |
| NK_ENUMERATION_TYPE | literals list |
| NK_INTEGER_TYPE | range |
| NK_REAL_TYPE | precision, range, delta |
| NK_COMPONENT_DECL | names, component type, init |
| NK_VARIANT_PART / NK_VARIANT | variants; choices, components, nested variant part |
| NK_DISCRIMINANT_SPEC | names, discriminant type, default |
| NK_EXCEPTION_DECL | names, renamed |
| NK_ENTRY_DECL | parameters |
| NK_SUBPROGRAM_RENAMING / NK_PACKAGE_RENAMING / NK_EXCEPTION_RENAMING | renamed entity |
| NK_GENERIC_DECL | formals, unit — **nested generics aliased across instances** |
| NK_GENERIC_INST | generic name, actuals — **nested instantiations aliased** |
| NK_GENERIC_TYPE_PARAM / NK_GENERIC_OBJECT_PARAM / NK_GENERIC_SUBPROGRAM_PARAM | full payloads |
| NK_TASK_SPEC / NK_TASK_BODY | entries; declarations, statements, handlers |
| NK_USE_CLAUSE / NK_WITH_CLAUSE | names |
| NK_PRAGMA | arguments |
| NK_REPRESENTATION_CLAUSE | entity name, attribute, expression, component clauses |
| NK_EXCEPTION_HANDLER | exception choices, statements |

### 2.10 Notable absences (summary of what does not exist anywhere)

- Template semantic analysis at declaration (only at body completion, and
  never for spec-only generics).
- Re-resolution of cloned instance trees (clones carry nulled symbols
  forever).
- Formal object mode enforcement and RM 12.1.1 constant/renaming semantics.
- Formal array / access / derived / private-discriminant matching at
  instantiation (any actual accepted).
- Instantiation-time constraint checks (RM 12.3(81)).
- Circular/recursive instantiation detection (the cloner's depth-500 limit
  is the only backstop).
- RM 12.1.2 operation-set modeling for formal types (template checked
  against generic `TYPE_*` stand-ins, not the class's defined operations).
- Validation of formal subprogram actuals' profiles and modes (12.3.6).
- "Exactly one directly visible" enforcement for box defaults.
- Unit-structured generic body association (filename convention instead).

### 2.11 Cross-subsystem smear (from the subsystem survey, §12 appendix)

The survey's architectural findings on where generic knowledge leaks beyond
§16:

| Area | Location | Summary |
|------|----------|---------|
| §12 ↔ §16 cycle | survey §4 | `§12 Semantics → §16 (instantiate) → §12 (re-resolve)` is the one genuine cycle in the compiler's dependency graph. Inherent to the macro model; the goal is containment, and today the clone-state contract (which node fields must be null, which survive cloning, who re-stamps `generic_formal_index`) is implicit across four sections |
| `generic_formal_index` lifecycle smear | §9 parse-time stamp (ada83.c:4788), §12 substitute, §13 consult | Three phases sharing one integer slot on the node; generic knowledge lives in §9, §12, §13, and §16 rather than being contained in §16 |
| Substitution-model frontier diagnostics | ada83.c:20322, 20663 | Two `unsupported: generic formal object substitution cycle` diagnostics — the survey's marker for where the substitution model hits its limits |
| Duplicated formal lvalue substitution | ada83.c:~20314 (`Generate_Lvalue`), ~31344 (assignment path) | The same generic-formal substitution logic implemented twice in §13 instead of one predicate-style helper |
| `Type_To_Rep` resolves formals at lowering time | ada83.c:~7997 (inside §10.7, 7982-8053) | The type system calls `Resolve_Generic_Formal` while answering "what is this type's representation" — a context-dependent answer to what freezing (§10.6) exists to make context-free; part of the survey's №1 coupling problem (LLVM lowering policy inside §10) |
| Nested-instantiation recursion | survey §16 finding | A generic instantiating another generic has no recursion handling (cf. F6's aliased `NK_GENERIC_INST`, F9's missing cycle detection) |
| `separate` generic bodies | survey §16/§17 findings | Generic units with `separate` bodies are not connected post-clone; subunits in general are parse-only (stub and separate body never connected) |
| Elaboration substrate | survey §15 finding | The §15 graph builds only `EDGE_SPEC_BEFORE_BODY` (one call site, ada83.c:43766); `EDGE_WITH` / `EDGE_ELABORATE` / `EDGE_ELABORATE_ALL` / `EDGE_INVOCATION` are declared (3285-3289, comments read `// ???`) but never constructed — instance `__elab` ordering rides this under-wired graph |
| ALI substrate | survey §14 finding | ALI files record pragma `Elaborate`/`Elaborate_All` flags (42904-42905) that nothing consults; only consumer is checksum-gated spec reuse (`Try_Load_From_ALI`, 44355-44392) — generic templates loaded via this path inherit its limits |

---

## 3. Reproduced evidence of the structural defect

All reproduced during the earlier session that produced the tree-copy plan
(2026-06-04 false-pass investigation), and all still present. The current
model keeps ONE resolved-ish AST per generic and emits instances by walking
it with a substitution map (`cg->current_instance` +
`Resolve_Generic_Actual_Type`, 40+ call sites). Consequences:

1. **Instance bodies are never elaborated.** A nested
   `PACKAGE PP IS NEW PKG` drops the template body's statement sequence
   (RM 12.3 violation). c35003b's ENTIRE test substance is absent from its
   IR — its "PASS" is a bare `RESULT` (false pass). Same class: cc3504i/j/k,
   cc3208c, cc3305d, cc3406c, cc3407e, cc3408c, and the ce2xxx
   SEQUENTIAL_IO/DIRECT_IO state init (std-stream FILES entries never set).
2. **Body-internal state has no per-instance storage.** SEQUENTIAL_IO's
   FILES table exists only as whatever the export-alloca path materializes;
   two instances would share nothing AND nothing.
3. **Spec-declared nested instances are never defined.** cd3015f's
   CHECK_HUE (`PROCEDURE CHECK_HUE IS NEW ENUM_CHECK(...)` inside a generic
   spec) has call sites but no definition — links only because the calls
   were also dropped.
4. **Expression emission against template symbols mis-reps.** A formal
   `"="` call emitted from a template body produced an i32 boolean where
   the consumer expected i8 (c67002c) — template symbols carry no
   instance-resolved types, so every emission site needs (and sometimes
   misses) `Resolve_Generic_Actual_Type`.
5. **Every fix near generics grows a gate.** The cc1310a fix is correct but
   carries a three-clause gate ("declaration-free body, statements are
   formal-subprogram calls only") — the honest frontier of what
   substitution-at-emission can do, and a standing marker of the debt.

The ACATS numbers in §1 are the suite-wide expression of the same defect
plus the missing legality layer.

---

## 4. Normative requirements: the RM 12 checklist

The complete requirement set the implementation must satisfy, extracted from
`reference/Ada83_LRM.md`. This is the audit baseline and the
production-readiness traceability source (§9 maps groups to phases).

### 12.1 Generic declarations

1. Generic declarations declare either a generic subprogram or generic package.
2. Generic declarations include a generic formal part declaring any generic formal parameters.
3. Generic formal parameters may be objects, types, or subprograms.
4. Subtype indications in generic formal parts must be only a type mark (no explicit constraints).
5. Generic subprogram designators must be identifiers.
6. Outside a generic unit's scope, the unit name denotes the generic template itself.
7. Within a generic unit's declarative region, the unit name denotes the result of the current instantiation.
8. Elaboration of a generic declaration has no other effect.
9. Within a generic subprogram, the unit's name can be overloaded and appear in recursive calls.
10. The unit name cannot appear after `new` in an instantiation (recursive instantiation prohibited).
11. Default expressions in formal parts are evaluated only when an instantiation uses them.
12. Default expressions for formal subprogram parameters are evaluated only for calls using those defaults.
13. Visibility rules apply to names in default expressions; denoted entities must be visible at the expression point.
14. Generic formal parameters and their attributes are not allowed in static expressions.
15. Multiple formal object identifiers are equivalent to a sequence of single declarations (12.1.1).

### 12.1.1 Generic formal objects

16. A formal object's type is the base type of the type mark.
17. Mode is in or in out; in is the default.
18. Default expressions are only allowed for mode in.
19. The default expression's type must match the formal's type.
20. Mode in = a constant initialized to a copy of the actual's value.
21. Mode in formal object's type must not be limited.
22. Mode in formal object's subtype is denoted by the declaration's type mark.
23. Mode in out = a variable that denotes (renames) the actual object.
24. Mode in out constraints are those of the actual, not the type mark.
25. (Note) Mode in out should use a base type / subtype name for clarity.

### 12.1.2 Generic formal types

26. Formal types denote the actual subtype supplied at instantiation.
27. Formal types are distinct from all other types within the generic.
28. Constraint applicability to formal types follows the class rules for nonformal types.
29. Discrete ranges in formal constrained array types must be only a type mark.
30. Formal private discriminant parts must not include default expressions.
31. Formal private variables must be constrained if the type has discriminants.
32. Formal private type operations: per RM 7.4.2 (assignment + equality unless limited).
33. Formal array type operations: per RM 3.6.2.
34. Formal access type operations: per RM 3.8.2.
35. Formal discrete `(<>)`: operations common to enumeration and integer types (RM 3.5.5).
36. Formal integer `range <>`: integer operations (RM 3.5.5).
37. Formal float `digits <>`: floating point operations (RM 3.5.8).
38. Formal fixed `delta <>`: fixed point operations (RM 3.5.10).
39. Implicit operations are implicitly declared at the formal type's declaration place.
40. Formal fixed multiplying operators returning universal_fixed are declared in STANDARD.
41. At instantiation each implicit operation is the basic operation / predefined operator of the actual type.
42. Rule 41 holds even if the operator was redefined for the actual type or its parent.
43. Formal subprograms can supply operations not implicitly available with the formal type.

### 12.1.3 Generic formal subprograms

44. Declared by generic parameter declarations with subprogram specifications.
45. Defaults: box (`<>`) or a subprogram/entry name.
46. Name-form defaults resolve per the 12.3.6 matching rules.
47. Formal subprogram parameter constraints are those of the matching actual.
48. Formal subprogram result constraints are those of the matching actual.
49. (Note) Base type names recommended for formal subprogram parameter/result types.
50. Formal parameter types can be any visible type, including formal types of the same generic.
51. Formal/actual parameter names need not match.
52. Formal/actual default expressions need not match.
53. A formal subprogram can be matched by a type attribute with a matching signature.
54. An enumeration literal matches a parameterless formal function returning its type.

### 12.2 Generic bodies

55. Generic bodies are templates for the nongeneric bodies obtained by instantiation.
56. Generic body syntax is identical to nongeneric body syntax.
57. Each generic subprogram declaration must have a corresponding body.
58. Elaboration of a generic body only establishes it as a usable template.

### 12.3 Generic instantiation

59. Instances are declared by generic instantiation constructs.
60. Generic actual parts contain positional or named generic associations.
61. Named associations are illegal if two or more formal subprograms share a designator.
62. Each actual must be supplied unless the formal declares a default.
63. Associations follow the 6.4 positional/named rules.
64. An instance is a copy of the generic unit excluding the formal part.
65. Instance of a generic package is a package; of a generic subprogram, a subprogram.
66. (a) Names denoting the generic unit denote the instance.
67. (b) Names denoting mode-in formal objects denote a constant with the copied value.
68. (c) Names denoting mode-in-out formal objects denote the renamed actual variable.
69. (d) Names denoting formal types denote the actual subtype.
70. (e) Names denoting discriminants of formal types denote the corresponding actual discriminants.
71. (f) Names denoting formal subprograms denote the actual subprogram/entry/literal.
72. (g) Names denoting parameters of formal subprograms denote the corresponding actual parameters.
73. (h) Names denoting local entities denote the corresponding instance-local entities.
74. (i) Names denoting global entities denote the same global entities.
75. Formal operators follow rule (f).
76. Predefined operators/operations of formal types refer to the actual type's operations in the instance.
77. Type marks and default expressions in the formal part follow the same mapping.
78. Actual parameter expressions are evaluated in an implementation-defined order.
79. Default expressions are evaluated in the order of the parameter declarations.
80. Elaboration of the instance follows evaluation of all explicit and default parameters.
81. The 12.3.1–12.3.6 constraint checks occur during instance elaboration.
82. Recursive generic instantiation is prohibited (direct or via a cycle).
83. (Note) Overloads differing only by formal types may create ambiguous calls in an instance.
84. (Note) Omitted actuals allowed only with a default.
85. (Note) Non-simple-name defaults are evaluated in parameter declaration order.

### 12.3.1 Matching: formal objects

86. Mode in is matched by an expression of the same type.
87. At instantiation the mode-in expression is checked as if by an explicit constant declaration.
88. CONSTRAINT_ERROR if the mode-in value fails the subtype check.
89. Mode in out is matched by a variable name of the same type.
90. The variable cannot be a formal parameter of mode out or a subcomponent thereof.
91. The variable must satisfy the renaming rules (RM 8.5).
92. (Note) Mode-in actual type must not be limited.
93. (Note) Mode-in-out constraints come from the actual.

### 12.3.2 Matching: formal private types

94. Non-limited formal private: matched by any non-limited type/subtype.
95. Limited formal private: matched by any type/subtype, limited or not.
96. With discriminants: actual must have the same number of discriminants.
97. Discriminant types must match by position.
98. With discriminants: the actual subtype must be unconstrained.
99. Without discriminants: the actual may have any number of discriminants.
100. An unconstrained formal name cannot match an unconstrained array actual where a constraint is contextually required.
101. Likewise for unconstrained discriminated actuals.
102. The contextual-constraint rule follows 3.6.1 / 3.7.2.
103. Types derived from a formal type follow the same constraint restrictions.
104. At instantiation, discriminant subtypes of actual and formal must match.
105. CONSTRAINT_ERROR if they do not.

### 12.3.3 Matching: formal scalar types

106. `(<>)` matched by any enumeration or integer subtype.
107. `range <>` matched by any integer subtype.
108. `digits <>` matched by any floating point subtype.
109. `delta <>` matched by any fixed point subtype.
110. No other matches are possible.

### 12.3.4 Matching: formal array types

111. Same dimensionality required.
112. Both constrained or both unconstrained.
113. Same index type at each position.
114. Same component type.
115. Non-scalar component subtypes both constrained or both unconstrained.
116. At instantiation, component constraints checked equal.
117. At instantiation, index bounds checked equal at each position.
118. CONSTRAINT_ERROR on failure of 116/117.
119. (Note) Formal index/component types that are themselves formal use the actual subtypes.
120. (Example) Constrained/unconstrained correspondence is load-bearing (MIX vs TABLE).

### 12.3.5 Matching: formal access types

121. Matched by an access subtype with the same designated type.
122. Non-scalar designated subtypes both constrained or both unconstrained.
123. At instantiation, designated-type constraints checked equal.
124. CONSTRAINT_ERROR on failure.
125. (Note) Formal designated types that are formal types use the actual subtype.

### 12.3.6 Matching: formal subprograms

126. Matched by subprogram/entry/enumeration literal with the same parameter and result type profile (6.6).
127. Parameter modes must be identical at corresponding positions.
128. Name default must denote a matching subprogram/entry/literal.
129. Name defaults are evaluated at each instantiation that uses them.
130. Box default: omitted actual takes the directly visible matching subprogram/entry/literal with the same designator at the instantiation point.
131. Box default: exactly one directly visible match required; zero or several = illegal.
132. (Note) Type attributes with matching signatures can match.
133. (Note) Enumeration literals match parameterless formal functions of the type.
134. (Note) Matching parallels the 8.5 renaming rules.
135. (Note) Parameter names need not match.
136. (Note) Default expressions need not match.

### Visibility and scope (8.x in generic context)

137. Generic formal parameters are declared in the generic's declarative region (8.1).
138. Their scope extends to the end of the generic unit's scope (8.2).
139. Generic parameter declarations are visible by selection in named associations (8.3(f)).
140. Formal parameter visibility follows the rules for other formal parameters (8.3).

### Consistency and cross-references

141. Instantiation legality (12.3.1–12.3.6 matching) is checked at instantiation.
142. Instance elaboration includes the 12.3.1–12.3.6 dynamic checks.
143. Generic library units follow standard library unit rules (10.1, 10.5).
144. Recursive instantiation (direct or cyclic) is prohibited.
145. Formal-part default expressions are not evaluated until an instantiation uses them.
146. Operations of formal types are implicitly declared and available in the template body.
147. A generic body must exist for each generic subprogram declaration.
148. Instance copy semantics exclude the formal part.
149. Box defaults resolve at the instantiation point to directly visible entities.
150. Profile matching requires identical mode correspondence (12.3.6, 8.5).

---

## 5. GNAT reference architecture (sem_ch12)

GNAT also uses macro expansion, structured as three stages
(sem_ch12.adb:96-150):

- **Stage 1 — generic declaration:** copy the generic tree, fully analyze
  the copy, save non-local (global) references into the original tree.
- **Stage 2 — instantiation:** copy the original tree, insert renaming
  declarations mapping formals to actuals, re-analyze the instance as
  ordinary code.
- **Stage 3 — delayed bodies:** instantiate bodies at the end of the
  compilation unit (`Pending_Instantiations`).

### 5.1 Template analysis (`Analyze_Generic_Package_Declaration`, :4443)

`Copy_Generic_Node(N, Empty, Instantiating => False)` (:4513) produces an
undecorated structural copy. `Analyze_Generic_Formal_Part` (:4576) then
`Analyze (Specification (N))` (:4581) run inside
`Push_Scope` + `Enter_Generic_Scope`, with formals standing in as real
(formal) types. After analysis, `Save_Global_References (Templ)` stamps the
original tree. Invariant: **the template is analyzed once; the decorated
copy establishes which references are global via scope-chain traversal.**

### 5.2 Instantiation (`Analyze_Package_Instantiation`, :4826)

1. `Preanalyze_Actuals` + `Init_Env` + `Generic_Renamings.Clear`
   (:5016-5026).
2. Circularity check: `In_Open_Scopes (Gen_Unit)` and
   `Contains_Instance_Of (Gen_Unit, Current_Scope, Gen_Id)` (:5129-5140).
3. `Copy_Generic_Node (..., Instantiating => True)` (:5171-5173) with sloc
   adjustment (`S_Adjustment`) so diagnostics carry "instance at" context.
4. `Analyze_Associations` (:5174-5259) inserts the formal-binding
   declarations (see 5.4) before the visible declarations.
5. Private-view bookkeeping (`Check_Private_View`, :9172; see 5.6).
6. The instance spec is analyzed as a normal package; the renamings are
   ordinary declarations and ordinary visibility does the rest.

### 5.3 `Copy_Generic_Node` (:9418)

Recursive structural copy: `Copy_Generic_Descendant` on all children
(:9463-9483), `Copy_Generic_List`/`Copy_Generic_Elist` for collections
(:9489-9537), sloc adjustment when instantiating (:9567+). **No semantic
fields are copied** — Entity/Etype are filled by re-analysis.

### 5.4 Formal binding (`Instantiate_Type` :14321, `Instantiate_Object`
:12901, `Instantiate_Formal_Subprogram` :12531)

Each formal becomes a *declaration in the instance*:

- Formal type → `subtype F is Actual;` after per-kind validation
  (:14354-14362): `Validate_Private_Type_Instance` (:14599 — limitedness,
  definiteness), `Validate_Derived_Type_Instance` (:14817 — ancestor and
  discriminant conformance), `Validate_Array_Type_Instance` (:14664 — index
  types and constraints), `Validate_Access_Type_Instance` (:14573 —
  designated type conformance), plus incomplete/interface validators for
  later Ada.
- Formal object mode in → `F : constant Ftyp := Actual;` (:13045+).
- Formal object mode in out → `F : Ftyp renames Actual;` (:12950-12962),
  with renaming-legality checks (:12983-12993).
- Formal subprogram → `function/procedure F ... renames Actual_Subp;`;
  box defaults resolve via direct visibility at the instantiation point.

A hash table (`Generic_Renamings`, :993-1045) maps formal entity → actual
entity for validation lookups (`Get_Instance_Of`) and nested generics.

### 5.5 `Save_Global_References` (:17313)

Walks the original (unanalyzed) tree after template analysis. `Is_Global (E)`
(:17396) tests whether the entity's scope chain escapes the generic;
`Reset_Entity` (:17352) stamps global references onto the original nodes;
`Save_Entity_Descendants` / `Save_Global_Descendant` (:17356, :17378)
recurse. **Only global references are stamped; local references stay blank
and resolve fresh in each instance.** This is what makes RM 12.3 rules
(h)/(i) hold under re-analysis at an arbitrary instantiation point.

### 5.6 Private view exchange (`Check_Private_View` :9172, `Switch_View`
:18778)

If a type is private in the template's view but full at the instantiation
point (or vice versa), nodes flagged `Has_Private_View` (:18609) trigger
`Switch_View` → `Exchange_Declarations` (partial ↔ full view in the symbol
table), tracked in `Exchanged_Views` and restored LIFO after the instance is
analyzed (:17072+).

### 5.7 Delayed body instantiation (:210-231;
`Instantiate_Package_Body` :13293, `Instantiate_Subprogram_Body` :13991)

Specs are instantiated immediately; bodies are queued
(`Pending_Body_Info` → `Inline.Pending_Instantiations`) and instantiated at
the end of the unit — this breaks spec/body circular dependencies (A's body
instantiates a generic of B and vice versa). Body instantiation reloads the
generic body if needed (`Load_Parent_Of_Generic`, :13605+), reinstalls the
enclosing scopes (:13618+), copies, inserts, analyzes, restores. Freezing
has already happened by then.

### 5.8 Nested generics (:168-208, `Current_Instantiated_Parent` :1023)

When copying an outer instance that contains a nested generic,
`Current_Instantiated_Parent` records the (generic → instance) pair, and
`Save_Global_References` uses scope depth against it so that references in
the nested generic to the *outer generic's* locals/formals are NOT preserved
(they must re-resolve inside the outer instance), while truly global
references are.

### 5.9 Circularity (:234-249, :5129-5140)

`In_Open_Scopes` (self-instantiation while being analyzed) plus
`Contains_Instance_Of` (cycle through instance-of links); sets
`Circularity_Detected` and rejects at compile time.

### 5.10 Load-bearing invariants to replicate

1. **Template analyzed once; instances are re-analyzed copies.** Globals
   captured at template analysis; locals resolve fresh.
2. **Copy-and-bind, not substitute.** Formal→actual is expressed as
   ordinary declarations (subtype / constant / renaming) inserted into the
   instance; no per-emission substitution map exists anywhere.
3. **Private-view discipline** between declaration point and instantiation
   point.
4. **Nested-generic scope filtering** so only truly global references
   survive copying.
5. **Delayed bodies** to break spec/body circularity.
6. **Compile-time circularity rejection.**

Measured against these, the current system substitutes at emission time with
no re-analysis, no legality validation, no global-reference discipline, and
no circularity detection.

---

## 6. Findings

Ordered by severity. Each names the RM rules violated (§4 numbering) and the
observable evidence.

### F1. Instances are never semantically re-analyzed (RM 64-77, 26, 41-42)

RM 12.3 defines an instance as *a copy of the generic unit* in which formal
names denote the actuals and everything else re-derives. The current emit
path walks one shared resolved template with a substitution map. Everything
semantic analysis computes per declaration — frozen sizes, constraint
ranges, overload bindings, task-master flags, static values — is computed
once against *formal* types and reused for every instance. Each consumer of
a type property in the emitter needs (and sometimes misses) a
`Resolve_Generic_Actual_Type` call. cc1221b (`'BASE'FIRST`), cc1302a
(`'PRED`/`'SUCC`), c67002c (formal `"="` boolean width) are this one defect
at different sites; §3's five consequences are its body-level expression.
This is the structural root; most cc failures trace to it.

### F2. RM 12.3.1–12.3.6 matching rules are essentially unimplemented (RM 86-136, 141)

The bc suite's 141 WRONG_ACCEPT measures this. Unchecked at instantiation:

- **12.3.2 formal private** (94-105): limitedness, discriminant
  count/type/position matching, actual-unconstrained requirement.
- **12.3.3 formal scalar** (106-110): class membership only loosely checked.
- **12.3.4 formal array** (111-115): dimensionality, per-position index
  types, component type, constrained-ness agreement — "any actual array
  accepted."
- **12.3.5 formal access** (121-122): designated-type matching.
- **12.3.6 formal subprogram** (126-127): profile and mode conformance.
- **12.3.1 formal object** (86, 89-92): expression-ness, non-limited type,
  renameable-variable requirement.
- **12.3(61)**: named associations with duplicate subprogram designators.

### F3. Instantiation-time constraint checks are missing (RM 81, 87-88, 104-105, 116-118, 123-124, 142)

Elaboration of an instantiation must perform the dynamic checks and raise
CONSTRAINT_ERROR: mode-in value vs formal subtype, discriminant subtype
agreement, index bounds and component-constraint agreement, designated-type
constraint agreement. The ~23-test cc3406/3407/3408/cc3504 family fails in
both directions (not raised when required; raised when not — cc3305d).

### F4. Formal objects have macro semantics, not RM 12.1.1 semantics (RM 16-24, 67-68, 87-88)

A formal object reference in the template is replaced by the actual
*expression tree* during cloning (ada83.c:43949-43964). Required: mode
**in** = a constant initialized once with a copy of the actual's value,
checked against the formal subtype; mode **in out** = a variable renaming
the actual. Expression splicing re-evaluates the actual at every use (wrong
under side effects or mutation of its operands), breaks `in out` aliasing
entirely, and bypasses the 12.3.1 value check. Modes are parsed
(`GEN_MODE_*`) then ignored. cc3121a, cc1010a.

### F5. Formal-object substitution matches by name, capturing homographs (RM 67-68, 73)

The cloner decides "is this identifier a formal object?" by case-insensitive
name comparison against the formal list (ada83.c:43953-43958). A local
redeclaration of the same name inside the template (an inner block's
variable, a parameter) is wrongly substituted — name capture, the classic
macro bug. RM 12.3 rules (b)/(c)/(h) distinguish by *denotation*, not
spelling.

### F6. The clone_kids table is badly incomplete

25 kinds covered; the ~35 kinds in §2.9 shallow-aliased — every
type-definition kind, every constraint kind, allocators, component and
discriminant declarations, variants, renamings, exception handlers, task
specs/bodies, and (critically) nested `NK_GENERIC_DECL` / `NK_GENERIC_INST`
/ `NK_GENERIC_*_PARAM`. Any instance-specific mutation of these corrupts
every other instance and the template. A subtype declaration whose
constraint mentions a formal (`SUBTYPE S IS T RANGE F .. L`) gets no
substitution at all.

### F7. The template is never analyzed at declaration time (RM 8, 55-58)

`NK_GENERIC_DECL` resolution (ada83.c:15770-15806) stores formals + unit and
stops. Formals enter a scope only when a *body* is found; spec-only
generics, or generics whose bodies are never demanded, are never checked.
Template errors surface at first instantiation or never — wrong for class B,
and the reason F8 was never built.

### F8. RM 12.1.2 operation sets are not modeled or enforced (RM 26-28, 32-43, 76, 146)

Inside the template: formal limited private must offer neither assignment
nor `"="`; formal private assignment + equality only; formal discrete the
RM 3.5.5 set; formal integer/float/fixed their predefined operator sets. At
instantiation each implicit operation must rebind to the *predefined*
operation of the actual even if redefined (41-42). None of this is modeled:
the template is checked (when at all) against whatever the
`TYPE_PRIVATE`/`TYPE_INTEGER` stand-ins permit, and instance binding takes
whatever the emitter's substitution finds.

### F9. No circular-instantiation detection (RM 10, 82, 144)

The cloner's depth-500 limit catches runaway recursion with a misleading
diagnostic instead of the required compile-time rejection of direct or
mutual cycles. GNAT: `In_Open_Scopes` + `Contains_Instance_Of`. The local
analogue is the `Loading_Set` pattern already used for circular WITH chains.

### F10. Instance body elaboration is conditional on an accident (RM 64, 80-81)

Instance body statement sequences are emitted only when the *resolved
template body* happens to exist (`template->generic_body`); the clone path
is unusable (F1) and the code skips elaboration as an "honest gap"
(ada83.c:38943, 39003). The c35003b false-pass class (§3 item 1) lives here,
as does the per-instance state problem (§3 item 2) and the undefined
spec-declared nested instances (§3 item 3, cd3015f).

### F11. Generic body discovery is by filename convention (RM 57, 143, 147)

`Expand_Generic_Package` finds bodies via `Lookup_Path_Body`
(`<package>.adb`/`.ada`). A generic declared inside a package (body in the
enclosing package body), or several generics in one file, fall outside the
convention. Body association must follow unit structure, not the filesystem.
The survey adds the `separate` case: generic units with `separate` bodies
are not connected post-clone — and subunits in general are parse-only
(`is_separate` is set, but stub and separate body are never connected), so
the generic fix here is bounded by the §17 subunit gap (an accept-and-ignore
feature in its own right; see the appendix's recommendation 5).

### F12. Formal subprogram defaults are partially right, subtly wrong (RM 11-12, 44-54, 128-133)

Box defaults resolve by visibility at the instantiation point — present —
but the "exactly one directly visible match" rule (131) is unenforced. Name
defaults must denote a profile-matching subprogram and be *evaluated at each
instantiation using them* (129); current handling resolves them once in
template scope. Default expressions of the actual subprogram's own
parameters must be usable at calls through the formal (12; cc3606a fails;
cc1311a adjacent). Attribute actuals (`T'SUCC`) and enumeration-literal
actuals exist but bypass profile checking (F2).

### F13. Mode-out subcomponent restriction unchecked (RM 90-91)

An `in out` actual may not be a formal parameter of mode out or a
subcomponent thereof, and must satisfy the RM 8.5 renaming rules. Distinct
from F2/F4 though it lands in the same validator.

### F14. No instantiation backtrace in diagnostics

Errors arising inside an instance (once instances are re-analyzed) need the
GNAT "instance at <site>" convention (S_Adjustment, §5.2 step 3); today
there is none. Without it the Phase 3 rewrite makes diagnostics worse before
better.

### F15. Generic knowledge is smeared across four subsystems (survey finding)

The subsystem survey's architectural verdict on §16 (full text in the §12
appendix; inventory in §2.11): `generic_formal_index` is stamped at parse
time (§9, ada83.c:4788), substituted at clone time (§16), and consulted at
codegen time (§13: `Generate_Lvalue`, `Underlying_Record_Type`) — three
phases sharing one integer slot, with the clone-state contract (which node
fields must be null, which survive, who re-stamps) implicit across all four
sections. The visible symptoms: two `unsupported: generic formal object
substitution cycle` diagnostics (ada83.c:20322, 20663) marking the
substitution model's frontier; the same formal-lvalue substitution logic
duplicated between `Generate_Lvalue` (~20314) and the assignment path
(~31344); and `Type_To_Rep` (§10.7) calling `Resolve_Generic_Formal` at
~7997, making "what is this type's representation" context-dependent —
exactly what type freezing (§10.6) exists to prevent, and part of the
survey's top-priority coupling problem (LLVM lowering policy inside §10).

The §12↔§16 instantiation cycle itself is inherent to the macro model and
acceptable; the defect is that nothing contains it. Phase 3 is the
containment: instantiation becomes a resolution-time event (clone + bind +
resolve), after which the clone is ordinary code — §13 emission knows
nothing about generics, the parse-time stamp and both §13 substitution
sites die with the substitution model, and the `Type_To_Rep` formal-
resolution call dies with `Resolve_Generic_Formal`. The survey's
recommended `Type_To_Rep`-to-§13 move is adjacent work this plan unblocks
but does not require.

---

## 7. The plan

### End state

When done, the codebase **has**:

- template semantic analysis at the generic declaration, with formal types
  carrying their RM 12.1.2 operation sets;
- instantiation as deep-copy + formal-binding declarations + full
  re-resolution through the ordinary resolver;
- one ordinary-package/subprogram emission path that instances share with
  non-generic code; per-instance symbols, types, storage, and bodies;
  instance body statements elaborated at the instantiation point per
  RM 12.3;
- RM 12.3.1–12.3.6 legality validation of every actual;
- instantiation-elaboration constraint checks raising CONSTRAINT_ERROR;
- compile-time circularity detection;
- unit-structured generic body association;
- "in instantiation at <site>" diagnostics.

The codebase **does not have**: `Resolve_Generic_Actual_Type`,
`Resolve_Generic_Formal`, `Substitute_Generic_Formal_Subprogram`,
`cg->current_instance`, `Reset_Template_Extern_Flags`,
`Find_Homograph_Body`, `Generate_Generic_Instance_Body`, the
`generic_actuals` substitution arrays, name-based formal-object splicing,
the cc1310a statements gate, the export-alloca + assignment-scan block,
`Delay_Cleanups`-style flag transfers between template and instance, or the
partial `clone_kids` table.

Each phase lands suite-green (the suite is the landing constraint, not the
direction; PASS deltas during the work are noise). Order matters: the cloner
and template analysis are prerequisites for the rewrite; legality checks are
independent of it and front-loaded in parallel because they are pure adds
with a large bc payoff.

### Phase 1 — Complete the cloner (fixes F6)

Make the child-edge table total over the node-kind enum, eliminating every
row in §2.9. One shape table is the single source of truth for "what are a
node's children"; derive the ALI serializer's walk (§14) from the same table
so the two cannot drift. Enforcement: a static assertion or debug-build
walker that visits every `NK_*` kind and cross-checks the union members at
ada83.c:1346-1442 against table rows — a new node kind without a table row
fails the build, not the next instantiation.

Golden test: clone a non-generic compilation unit, resolve and emit both
original and clone, require byte-identical IR. No behavior change lands in
this phase.

### Phase 2 — Template analysis at declaration (fixes F7, grounds F8, enables F5's fix)

On `NK_GENERIC_DECL`: push a generic scope and declare each formal as a real
symbol —

- **formal types** as distinct `Type_Info`s tagged with their formal class
  and carrying the RM 12.1.2 operation set for that class (discrete →
  RM 3.5.5 attributes and operators; integer/float/fixed → their predefined
  operator sets per 3.5.5/3.5.8/3.5.10, including the universal_fixed
  multiplying operators in STANDARD (40); private → assignment + predefined
  equality; limited private → neither; array → 3.6.2; access → 3.8.2);
  formal-type attributes are non-static (RM 14);
- **formal objects** as constant (mode in) or variable (mode in out)
  symbols per RM 12.1.1, with the type-mark/limited/default legality checks
  (17-22, 30-31);
- **formal subprograms** with fully resolved profiles; `IS name` defaults
  recorded for per-instantiation re-evaluation (129), absorbing
  `Resolve_Generic_Formal_Subprogram_Defaults`;

then resolve the unit spec immediately and the body when it arrives (the
existing body-completion path at ada83.c:14777-14995 becomes the body half
of this, no longer the only half). Template errors now surface at the
declaration; spec-only generics get checked; bc tests for illegal template
bodies start converting. The analyzed template also decorates every
identifier with its denotation — the input Phase 3 needs to distinguish
formal references from local homographs (F5) and global references from
local ones (RM 73-74).

### Phase 3 — Tree-copy instantiation (fixes F1, F4, F5, F10, F11, F14)

This is the `docs/generics-instantiation.md` design, upgraded by this audit.

**At instantiation (resolution time):**

1. **Clone** the generic's spec+body AST with the Phase 1 total cloner — a
   deep structural copy, mechanical over the shape table. Cloned nodes get
   fresh `symbol = NULL`, `type = NULL` (re-resolution owns them); source
   locations kept pointing at the template, with diagnostics prefixed by
   the instantiation site (GNAT's "instance at ..." convention — F14).
   The cloner performs **no expression substitution at all** — `KIDS`-table
   copy only. F5's name capture becomes structurally impossible because
   nothing is spliced.
2. **Bind formals as declarations in a fresh instance scope**, per GNAT
   (§5.4), not as splices:
   - formal type → the instance scope's name aliases the actual `Type_Info`
     directly (a subtype view; no per-emission mapping);
   - formal object mode **in** → an actual constant declaration in the
     instance: one evaluation of the actual expression (78-79: evaluation
     order is per-declaration), with the RM 12.3.1 subtype check attached
     (87-88);
   - formal object mode **in out** → a renaming binding to the actual
     variable's lvalue (23-24, 89-91);
   - formal subprogram → a symbol bound to the actual subprogram, entry,
     enumeration literal, attribute, or predefined operator (126, 132-133;
     entries included — uniform, replacing the `actual_subprogram` special
     cases);
   - formal-kind legality checks (`TYPE TT IS (<>)` etc.) run at the
     binding scope — the existing checks move here, they don't grow.
3. **Resolve the clone** with the ordinary resolver, **in the scope of the
   generic declaration plus the formal bindings — not the instantiation
   scope.** This yields RM 12.3 rule (i) — globals denote the same entities
   as in the template — without porting GNAT's `Save_Global_References`;
   the Phase 2 decorated template is the cross-check (assert the clone's
   global resolutions match the template's). Everything downstream —
   `is_task_master`, master scope ids, access-to-task tagging, frozen
   sizes, overload binding, the family-rename slot — computes per instance
   with zero new code, because the clone is just a package.
   The one place declaration-scope resolution is insufficient is private
   types whose view differs between the declaration and instantiation
   points (GNAT's `Switch_View`, §5.6). Ada 83 ACATS exercises this
   lightly — resolve against the full view and document the deviation, or
   port the view exchange if bc demands it.
4. **Emit the clone** through the ordinary `Generate_Declaration` paths.
   Nested instance → nested package (frame storage, inline body statements
   at the declaration point — RM 7.2/12.3 — via the existing nested-package
   path at ada83.c:~37870). Library instance → globals + `__elab` function
   through the existing library path and the §15 elaboration graph. The
   "resolved template body or nothing" gates (ada83.c:38938-38944,
   39001-39005) disappear: the clone IS resolved.

**Body discovery** moves from filename convention to unit structure: a
generic body is associated when its enclosing unit's body is parsed, like
any other body completion (F11). The disk lookup (`Lookup_Path_Body` +
`generic_body_cache`) remains only as the library-unit loading path under
§17's normal rules, and bodies still parse once per template (the clone, not
the parse, is per-instance).

**Sharing/limits:** clone per instantiation, no body sharing (Ada 83 has no
shared-generic requirement; ACATS instantiation counts are small).

**Implementation notes pinned during execution** (verified against the
code, not speculative):

- **`Clone_Subtree` semantic reset list.** The pure structural cloner walks
  `Syntax_Tree_Shape` (§8) and must reset on every cloned node: `symbol`,
  `type`, `folded_valid`, `apply.resolution` (→ `APPLY_UNRESOLVED` — its
  own comment promises this of cloned nodes), and the payload symbol
  fields `loop_stmt.label_symbol`, `exit_stmt.target`, `goto_stmt.target`,
  `label_node.symbol`, `accept_stmt.entry_sym`, plus
  `subprogram_body.code_generated` and the master fields
  (`is_task_master`, `master_scope_id`) — instance masters are computed by
  the instance's own resolution. Cloning the *resolved* template is
  correct (GNAT clones analyzed trees) provided resolution's in-place
  tree rewrites are idempotent; any non-idempotent rewrite the suite
  exposes is a resolver bug to fix, not a cloner workaround.
- **Scope re-rooting exists.** Scopes are parent-chained
  (`Scope.parent`), `Symbol_Manager_Push_Existing_Scope` is in the API,
  and symbols carry `defining_scope` — so "resolve the clone in the scope
  of the generic declaration plus the formal bindings" is: save
  `sm->current_scope`, set it to the template symbol's `defining_scope`,
  push a fresh instance scope, bind formals, resolve the clone, restore.
- **Why GNAT wraps subprogram instances in an anonymous package.** A
  formal object of a generic *subprogram* elaborates once, at the
  instantiation (RM 12.3(80)) — not per call. Its storage is
  instance-level, not frame-level; the binding declarations therefore
  need a package-like home whose elaboration runs at the instantiation
  point. The instance model must give subprogram instances exactly that:
  per-instance statics for mode-in constants and in-out renamings,
  initialized in the enclosing elaboration flow, with the instance
  subprogram reading them as uplevel/global state. (The current
  machinery's per-call frame materialization at ada83.c:37893+ is wrong
  in both directions: re-evaluates the actual on every call, in the
  caller's context instead of the instantiation's.)

**Stage-2 (package instances) worklist, pinned mid-execution** — stage 1
(subprograms) is landed; this is the live worklist for the package cut:

1. `Instantiate_Generic_Package (inst_sym, template)` mirroring the
   subprogram one: clone `template->generic_unit` (spec) and
   `template->generic_body` (or the disk-parsed body — parse via the §17
   loader at instantiation when absent), rename to the instance, re-root to
   `template->defining_scope`, push instance scope, bind formals (same
   binding forms + remap pairs as stage 1), resolve spec
   visible+private declarations, `Mark_Package_Level_Objects` (their
   `parent = inst_sym`, `is_package_level = true` → per-instance global
   mangling `<inst>__<name>` already classifies via `Symbol_Is_Global`'s
   instance-package arm), populate `inst_sym->exported` from the resolved
   clone's declaration symbols (this REPLACES the SUBSTITUTE_TYPE /
   RESOLVE_EXPORT_OR_GLOBAL_TYPE export-cloning block), then resolve the
   body like the package-body completion path does (install spec symbols,
   resolve declarations/statements/handlers).
2. Emission through ordinary package paths keyed off `expanded_spec/_body`
   being RESOLVED clones: package-level globals emit as for any package;
   subprogram bodies inside the instance emit via `Generate_Subprogram_Body`
   on the clone's body declarations (each spec->symbol points at its own
   per-instance symbol — no `Find_Homograph_Body`); body statement
   sequence emits inline at the instantiation point (nested, RM 7.2/12.3)
   or into the instance `___elab` (library level) — binding elaboration
   first, then statements, replacing the "resolved template body or
   nothing" gates.
3. Outside visibility: `INST.X` resolution reads `inst_sym->exported`
   (now real per-instance symbols); USE-clause aliasing unchanged.
4. Sub-checks: two instances of one stateful generic (per-instance body
   state); spec-declared nested instantiation (cd3015f CHECK_HUE);
   SEQUENTIAL_IO/DIRECT_IO (disk template body, ce2102-class); cc1310a
   stays green; c35003b substance executes.
5. Then stage 3: the sweep (disposition table §8.10) — delete
   `Resolve_Generic_Actual_Type`, `Resolve_Generic_Formal`,
   `Substitute_Generic_Formal_Subprogram`, `cg->current_instance`,
   `Generate_Generic_Instance_Body`, `Find_Homograph_Body`,
   `Expand_Generic_Package`, `Node_Deep_Clone`'s substitution,
   `generic_actuals` arrays (Phase 4 validators get the resolved actuals
   another way: keep the slot-resolution pass outputs as locals),
   `generic_formal_index` fields, the cc1310a gate, the §16 macro-style
   header comments.

**Staging within the phase** (each step lands suite-green):

1. Generic **subprogram** instantiations switch to clone+resolve+emit
   (smaller surface: no exports, no spec/body split). Delete their slice of
   the substitution machinery. Suite gate: cc-subprogram families (cc3601*,
   cc3606*).
2. Generic **package** instantiations switch. The big cut: exports become
   ordinary declarations of the clone; body state materializes
   per-instance; body statements elaborate; spec-declared nested instances
   become ordinary nested instantiations of the clone (cd3015f's CHECK_HUE
   defines itself). Delete the export-alloca block, the assignment scan,
   the homograph fishing, the cc1310a gate.
3. **Sweep the substitution API dead.** Delete `Resolve_Generic_Actual_Type`
   first; let the compiler list every survivor — each is either dead or a
   missed clone-path conversion. Same for `cg->current_instance`,
   `Resolve_Generic_Formal`, `Substitute_Generic_Formal_Subprogram`, the
   `generic_actuals` arrays, `Generate_Generic_Instance_Body`,
   `Find_Homograph_Body`, `expanded_spec`/`expanded_body`,
   `Generic_Formal_Substitution_Depth`. The compiler is the search tool; a
   refactor is done iff zero call sites of the old API remain. The sweep
   reaches beyond §13: `Type_To_Rep`'s formal-resolution call (~7997, §10),
   the duplicated lvalue substitution (`Generate_Lvalue` ~20314, assignment
   path ~31344), the two `substitution cycle` diagnostics (20322, 20663),
   and the parse-time `generic_formal_index = -2` stamp (4788) all die
   here — this is what un-smears generic knowledge from §9/§10/§12/§13
   back into §16 (F15).
4. **Re-baseline.** Expect HONEST count movement: false passes (c35003b
   class) become real results — some convert to PASS outright (statements
   now correct), some expose the next true gaps (generic-formal subtype
   range checks, c35003b E1). Document the delta; the signal is which
   failures are now REAL.

### Phase 4 — Legality validation of actuals (fixes F2, F12, F13)

A per-formal-kind validation pass at the head of instantiation resolution,
mirroring GNAT's `Validate_*_Instance` family (§5.4), covering RM items
86-136 plus 61-62:

- **private** (94-105): limitedness vs formal's limitedness; discriminant
  count, types by position, actual-unconstrained requirement; the
  contextual-constraint restrictions (100-103);
- **scalar** (106-110): exact class membership;
- **array** (111-115): dimensionality, per-position index types, component
  type, constrained/unconstrained agreement, component-subtype
  constrained-ness for non-scalar components;
- **access** (121-122): designated-type identity, designated-subtype
  constrained-ness agreement;
- **object** (86, 89-92): mode in — expression of the type, non-limited;
  mode in out — variable, renameable per 8.5, not an out-parameter or
  subcomponent thereof (90, F13);
- **subprogram** (126-136): full parameter/result type profile per 6.6 with
  identical modes; attribute and enumeration-literal actuals validated
  against the profile; box default "exactly one directly visible match"
  (130-131); name defaults re-resolved per instantiation (129); actual's
  own parameter defaults preserved for calls through the formal (12,
  cc3606a);
- **associations** (60-63): named-association prohibition under duplicate
  subprogram designators (61); missing-actual-without-default rejection
  (62).

Largely independent of Phase 3 and worth ~100+ bc conversions; can land in
parallel slices (one formal kind per slice, each naming the bc tests it
converts).

### Phase 5 — Instantiation-elaboration constraint checks (fixes F3)

With Phase 3's model these become ordinary checks on ordinary declarations:
the mode-in constant declaration gets the standard subtype check via the
§13.2 `CHK_*` machinery (87-88, suppressible per pragma Suppress like any
other check); discriminant-subtype agreement (104-105), index-bounds and
component-constraint agreement (116-118), and designated-type constraint
agreement (123-124) emit at the instance's elaboration point and raise
CONSTRAINT_ERROR. Gate: the cc3406/cc3407/cc3408/cc3504 family, plus
cc3208a and the raised-inappropriately direction (cc3305d).

### Phase 6 — Circularity detection (fixes F9)

An `Instantiating_Set` mirroring §17's `Loading_Set`: push the template
symbol on instantiation entry, reject re-entry with "recursive generic
instantiation" naming the cycle, pop on exit. Covers direct and mutual
recursion (10, 82, 144) — including the survey's nested-instantiation case
(a generic instantiating another generic), which Phase 3 makes an ordinary
recursive instantiation of the clone and this phase bounds. The cloner's
depth-500 guard remains only as a backstop against non-instantiation
pathologies.

### Adjacent work the plan unblocks (survey-ordered, not in scope)

From the survey's recommended order of work (§12 appendix, recommendation
list). Not required for generic correctness, but each touches the same
ground and is sequenced by this plan:

- **`Type_To_Rep` + fat-pointer helpers move from §10 to §13** (survey's
  highest-value structural fix). Phase 3's sweep removes the
  `Resolve_Generic_Formal` call inside it, deleting the generic half of the
  layering violation; the move itself is then purely a §10/§13 concern,
  mechanical via delete-first.
- **Elaboration edge wiring** (`EDGE_WITH`, `EDGE_ELABORATE`,
  `EDGE_ELABORATE_ALL` from resolved context clauses and pragmas). Instance
  `__elab` functions (W10) are registered in a graph that today builds only
  `EDGE_SPEC_BEFORE_BODY`; correct cross-unit instance elaboration order
  inherits whatever §15 provides. Small wiring job per the survey; gives
  §14's recorded elaboration pragmas their missing consumer.
- **Subunit (`separate`) compilation** (§17 gap). F11's unit-structured
  body association is designed not to preclude it; `separate` generic
  bodies become ordinary subunits once stub-to-body connection exists.
- **Accept-and-ignore audit** (enum/record rep clauses, address clauses,
  `NK_CODE`): the survey's "implement or diagnose, never silently drop"
  rule is the same principle this plan applies to generic legality
  (rejecting what the old system silently accepted).

---

## 8. Refactor discipline and disposition table

The end result must not contain new code alongside old dead code, artifacts
of the old design, or a halfway-done refactor. These are the mechanisms that
make that outcome structural rather than aspirational. The core discipline
is the repository's standing rule: **delete the old definition first; the
compiler is the search tool; a refactor is done iff zero call sites of the
old API remain.**

### 8.1 The disposition table comes before the first edit

§2 enumerates every piece of generic machinery with file:line. The table in
§8.10 extends it: every symbol, struct field, and code range gets exactly
one verdict — **DELETE** (no successor of the same shape), **MOVE** (the
logic survives; the API, location, or representation dies), or **KEEP**.
An unclassified item means the change is not yet understood — do not start.
The table is the contract; the final review (§8.8) audits against it. It
catches the artifacts grep never will: struct fields, sentinel values,
decoration stamps — not just functions.

### 8.2 Delete the old definition first; the compiler is the worklist

Each phase slice begins by deleting its old API and struct fields from the
§N spec block and the body block. The resulting `gcc` error list is the
exact, complete worklist of sites to convert. The conversion rule at every
site: rewrite the site to the new model. Never an adapter, never a wrapper
preserving the old signature, never a "temporary" forwarding helper — those
are the fake-refactor patterns the repository rules already ban.

### 8.3 Struct fields are the real test

Functions are easy to delete; fields linger. `generic_actuals` /
`generic_actual_count`, `expanded_spec` / `expanded_body`,
`generic_body_cache`, `cg->current_instance`,
`Generic_Formal_Substitution_Depth`, and the three `generic_formal_index`
fields are deleted *from their structs*, not merely left unwritten — the
compiler then finds every read and write. A field still present in a struct
at landing is an old-design artifact even if nothing assigns it.

### 8.4 Banned shim patterns

During the transition, none of the following may exist, even briefly, in a
landed commit:

- `#ifdef` old/new switches or runtime toggles selecting a generics path;
- `if (is_instance) <new path> else <old path>` forks for the same
  construct;
- any helper that takes an instance and a formal and answers "what is the
  actual?" — that is `Resolve_Generic_Actual_Type` reborn under a new name;
- substitution logic anywhere in the cloner — the cloner is a pure
  structural copy (shape-table walk), full stop; substitution logic
  existing anywhere means the old design is alive;
- a secret string/array side-channel preserving the `generic_actuals` slot
  protocol behind a new signature.

### 8.5 Interim states are scoped by construct, never duplicated per construct

Phase 3 stages subprogram instantiations before package instantiations.
That is acceptable only under this rule: at every landed commit, each
construct is served by exactly one mechanism. When subprograms switch to
clone+bind+resolve, the subprogram slice of the substitution machinery is
deleted **in the same commit**. Two mechanisms for the same construct never
land. The transition window where two mechanisms exist for *different*
constructs is bounded to the interior of Phase 3 and closed by its step 3
sweep.

### 8.6 The landing proof is compiler-mechanical, not grep

grep does not parse C — identifiers nest, strings hide tokens, macros
expand silently. The proof that the old design is gone is: each name on the
DELETE list has its declaration removed and the build is clean. Zero
survivors, compiler-verified. (grep is fine for first-look eyeballing;
never as the final answer.)

### 8.7 Comments die with the code they describe

The honest-gap comments at ada83.c:38938-38944 and 39001-39005 ("the
expanded_body clone is unusable here…"), the cc1310a three-clause gate
comment, and every other comment describing substitution-at-emission are
removed with the code. A surviving comment that references the old design
is an artifact. Replacement comments are evergreen — they describe the
clone+bind+resolve model, not the history of getting there.

### 8.8 Final adversarial review pass

After the last phase, a dedicated review of §16 and every row of the §9
wiring matrix hunts for: dual paths, dead branches, vestigial decorations
(e.g. the `generic_formal_index = -2` stamp at ada83.c:4788 surviving after
the cloner no longer substitutes), shims per §8.4, and old-design comments
per §8.7. The reviewer's standing question for every line touched: *could
this line only exist because the old design existed?* Audited against the
§8.10 table: every DELETE row absent, every MOVE row's destination present
and its source absent, every KEEP row untouched or intentionally reworked.

### 8.9 The suite is a landing constraint, not a reason to keep the old path

PASS deltas mid-refactor are noise. The temptation "keep the old path
because these three tests need it" is a signal that the new path is
incomplete — finish the new path; do not preserve the old one. (This is how
half-refactors happen: the old path stays for a tail of tests and never
leaves.)

### 8.10 Disposition table

Verdicts: **DELETE** = removed, no successor of the same shape; **MOVE** =
logic survives at the named destination, the listed API/location/
representation is removed; **KEEP** = survives (noted where reworked).
"Phase" = where the verdict executes.

#### A. Functions and APIs

| Item | Location | Verdict | Destination / replacement | Phase |
|------|----------|---------|---------------------------|-------|
| `Resolve_Generic_Formal` | ada83.c:8202-8213 (decl 1824) | DELETE | Formal type names alias the actual `Type_Info` via instance-scope binding | 3 |
| `Resolve_Generic_Actual_Type` | ada83.c:17865-17879 (decl 1826) | DELETE | Same; deleted first in the Phase 3 sweep so the compiler lists all 40+ call sites | 3 |
| `Generic_Actual_Type_By_Name` | ada83.c:14057-14064 | DELETE | Name lookup is ordinary symbol resolution in the instance scope | 3 |
| `Substitute_Generic_Formal_Subprogram` | ada83.c:23293-23307 (decl 2484) | DELETE | Formal subprogram is a bound symbol; calls resolve ordinarily | 3 |
| `Generate_Generic_Instance_Body` | ada83.c:37670+ (decl 2957) | DELETE | Instances emit through ordinary `Generate_Declaration` paths | 3 |
| `Find_Homograph_Body` | ada83.c:38990 (uses) | DELETE | Bodies are ordinary declarations of the resolved clone | 3 |
| Template `extern_emitted` flag resets ("`Reset_Template_Extern_Flags`") | inside Generate_Generic_Instance_Body | DELETE | Per-instance symbols make per-instance emission state automatic | 3 |
| `Expand_Generic_Package` | ada83.c:43989-44036 (decl 3425) | DELETE | Clone+bind+resolve at instantiation resolution | 3 |
| Export-cloning block + `SUBSTITUTE_TYPE` / `RESOLVE_EXPORT_OR_GLOBAL_TYPE` macros | ada83.c:16345-16487, 16600-16631 | DELETE | Exports are ordinary declarations of the resolved clone | 3 |
| Subprogram-instance profile substitution | ada83.c:16254-16342 | DELETE | The clone's spec resolves against bound formals | 3 |
| Deferred-bodies generic queueing | ada83.c:37361-37373, 38973-38979 | DELETE | Instance emission is ordinary; no template-body wait | 3 |
| "Resolved template body or nothing" emission gates | ada83.c:38930-38971, 38997-39019 | DELETE | Ordinary nested-package / library `__elab` elaboration of the clone | 3 |
| `Node_Deep_Clone` formal-type substitution | ada83.c:43940-43944 | DELETE | Cloner is pure structural; types resolve on the clone | 1→3 |
| `Node_Deep_Clone` formal-object name splice | ada83.c:43949-43964 | DELETE | Mode in → constant declaration; mode in out → renaming binding (RM 12.1.1) | 3 |
| cc1310a three-clause statements gate | §13 emission path | DELETE | Clone body statements emit unconditionally | 3 |
| `unsupported: generic formal object substitution cycle` diagnostics | ada83.c:20322, 20663 | DELETE | The substitution model dies; formal objects are bound declarations (no cycle possible) | 3 |
| Formal lvalue substitution, duplicated | ada83.c:~20314 (`Generate_Lvalue`), ~31344 (assignment path) | DELETE | No emission-time substitution; lvalues are ordinary symbols of the resolved clone | 3 |
| `Type_To_Rep`'s `Resolve_Generic_Formal` call | ada83.c:~7997 (§10.7) | DELETE | Dies with `Resolve_Generic_Formal`; representation queries run only on resolved clones, restoring a context-free answer (unblocks the survey's `Type_To_Rep`→§13 move) | 3 |
| `Node_Deep_Clone` structural copy | ada83.c:43918-43974 (minus substitution) | MOVE | `Clone_Subtree` — pure shape-table walk; fresh `symbol = NULL`, `type = NULL`; template slocs kept | 1 |
| `clone_kids` shape table | ada83.c:43887-43914 | MOVE | Total child-edge table over all `NK_*`; single source of truth, shared with the ALI serializer walk; build-time totality assertion | 1 |
| `Node_List_Clone` | ada83.c:43868-43877 | KEEP | Reworked: loses the `env` substitution parameter | 1 |
| Cloner depth-500 guard | ada83.c:43922-43925 | MOVE | Backstop only, behind `Instantiating_Set` compile-time rejection | 6 |
| Generic body completion path | ada83.c:14777-14995 | MOVE | Becomes the body half of Phase 2 template analysis (no longer the only analysis) | 2 |
| `Resolve_Generic_Formal_Subprogram_Defaults` | ada83.c:8157-8167 (decl 1876-1882) | MOVE | Phase 2 template analysis records `IS name` defaults; Phase 4 re-resolves them per instantiation (RM 12.3.6(129)) | 2, 4 |
| `Generic_Formal_Parameter_Types` | ada83.c:14069-14094 | MOVE | Phase 4 profile validator, operating on bound formal symbols, not substitution arrays | 4 |
| `Generic_Formal_Equality_Actual` | ada83.c:14102-14144 | MOVE | Phase 4 subprogram validator (the `"/="` → `"="` RM 6.7 rule survives) | 4 |
| Slot mapping + actual placement (positional/named) | ada83.c:15884-15956 | MOVE | Association legality (RM 12.3(60-63)) → Phase 4 validators; placement feeds the bind-formals step | 3, 4 |
| Three actual-resolution passes (type/object/subprogram) | ada83.c:15962-16250 | MOVE | The bind-formals step: subtype alias / constant declaration / renaming / bound subprogram symbol | 3 |
| Attribute, enum-literal, operator, `"/="`-negation actual handling | ada83.c:16077-16250 | MOVE | Survives as binding forms, validated against the formal's profile in Phase 4 | 3, 4 |
| `Lookup_Path_Body` / `Lookup_Path_Ext` / `Lookup_Path` | ada83.c:44077-44107 | KEEP | General library loading; no longer the generic-body *association* mechanism (that moves to unit structure) | 3 |
| `Emit_Bind_Bounded_Array_Storage` | ada83.c:36520 | KEEP | General array storage path; instances reach it as ordinary code | — |

#### B. Struct fields and globals

| Item | Location | Verdict | Destination / replacement | Phase |
|------|----------|---------|---------------------------|-------|
| `Symbol.generic_actuals[]` + `generic_actual_count` | ada83.c:2001-2013 | DELETE | Formal bindings are declarations/symbols in the instance scope | 3 |
| `Symbol.expanded_spec` / `expanded_body` | ada83.c:2014-2015 | DELETE | The instance's resolved clone tree (stored as the instance's unit AST) | 3 |
| `Symbol.generic_body_cache` | ada83.c:1997 | DELETE | Body association by unit structure; library-unit bodies parse once via §17's normal path | 3 |
| `cg->current_instance` | ada83.c:2219 | DELETE | No emission-time instance context; the clone is ordinary code | 3 |
| `Generic_Formal_Substitution_Depth` | ada83.c:2450-2454 | DELETE | Nothing substitutes | 3 |
| `Syntax_Node.generic_formal_index` + `-2` sentinel init | ada83.c:930-933, 4788 | DELETE | No name-splice verdicts; cloner does not substitute | 3 |
| `Type_Info.generic_formal_index` | ada83.c:1734 | DELETE | Phase 2 formal-class tag on formal `Type_Info`s (drives the RM 12.1.2 operation sets and Phase 4 validators) | 2, 3 |
| `Symbol.generic_formal_index` | ada83.c:1915 | DELETE | Formal symbols are ordinary symbols in the generic/instance scopes | 3 |
| `Symbol.generic_formals` / `generic_unit` / `generic_body` | ada83.c:1994-1996 | KEEP | The template ASTs; `generic_body` now resolved by Phase 2 | 2 |
| `Symbol.generic_template` | ada83.c:2000 | KEEP | Instance→template link: circularity detection, "instance at" diagnostics | 3, 6 |
| `is_generic` unit flags + ALI "GE" token | ada83.c:3166, 3229, 42789, 43138 | KEEP | ALI round-trip unchanged | — |

#### C. Syntax and parser

| Item | Location | Verdict | Notes | Phase |
|------|----------|---------|-------|-------|
| `NK_GENERIC_*` node kinds and payloads | ada83.c:883-888, 1346-1384 | KEEP | Payloads gain nothing; shape-table rows added for the three `*_PARAM` kinds, `NK_GENERIC_DECL`, `NK_GENERIC_INST` | 1 |
| `Generic_Def_Kind`, `Generic_Mode_Kind` | ada83.c:896-903 | KEEP | `GEN_MODE_*` finally *consumed* (RM 12.1.1 semantics + 12.3.1 validation) | 3, 4 |
| All §9.15 parsing | ada83.c:6741-6950, 7065-7113, 7178-7194 | KEEP | No change expected | — |

#### D. Comments and gates

| Item | Location | Verdict | Phase |
|------|----------|---------|-------|
| "expanded_body clone is unusable / honest gap" comments | ada83.c:38938-38944, 39001-39005 | DELETE (with the code they describe) | 3 |
| cc1310a gate comment | §13 emission path | DELETE (with the gate) | 3 |
| §16 header comments describing macro-style substitution | ada83.c:3410-3425, 43850-43863 | MOVE — rewritten to describe clone+bind+resolve (evergreen, no history) | 3 |

Anything discovered mid-refactor that is not in this table gets a verdict
added here *before* it is touched — the table stays total over the old
design for the life of the work.

## 9. Machinery wiring matrix

Every subsystem the generic system touches, what must exist there, which
phase delivers it, and how it is verified wired-in. "Production ready" for a
row means: implemented, reachable from the normal compile path (not gated on
an accident), covered by a named gate, and with no surviving call sites of
the machinery it replaces.

| # | Subsystem | Required machinery | Phase | Wired-in verification |
|---|-----------|-------------------|-------|----------------------|
| W1 | §8 syntax | Shape table total over `NK_*`; single source of truth for child edges | 1 | Build-time assertion: every kind with children has a row; golden clone=IR test |
| W2 | §9 parser | Formal part, all `GEN_DEF_*` forms, both instantiation forms (already present) | — | bc parse-level tests; no change expected |
| W3 | §11 names | Generic scope for template analysis; instance scope with formal-binding symbols; declaration-scope resolution environment for clones | 2, 3 | Template errors at declaration (bc); clone-vs-template global-resolution assert |
| W4 | §12 semantics (declaration) | Template fully analyzed at `NK_GENERIC_DECL`; formal type classes with RM 12.1.2 operation sets; formal object/subprogram legality (items 4, 16-31, 44-54) | 2 | bc template-legality families convert; spec-only generic errors surface |
| W5 | §12 semantics (instantiation) | Clone + bind + re-resolve; per-formal-kind validators (86-136); association rules (60-63); default handling (11-12, 78-79, 128-133) | 3, 4 | bc WRONG_ACCEPT count; cc1010a/cc3121a/cc1311a/cc3606a |
| W6 | §13 codegen | Instances emit through ordinary `Generate_Declaration`/nested-package/library paths; per-instance storage; body statements at declaration point and in `__elab`; zero substitution call sites | 3 | grep-free proof: deleted symbols absent (compiler-verified); c35003b substance in IR; two-instance independent-state test |
| W7 | §13.2 checks | 12.3.1-12.3.6 dynamic checks through `CHK_*` (suppressible) | 5 | cc3406/3407/3408/3504 family; pragma Suppress interaction test |
| W8 | §13.5 exceptions | Instance body handlers wrap elaboration statements (already shaped at ada83.c:38948-38967; survives as ordinary nested-package emission) | 3 | Handler-in-instance-body directed test |
| W9 | §14 ALI | `is_generic` flags (present); serializer walk derived from the W1 shape table | 1 | ALI round-trip test on a generic unit |
| W10 | §15 elaboration | Library instance `__elab` registered in the elaboration graph (present, becomes unconditional) | 3 | ce2102-class std-stream init runs; elaboration-order test with instance |
| W11 | §16 | Total cloner; `Expand_Generic_Package` replaced by clone+bind+resolve; no expression substitution in the cloner | 1, 3 | Golden test; name-capture directed test (local homograph of a formal) |
| W12 | §17 loading | Body association by unit structure; disk lookup only via normal library loading; `Instantiating_Set` circularity rejection | 3, 6 | Generic-inside-package body test; mutual-instantiation bc-style test |
| W13 | Diagnostics (§5) | "in instantiation at <site>" note line on clone-sourced errors | 3 | Directed test: error in template body reported with both locations |
| W14 | Tasking (§13/RM 9) | Task specs/bodies inside generics clone and resolve per instance (clone is a package; master/activation machinery applies untouched) | 1, 3 | Generic-with-task directed test; tasking suite unchanged (deterministic since the RM 9 work — a flip is a real regression) |
| W15 | §10 types | `Type_To_Rep` no longer resolves generic formals at lowering time (~7997 call dies in the Phase 3 sweep); a type's representation answer is context-free, as freezing (§10.6) intends | 3 | Compiler-verified with the `Resolve_Generic_Formal` deletion; representation asked only of resolved clones |
| W16 | §15/§14 substrate | Instance `__elab` registration (W10) rides the elaboration graph as wired today — `EDGE_SPEC_BEFORE_BODY` only, `EDGE_WITH`/`EDGE_ELABORATE*` unbuilt, ALI elaboration pragmas unconsumed (survey §14/§15 findings) | 3; edge wiring is adjacent work (§7) | Elaboration-order directed test with instance; cross-unit ordering limits documented until the edges exist |

RM-checklist traceability: items 1-15 → W2/W4; 16-25 → W4/W5; 26-43 →
W4 (operation sets) + W5/W6 (rebinding, 41-42); 44-54 → W4/W5; 55-58 → W4/W12;
59-85 → W5/W6/W10; 86-136 → W5/W7; 137-140 → W3; 141-150 → W5/W7/W12.

---

## 10. Risks

Carried from the tree-copy plan, plus this audit's additions:

- **Resolution side effects on shared rts sources:** templates from `rts/`
  (SEQUENTIAL_IO, DIRECT_IO) get cloned per WITH'ing unit — watch compile
  time (arena growth fine; resolution cost per instance ~ package size).
- **Recursive instantiation:** depth guard with a real diagnostic,
  mirroring `Loading_Set`'s circular-WITH check (now Phase 6 proper, not
  just a guard).
- **ALI/separate compilation:** instances are intra-unit today; keep that
  (the clone lives in the instantiating unit's tree).
- **Diagnostic locations:** keep template file:line; add one
  "in instantiation at <site>" note line — without it, errors in clones are
  unfindable.
- **Declaration-scope resolution vs private views:** the Phase 3 choice to
  resolve clones in the generic-declaration scope sidesteps
  `Save_Global_References` but leaves the private-view-difference corner
  (§5.6). Bounded: Ada 83 ACATS coverage is light; the fallback (port the
  view exchange) is localized to type-view switching in §11.
- **Validator strictness vs existing passes:** Phase 4 will newly reject
  programs the suite currently accepts-and-passes; any cc PASS→COMPILE
  regression is by definition a validator bug or a previously-false pass —
  triage each, per the honesty ledger.
- **Arena lifetime:** clones are arena-allocated and never freed until
  shutdown (§3 memory model) — instantiation-heavy units grow the arena;
  acceptable, but don't add per-instance `free()`.
- **Elaboration-graph under-wiring (survey §15):** instance `__elab`
  ordering depends on `EDGE_SPEC_BEFORE_BODY` plus source-order fallback
  until `EDGE_WITH`/`EDGE_ELABORATE*` edges are built; a generic-heavy
  multi-unit program may surface this before other code does. Tracked as
  adjacent work (§7), not silently absorbed.
- **Driver fork state (survey §19):** `ALI_Cache` and `g_elab_graph` are
  process globals inherited by fork-per-file workers without reset; latent
  today, but template loading through the ALI path crosses it — keep the
  invariant in view when Phase 3 touches template loading.

---

## 11. Production-readiness gates and verification

A phase is done when all of its gates hold; the work is done when all rows
of §9 hold simultaneously on a green suite, with the §8.10 disposition
table fully discharged (every DELETE absent, every MOVE's source absent and
destination present).

**Suite gates (run full Class A + B + C before/after each stage; B
especially — generic legality tests are sensitive to where resolution errors
surface):**

- **bc gate** (Phases 2, 4): WRONG_ACCEPT count is the metric (baseline
  141); each validation slice names the bc tests it converts.
- **cc gate** (Phases 3, 5): constraint-check family
  cc3406/3407/3408/cc3504 + cc3208a + cc3305d (Phase 5);
  attribute/operation families cc1221b/cc1302a (Phase 3); formal-object
  families cc1010a/cc3121a (Phase 3); defaults cc3606a/cc1311a (Phase 4);
  the cc1222a SIGSEGV root-caused (expected F1-class; re-verify after
  Phase 3); internal errors cc1308a, cc3601a fixed (Phase 3 step 1 gate).
- **No-regression invariants:** cc1310a unchanged; tasking results
  deterministic (a tasking flip is a real regression, not noise).

**Honesty ledger** (the §11.4-style discipline): every PASS→FAIL listed with
the real gap it exposes; every FAIL/SKIP→PASS listed with the fix that
earned it. False passes (c35003b class) are expected to convert to real
results in Phase 3 — some to PASS outright, some exposing the next true
gaps; the signal is which failures are now REAL.

Ledger to date (baselines: class C 989/1979, class B 282/1350):

- *Phase 1 (total cloner)*: class C 989 → 989; the one observed flip
  (c43207a) reproduces on identical IR with identical binaries — uninit
  data in the emitted program, pre-existing, outside generics.
- *Phase 2 (template analysis)*: class C 989 → 992, class B 282 → 285.
  - FAIL→PASS: c23006f, ca2009a, cd2a32c; cc1010b (RM 12.1.1 formal-object
    mode semantics); bc1101a, bc1102a, bc1107a (template legality now
    checked at the generic declaration).
  - PASS→FAIL: cc1221a — exposed false pass. Both before and after, the
    instance function writes its body-level variables through frame
    offsets computed for the *parent's* frame layout against its own
    16-byte `__frame_base` alloca (provable in the baseline IR: store at
    offset 110 into `alloca i8, i64 16`). The baseline's PASS was benign
    stack corruption; Phase 2's symbol-creation order shifted the offsets
    by 8 and the corruption became fatal. The defect is the
    substitution-at-emission model's shared-symbol frame layout (F1);
    per-instance resolution (Phase 3) is the structural fix. Not patched
    in the dying machinery.
  - Collateral fix: `Eval_Const_Numeric` no longer "folds" a
    `SYMBOL_CONSTANT` with no static initializer to its `frame_offset`
    (an ASCII-named-number stash) — gated on `is_named_number`. Found via
    c35507p when formal IN objects became constants.
- *Phase 3 stage 1 (tree-copy subprogram instances)*: class C 992 → 1005
  (1006 with the c43207a flake up). Zero PASS→FAIL. Generic subprogram
  instantiations now clone (`Clone_Subtree`), bind formals as instance-scope
  symbols (type alias / per-instance IN constant global / IN OUT pointer
  cell, elaborated at the instantiation point — `___elab`-wrapped at
  library level), resolve in the generic declaration's scope, and emit
  through `Generate_Subprogram_Body`. Formal-part defaults use a
  *decorated* clone (declaration-point visibility, RM 12.1(13)) with
  formal references remapped to bindings (RM 12.3(79)); the generic's own
  name binds to the instance for recursion (RM 12.1(7)); instantiations
  preceding the body instantiate when the body arrives.
  - FAIL→PASS: cc1005c, cc1010a, cc1104c, cc1107b, cc1221b, cc1221d,
    cc1018a, c35712c, c35a05q, c38102e, c41303n, c41303v, c87b50a,
    cb1005a, cd2a23b, c23006g, c58004g, c54a13b, c43204c/h, c43205h …
    (net +13 over Phase 2).
  - Collateral general fixes found through the honest path: (1) RM 3.7.2
    exactly-once — object elaboration re-evaluated discriminant
    constraint expressions when binding dependent-bounds allocas
    (pre-existing, non-generic repro confirmed; now binds from the
    record's just-stored slot; fixed c32107c); (2) symbol-targeted
    assignment stores route through `Emit_Assignment_Target_Address`
    (one choke point honoring IN OUT binding indirection);
    (3) `Resolve_Subprogram_Rename` returns terminal non-subprogram
    targets (entries, enumeration literals) instead of stopping one
    link short.
- *Phase 3 stage 2 (tree-copy package instances)*: class C 1006 → 1037,
  cc group 18 → 34. Sole open delta vs stage 1 is the proven c43207a
  coin-flip. Package instantiations now clone spec+body (disk templates
  parsed via the loader; SEPARATE stubs are NOT bodies — the subunit
  completes the template, ca2009a), bind formals (formal types as
  actual-VIEW subtypes carrying RM 12.1.2(41-42) predefined-operator
  rebinding, c74409b; "/=" actuals as synthesized wrapper BODIES — one
  ordinary call path, zero emission special cases, cc3601c), resolve
  under declaration-point visibility (creation-sequence cutoffs — the
  Save_Global_References equivalent — spec horizon at the generic
  declaration, body horizon at the template body, cc1010a/cc1010b), and
  emit through the ordinary package paths (instance package-level state =
  per-instance globals even when nested; bindings elaborate first in the
  instance's ___elab, c23006f).
  - Structural collaterals: scope flat arrays hold only overload-chain
    heads — the export list flattens chains (sequence-windowed to the
    visible part), fixing USE-visible homograph collapse (ce2111*,
    ce2409*); globals are never uplevel (`Is_Uplevel_Access` says no, and
    the frame-alias emitters skip them — the cc1221a-class frame-offset
    corruption is structurally gone); `is_limited` survives private
    completion (RM 7.4.4) with VIEW-AWARE enforcement
    (`Type_Limited_View_Active`: outside the defining package only) and
    representation-gated build-in-place (scalar-rep limited types return
    plainly; parameterless BIP calls route through Generate_Apply,
    ce3201a/ce3706c); a formal's 'BASE chains to the actual's BASE, not
    its first subtype (c35a06o).
  - Now-executing exposed gaps (next phases' work): c35003b (formal
    subtype-indication range checks, Phase 5), cd3015f (spec-declared
    nested instantiation substance now runs and fails honestly).
- *Phase 3 stage 3 (the sweep)*: class C steady at 1037/1038 (the band is
  the c43207a coin), cc 34, ~1000 lines removed. Compiler-proven extinct
  (declaration removed, clean build, zero survivors):
  `Resolve_Generic_Actual_Type`, `Resolve_Generic_Formal`,
  `Substitute_Generic_Formal_Subprogram`, `cg->current_instance`,
  `Generate_Generic_Instance_Body`, `Find_Homograph_Body`,
  `Emit_Instance_Exported_Globals`, `Reset_Template_Extern_Flags`,
  `Expand_Generic_Package`, `Node_Deep_Clone`/`Node_List_Clone` (the
  substituting cloner — `--clone-check` now exercises `Clone_Subtree`),
  `Generic_Formal_Substitution_Depth`, the `generic_formal_index` trio
  (Syntax_Node/Type_Info/Symbol) and every stamper, the export-cloning
  and profile-substitution blocks, the old NK_GENERIC_INST emission tail,
  the deferred-instance queue branch, both `substitution cycle`
  diagnostics, the `Emit_Symbol_Name` template-prefix and
  `Find_Instance_Local` rules, `Operator_Call_Target`'s substitution arm,
  `Type_To_Rep`'s and `Float_LLVM_Rep_Of`'s formal-resolution fallbacks,
  the assignment-target and composite-address splice blocks, the
  SYMBOL_GENERIC recursion redirects, and the `instantiated_subprogram`
  dead field. §16's headers and the table of contents now describe
  tree-copy instantiation.
  - Disposition amendments (recorded per §8.10's "table stays total"
    rule): `generic_actuals` did not vanish — it became the typed
    `Generic_Actual_Binding *actual_bindings` record on the instance
    (KEEP, re-justified): it is the instantiation's resolved
    formal-to-actual association — GNAT keeps the same data as renaming
    assoc lists — consumed by the formal binder, by late body completion,
    and next by the Phase 4 validators; no emission-time reader exists.
    `expanded_spec`/`expanded_body` renamed to
    `instance_spec`/`instance_body` (the resolved clones).
    `generic_body_cache` KEEP: it caches the §17 disk parse of a library
    template body, one parse per template. `Generic_Actual_Type_By_Name`
    KEEP: a pass/binder-side name lookup over the bindings record.
  - Structural addition forced by the sweep, in the right direction:
    attribute actuals (IS T'SUCC) became synthesized wrapper BODIES
    (`Synthesize_Attribute_Wrapper`, the same mechanism as "/=") — the
    old emission-time attribute expansion died with its block. Bodyless
    intrinsic instances (UNCHECKED_CONVERSION) build their profile from
    a spec clone.
- *Phase 4 (actual validation)*: `Validate_Generic_Actuals` enforces
  RM 12.3.1–12.3.6 over the bindings record at the head of instantiation
  resolution: scalar class membership, private limitedness + discriminant
  count/unconstrained-ness, array dimensionality / constrainedness / index
  and component types (through `Formal_Mark_Actual_Type`), access
  designated types, IN OUT variable-ness (`Actual_Is_Renameable_Variable`,
  RM 8.5), formal-subprogram kind / arity / per-position mode and type /
  result type, and missing-actual-without-default. Gate per direction:
  class C only — 1038, zero diff (no legal program rejected); negative
  behavior spot-proven by hand (`float for range <>`, literal for IN OUT —
  both diagnosed at the actual's location). The bc suite is explicitly
  out of scope until the executable substrate matures.
- *Phase 5 (instantiation-elaboration constraint checks)*: class C
  1038 → 1054, cc group 34 → 50. RM 12.3.2/12.3.4 subtype agreement
  (formal subtype indications with explicit constraints vs the actual's
  bounds) is captured as `Instance_Agreement_Check` pairs at RESOLUTION
  time in the binder — emission has no scopes to resolve marks in — and
  emitted by `Emit_Subtype_Agreement_Check` (static fast-path folds both
  sides when possible; otherwise runtime bound loads via `Emit_Type_Bound`
  + `Emit_Check_With_Raise`). RM 3.3.2 elaboration checks for subtype
  declarations inside instances come from `Emit_Subtype_Constraint_Compat_Check`
  at NK_SUBTYPE_DECL codegen — guarded on `cg->current_function` (module
  level has no instruction stream) and exempting null ranges per
  RM 3.5(4) (a null range is compatible with anything; c35502b/p).
  - Root causes unearthed on the way: the parser DISCARDED formal-array
    index constraints (built no def_detail) — `TYPE T IS ARRAY (...) OF ...`
    formals now build a real `NK_ARRAY_TYPE` (indices as subtype
    indications, box or range; component subtype), so the validator and
    agreement capture see the true shape; `Formal_Mark_Actual_Type` now
    accepts SYMBOL_SUBTYPE marks, not just SYMBOL_TYPE.
  - Residue pass (closed after Phase 7): class C 1055 → 1068, the whole
    cc3407/cc3504 families green plus c35003b and c36204c.
    `Emit_Subtype_Agreement_Check` became a shape dispatcher —
    `Emit_Value_Agreement_Check`/`Emit_Bound_Agreement_Check` over a
    comparison domain (`Emit_Agreement_Bound`): scalar pairs compare
    bounds (fcmp for float, scaled mantissas for fixed — real-valued
    bounds divide by the type's small; folded `FIX(3)`-style
    BOUND_INTEGERs scale too); array pairs compare per-dimension
    Index_Info constraints (an array subtype's constraint lives in
    Index_Info, NOT in the shared index_type) and recurse on components;
    access pairs recurse on designated subtypes (cc3504c's
    access-to-access); record/private pairs compare discriminant
    constraint values (`Discriminant_Constraint_Bound`). The capture
    filter is now unconditional — the emitter ignores shapes it cannot
    compare. `Emit_Type_Bound` gained float-rep output
    (`Emit_Float_Constant`).
  - c35003b closed by generalizing the RM 3.5(4)/3.6.1 elaboration check:
    `Emit_Range_Constraint_Compat_Check` (bound-pair core; static-static
    decides at compile time and emits `Emit_Raise_And_Continue` on
    violation) called at NK_TYPE_DECL for array index constraints (bounds
    from Index_Info), an access type's designated subtype, and a derived
    scalar's parent indication; subtype indications using `T RANGE
    A'RANGE` now populate bounds via the shared
    `Index_Bound_From_Range_Attr`. Guard against the implementation's
    integer-type model: a type declaration's FIRST subtype has no
    constraint-vs-mark relation (RM 3.5.4 width choice is compile-time
    legality, not an elaboration check) — without this, 64-bit integer
    type definitions and SYSTEM.ADDRESS (i64 bounds over a 32-bit
    INTEGER base in our model) false-raised (cc1221b, cc1222a, cd7002a).
- *Phase 6 (circular instantiation detection)*: `Instantiating_Set` — the
  §17 `Loading_Set` mirror — pushes each (generic, instance) entry during
  instantiation and rejects re-entry with a "recursive instantiation"
  diagnostic at compile time (RM 12.1: a generic unit cannot instantiate
  itself, directly or through intermediaries). Zero class C movement
  (purely a rejection path); hand-proven on a direct self-instantiation
  and a two-generic mutual cycle.
- *Phase 7 (class A pass)*: class A 130 → 136 of 140 — above the 134
  pre-work baseline; the 4 remaining skips (a85013b, ac3207a, ae2101a,
  ae2101f) pre-exist on main and are not generics work. Fixes, each a
  real defect:
  - a87b59a: pass-3 subprogram-actual resolution took the FIRST homograph
    — `Disambiguate_Subprogram_Actual` now walks the overload chain for
    the formal-profile match (RM 12.3.6); builtin-operator actuals ("&",
    "+", …) get synthesized wrapper bodies (`Synthesize_Operator_Wrapper`,
    same mechanism as "/=" and attributes); wrapper bodies live at the
    INSTANTIATION's static level (`binding->parent = instance_sym->parent`,
    re-pointed after `Symbol_Add` stamps the instance) so callers inside
    the instance pass a `%__parent_frame` that exists, and
    `is_instance_wrapper` joins the mangle-suffix rule (the `@ne`
    redefinition collision).
  - ac3106a: two pre-existing indexed-lvalue defects exposed by IN OUT
    binding elaboration (`Generate_Lvalue` on the actual): a non-symbol
    prefix (selected component) of a dynamic-bounds array returned the
    raw fat `{ptr,ptr}` value as the GEP base — now unpacked (data +
    bounds, dynamic index math); an implicit access-to-array dereference
    extracted the data pointer but DROPPED the bounds, leaving the index
    check on the unconstrained placeholder range — now both bounds are
    extracted and `has_dynamic_low` set. Both fixes are general indexing
    correctness, not generics special cases.
  - Closing gate: class A 136/140, class C 1055/1979 with an EMPTY
    regression diff against the prior pass-set — the sole delta is the
    ledgered c43207a coin landing on PASS.

**Final state after the Phase 5 residue pass: class A 136/140, class C
1068/1979 — every delta against the 1055 baseline is a gain (cc3407b/c/d/e,
cc3504b/c/d/f/g/h/j/k, c35003b, c36204c, plus the three false-raise
recoveries cc1221b/cc1222a/cd7002a); zero losses.**

- *Phase 8 (class A to 100%)*: class A 136 → **140/140**, class C
  1068 → 1071 (gains c74401k, c93005b, cc3207b; zero losses). The four
  remaining skips, all pre-existing on main, each exposed a general
  defect:
  - ae2101a/ae2101f: the deferred-bodies queue was a fixed 64-slot array
    whose overflow fell back to INLINE emission — a `define` nested inside
    the still-open function (SEQUENTIAL_IO/DIRECT_IO instances queue many
    bodies). The queue is now arena-grown without bound through one
    `Defer_Subprogram_Body` helper (dup-check included); all four
    queuing sites use it.
  - ac3207a: selected-component lookup through a package's exported list
    returned the single task's TYPE symbol — but RM 9.1 says a single
    task declaration declares an OBJECT (its type is anonymous), and the
    type symbol has no storage. `Single_Task_Object_Denoted` redirects
    name lookup landing on a single-task type symbol to the implicit
    object variable; applied at both package-member lookup paths.
    NOT generics-specific: a plain nested package with `P.T1.E` failed
    identically.
  - a85013b: an entry rename inside a task body bound its hidden
    task/index slots via `getelementptr %__frame_base` — but a task
    function never allocates a frame base (its locals are allocas;
    enclosing-scope variables arrive through `%__frame.` aliases), and
    deferred emission leaked the ENCLOSING function's has-nested flag
    into the task's declarations. `Generate_Task_Body` now saves and
    zeroes `current_nesting_level`, and the rename-binding emitter
    follows the object-declaration pattern (frame slot when the function
    owns a frame, alloca otherwise) and stores through
    `Emit_Symbol_Storage` (alias-aware) on both slots.

**Directed tests (beyond ACATS):**

- c35003b's E1..R2 substance executes — strings present in the IR and the
  run exercises them.
- cd3015f's CHECK_HUE is defined, not just called.
- ce2102-class std-stream FILES initialization runs at elaboration.
- Two instances of one stateful generic hold independent state.
- Mode-in-out formal object aliasing observable through the instance.
- A side-effecting actual for a mode-in formal object evaluates exactly
  once, at instantiation.
- A template-local homograph of a formal object is NOT substituted (name
  capture).
- A generic declared inside a package, body in the enclosing package body,
  instantiates correctly (W12).
- An error inside a template body reports template location + "in
  instantiation at" the instance site (W13).
- A generic containing a task type: per-instance task machinery (W14).
- Clone golden test: non-generic unit, clone emits byte-identical IR (W1).
- ALI round-trip of a generic unit (W9).

**Mechanical completeness proofs:**

- The end-state deletion list (§7) and the disposition table (§8.10) are
  compiler-verified: each deleted symbol's removal compiles clean with zero
  survivors — the compiler is the search tool, not grep (§8.6).
- The W1 build-time shape-table assertion stands permanently against new
  node kinds.
- The final adversarial review (§8.8) signs off the wiring matrix and the
  disposition table together.

---

## 12. Appendix — Subsystem Survey: Purpose Fidelity, Completeness, and Coupling

Merged in full from `docs/systems.md`. An architectural audit of the
nineteen subsystems in `ada83.c` (~46k lines). Three questions per
subsystem: is it used for its intended purpose, how complete is it, and how
is it coupled to the rest of the compiler. Line numbers reference the state
of the file at the time of this survey; treat them as anchors, not
contracts. (Throughout this appendix, §N refers to ada83.c subsystems, not
sections of this document.)

### A.1 Verdict at a glance

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

### A.2 Ranking by completeness

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

### A.3 Per-subsystem findings

#### §1–§5: Foundation layers

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

#### §6: Arithmetic

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

#### §7–§9: Lexer, syntax tree, parser

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

#### §10: Types

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

#### §11: Names

The best subsystem in the file. Overload resolution is one pipeline —
`Collect_Interpretations` → `Filter_By_Arguments` → `Disambiguate` →
`Resolve_Overloaded_Call` (8796–9026) — and every resolution site in §12
funnels through it; no ad-hoc resolution paths exist. Use-clause visibility
is alias-based and RM-conformant; character literals resolve as enumeration
members through both derivation and subtype chains. `MAX_INTERPRETATIONS`
(64) is a pragmatic cap, not a correctness issue.

#### §12: Semantics

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

#### §13: Code generation

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

#### §14: Library management

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

#### §15: Elaboration

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

#### §16: Generics

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

(The main document is the deep-dive this paragraph summarizes: §2 maps the
machinery, F1–F15 are the findings, and the §7 plan replaces the
substitution model outright. Note the survey's "then the clone is
re-resolved through the ordinary §12 entry points" describes the *intended*
design; the main document's audit found the clone path half-dead — clones
are never re-resolved (see §2.7 and F1) — which only sharpens the survey's
"structurally fragile" verdict.)

#### §17: File loading

Include-path search (explicit `-I`, then exe-relative `rts`, input dir, `.`)
and circular-`with` detection via `Loading_Set` both work as designed.
Subunits are the gap: `separate` bodies parse (`is_separate` is set) but
there is no mechanism to compile a subunit against its parent context — the
stub and the separate body are never connected. Parse-only support for a
language feature is the same accept-and-ignore pattern as the rep clauses.

#### §18: SIMD

Genuinely used, not decorative: `Simd_Skip_Whitespace`, `Simd_Find_Newline`,
and `Simd_Scan_Identifier` sit directly in the lexer hot loops (4396, 4410,
4429), and `Simd_Parse_Digits_Avx2` **is** live — it is the batch path
inside `Big_Integer_From_Decimal_SIMD` (45712), parsing 8/16-digit chunks.
Runtime AVX-512/AVX2 detection gates the x86 paths. The asymmetry: ARM64
gets NEON for the scanning functions but the decimal batch path is
x86-only, and the scalar fallback's 4-byte unrolling is unbenchmarked
folklore. Low risk either way.

#### §19: Driver

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

### A.4 Dependency and coupling analysis

#### The dependency picture

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

#### What is healthy

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

#### Coupling problems, in priority order

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
   codegen.* (Superseded by the main document's plan: Phase 3 deletes the
   substitution model entirely, which contains the cycle at resolution time
   and removes both §13 consumers — see F15.)

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

### A.5 Dead code inventory

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

### A.6 Recommended order of work

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

(Items 3 and 4 interact with the generics plan as "adjacent work" — see §7;
item 5's no-silent-drops rule is the same principle the generics plan
applies to instantiation legality.)
