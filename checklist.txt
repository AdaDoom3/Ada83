In the pursuit of high-quality code (low line count (e.g. low undue complexity) and high feature/correctness) - rewrite ada83.c into ada83new.c with much better Haskell-like C99 and tight smart code-golfing with, ironically, textbook "Literate Programming" style with Ada-like full names and sparse, but thoughtful comments.

Use checklist.txt and update it with progress

We want to remove all of the following according to the analysis below:
- Unnecessary functions
- Duplicated code patterns where predicates could be used to reduce bloat
- Sketchy hacks designed to ignore incorrectness
- Anything which goes against what is done in GNAT LLVM
- Competing implementations of different aspects of the compiler conceptually

Don't be afraid to break things in the pursuit of perfection.

## Part 1 — lines 1–1600 (read & analyzed)

This chunk covers: **type-metric helpers**, a **homebrew bigint** (used during literal scanning), a **single global arena**, **string helpers**, **error reporting**, **token/keyword tables**, and most of the **lexer**, then starts the AST definitions and generic vector push macro.

---

# What’s solid vs. what’s already risky

### Solid

* **Safe ctype wrappers** (lines 33–40): correct avoidance of UB from signed `char`.
* The **type-metrics helpers** are conceptually aligned with GNAT LLVM’s “centralize size/alignment math” idea (lines 42–207), even if the implementation is too platform-assumptive.
* Lexer structure is readable: `peek/advance/skip_whitespace` + per-token scanners + `lexer_next_token`.

### High-risk / sketchy (in this chunk)

1. **Hard-coded target model**: `WPTR = 64` and “LP64 assumed” (lines 73–76).
   *Against GNAT LLVM practice:* GNAT derives pointer width and layout from target data/layout, not a compile-time constant.
2. **Arena allocator resets without tracking old chunks** (lines 474–486). That’s “leak-by-design” (fine for a one-shot compiler), but the *resetting* of `main_arena.base` means you can never free earlier chunks or even iterate them for cleanup/debug.
3. **`string_to_lowercase` is a ring of 8 static buffers** (lines 502–512): not thread-safe, not re-entrant, easy to misuse (values silently overwritten).
4. **Bigint uses `calloc(c, 8)` / `realloc(..., c * 8)`** (lines 223–245): non-portable and brittle. Should be `sizeof(uint64_t)` and checked.
5. **Number literal scanning is correctness-fragile** (lines 822–1035), with multiple “this will bite later” behaviors:

   * uses **fixed-size 512 scratch buffers** for mantissa/exponent/text (lines 899–910, 958–966).
   * uses **double (`pow`, `strtod`) as an intermediate** for based integer constants (lines 910–954): precision/rounding risk near i64 limits.
   * constructs a bigint from `text_pointer` even when the literal contains exponent notation (lines 1010–1017). `unsigned_bigint_from_decimal` ignores non-digits, so something like `1e2` becomes bigint **12**, not 100.
6. **`scan_identifier` has a dead/pointless check** (lines 818–820). As written, it can’t trigger because the loop stops exactly when the next char is not `[A-Za-z0-9_]`.

---

# Unnecessary functions / machinery (so far)

* `unsigned_bigint_multiply` (lines 419–423) is a **one-line wrapper** calling `unsigned_bigint_multiply_karatsuba`. If you want a policy hook, make it real (threshold selection, algorithm choice); otherwise it’s bloat.
* `unsigned_bigint_add_abs` and `unsigned_bigint_sub_abs` (lines 308–317) are thin wrappers around `unsigned_bigint_binary_op`. Not terrible, but arguably redundant unless you expect call-site clarity to matter.
* `to_bits_*` and `to_bytes_*` triples (lines 120–127) exist to support `_Generic`, but most compilers end up passing `size_t`/`int` anyway → defaulting to the u64 version. If you keep `_Generic`, consider covering `size_t` and `unsigned long` explicitly to reduce silent casts.

---

# Duplicated code patterns worth collapsing (early wins)

### 1) “Grow arena string buffer” pattern appears twice in `scan_string_literal`

Lines 1071–1077 and 1085–1091 duplicate the exact “if full → allocate bigger → memcpy” block. This should be a tiny helper like:

* `arena_grow_copy(char **buf, int *cap, int needed_len)`
  so you don’t maintain the same logic twice.

### 2) Manual whitespace char list in `skip_whitespace`

Lines 793–796 hardcode `' ' '\t' '\n' ...` even though you already defined `ISSPACE`. The only reason not to use `ISSPACE` is if you need *Ada-specific* whitespace rules. If not, simplify.

### 3) Digit parsing repeated in several places

In `scan_number_literal`, the “hex digit value” ternary appears multiple times (lines 925–927, 971–973, 995–997). Factor into a small function:

* `static inline int digit_value(int ch)` with base validation.

---

# “Sketchy hacks” / correctness hazards (called out by line)

### Bigint layer

* **Allocation sizing via literal `8`**: lines 224, 242–243, 352–353, 393–394.
* **No OOM handling** anywhere in bigint allocation/grow.
* **Karatsuba views**: `a0/a1/b0/b1` are made as stack structs pointing into another bigint’s `digits` (lines 378–381). That can work, but your `capacity` fields are inconsistent (`a1` has capacity 0) and this style invites accidental `grow()` calls on a view later.

### Literal scanning (most serious in this chunk)

* **Exponent + bigint bug**: building `unsigned_integer = unsigned_bigint_from_decimal(text_pointer)` (line 1016) even when `text_pointer` contains `e`/`E` results in nonsense bigints.
* **Based integer constants via floating math**: lines 910–954. GNAT wouldn’t do this; it uses integer arithmetic all the way.
* **Fixed scratch buffers**: base_pointer(32), mantissa/exponent/text(512) — lines 837, 899–901, 958.

### “Extensions” without strong rationale

* Treating `'!'` like `'|'` (lines 1173–1176). Ada doesn’t use `!`. If this is a debug/extension token, it should be explicit and quarantined, not silently mapped.

---

# Against GNAT LLVM style (in this chunk)

GNAT LLVM’s “type metrics” are a *central truth* derived from target layout. Here you’re mixing a good idea with wrong mechanics:

* `WPTR=64` (line 75) and “LP64 assumed” is exactly what GNAT avoids: pointer width, alignment, and ABI layout must come from target configuration / datalayout.
* The metrics section is trying to look like GNAT (`To_Bits`, `To_Bytes` etc.), but the compiler later will inevitably need:

  * pointer size for fat pointers, access types,
  * record layout + bitfields,
  * alignment rules per type,
  * endian/layout constraints.

Hardcoding `WPTR` means the entire compiler is silently “x86_64-only” (or at least “64-bit only”), and worse: the source *pretends* it’s an architectural abstraction.

---

# Function-by-function quality ratings (1–10)

### Type metric helpers

* `to_bits_u64/u32/i64` (120–122): **8/10** — clean, but unnecessary multiplicity unless `_Generic` is truly used everywhere.
* `to_bytes_u64/u32/i64` (125–127): **8/10** — correct ceiling division.
* `byte_align_u64/u32` (130–132): **8/10** — fine.
* `align_to` (134–136): **7/10** — assumes power-of-2 `align` but doesn’t assert; ok if disciplined.
* `llvm_int_type` (166–173): **7/10** — reasonable mapping; but silently returns i128 for anything >64.
* `llvm_float_type` (176–178): **7/10** — simplistic but ok.
* `fits_in_signed` (183–186): **5/10** — UB risk if `bits==64`? (`1LL << 63` is problematic in signed). Needs careful handling.
* `fits_in_unsigned` (188–190): **6/10** — `1ULL<<bits` is UB if `bits==64`. Needs guard.
* `bits_for_range` (193–205): **7/10** — coarse but fine; relies on the two helpers above.

### Bigint

* `unsigned_bigint_new` (221–229): **4/10** — no null checks, hardcoded `8`, count starts 0 but capacity maybe small; ok-ish.
* `unsigned_bigint_free` (230–237): **7/10** — fine.
* `unsigned_bigint_grow` (238–246): **4/10** — no realloc failure checks, hardcoded `8`, `memset` assumes realloc succeeded.
* `UNSIGNED_BIGINT_NORMALIZE` macro (247–254): **6/10** — works; macro is ok but could be inline fn for type safety.
* `unsigned_bigint_compare_abs` (255–263): **7/10** — fine.
* `add_with_carry` (264–269): **8/10** — good use of `__uint128_t`.
* `subtract_with_borrow` (270–275): **6/10** — works but is “clever”; comment would help; portability depends on `__uint128_t`.
* `unsigned_bigint_binary_op` (276–307): **6/10** — ok core, but subtraction assumes `a >= b` abs; relies on callers.
* `unsigned_bigint_add_abs/sub_abs` (308–317): **6/10** — wrappers.
* `unsigned_bigint_add` (318–340): **7/10** — reasonable sign logic.
* `unsigned_bigint_subtract` (341–347): **6/10** — uses stack copy trick; ok.
* `unsigned_bigint_multiply_basic` (348–367): **7/10** — standard schoolbook.
* `unsigned_bigint_multiply_karatsuba` (368–418): **5/10** — conceptually ok, but the view structs / capacities are sketchy, and carry-add loops use a fragile carry test (`c = v < r->digits[...]`) after assignment.
* `unsigned_bigint_multiply` (419–423): **3/10** — pure wrapper; either make it a strategy switch or drop it.
* `unsigned_bigint_from_decimal` (424–454): **3/10** — very allocation-heavy and (later) misused by literal scanning with exponent forms.

### Arena / strings / errors

* `arena_allocate` (474–486): **5/10** — practical but leaks chunks; no OOM checks; global mutable state.
* `string_duplicate` (487–492): **8/10** — good.
* `string_equal_ignore_case` (493–501): **7/10** — fine; could early-exit on pointer equality.
* `string_to_lowercase` (502–512): **2/10** — static ring buffer hack; unsafe API.
* `string_hash` (513–519): **7/10** — ok (FNV-ish), case-folded.
* `edit_distance` (521–537): **6/10** — fine for “did you mean”, but uses a big stack array per call.
* `report_error` (540–549): **7/10** — good non-fatal path.
* `fatal_error` (552–562): **6/10** — ok; increments `error_count` oddly for fatal, but harmless.

### Lexer core

* `keyword_lookup` (760–766): **6/10** — linear scan is fine at this size; could be perfect-hash later.
* `lexer_new` (767–770): **8/10**
* `peek` (771–774): **8/10**
* `advance_character` (775–788): **8/10**
* `skip_whitespace` (789–805): **7/10** — correct Ada comment handling; whitespace check is verbose.
* `make_token` (806–809): **8/10**
* `scan_identifier` (810–821): **6/10** — fine, but contains that dead “kw+x” check.
* `scan_number_literal` (822–1035): **3/10** — too many ad-hoc branches, precision hazards, buffer-size hazards, bigint misuse with exponent.
* `scan_character_literal` (1036–1053): **7/10** — ok.
* `scan_string_literal` (1054–1103): **6/10** — works, but duplicated growth logic and unbounded arena consumption.
* `lexer_next_token` (1104–1267): **7/10** — readable; a couple of questionable extensions.

### Vector push macro-generated fns

* `nv/sv/lv/gv/lev/fv/slv` (1427–1444): **6/10** — typical stretchy-buffer push; missing allocation failure handling, but compact.

---

# Competing implementations (conceptual alternatives) for this chunk

### A) Literal handling: “GNAT-like correctness” vs. “fast & loose”

* **Current**: `scan_number_literal` tries to do everything: base literals, reals, exponent, bigint fallback, and float parsing.
* **GNAT-like**: parse into a **token structure that preserves exact lexeme** and only later:

  * convert to multiprecision integer/rational using integer arithmetic,
  * apply base/exponent semantics with exactness,
  * only then decide if it fits in i64 / needs bigint / is real.

This would also solve the `1e2 -> bigint 12` class of bugs by construction.

### B) Bigint strategy: heap-per-step vs. in-place accumulator

* **Current**: `unsigned_bigint_from_decimal` allocates/frees repeatedly.
* **Alternative**: keep an in-place bigint accumulator:

  * `mul_small_inplace(r, 10)`
  * `add_small_inplace(r, digit)`
  * grow occasionally.
    That’s smaller, faster, and simpler than Karatsuba for “literal parsing” use-cases.

### C) Arena: single global bump allocator vs. chunked arena with list

* **Current**: replaces `main_arena.base` with a new chunk when full.
* **Alternative**: maintain a linked list of chunks so you can:

  * free everything at end,
  * optionally reuse between compilation units,
  * debug memory usage.
    Still a bump allocator, but not “forgetful”.

## Part 2 — lines 1601–3200 (read & analyzed)

This section finishes off the giant `Syntax_Node` union, introduces a few more top-level structs, then moves into the **parser core**: token advancement, error recovery, name/primary/expression parsing, formal parameters, generic formals, and `if` statements.

---

# 1) Immediate red flags (naming, correctness, and “papering over” errors)

### A) Function names don’t match what they allocate

* `reference_counter_new()` allocates a `Representation_Clause` (lines 1860–1866).
* `label_use_new()` allocates a `Library_Unit` (lines 1867–1874).
* `generic_type_new()` allocates a `Generic_Template` (lines 1875–1880).

That’s not style nitpicking: it **actively harms readability** and makes grep/maintenance lie to you. It strongly suggests copy/paste rename drift.

### B) `parser_identifier()` returns garbage on error

```c
String_Slice identifier = string_duplicate(parser->current_token.literal);
parser_expect(parser, T_ID);
return identifier;
```

(lines 2105–2110)

If the current token is **not** `T_ID`, you still duplicate its literal and hand it to the AST as an “identifier”. And `parser_expect(T_ID)` (lines 2067–2100) *may just “pretend it was there” or skip one token*.

This is a real correctness bug: AST can contain identifiers that were never identifiers, and later passes must defensively treat names as possibly-invalid strings.

### C) “Pretend tokens exist” error recovery is a sketchy hack

In `parser_expect()`:

* for `; ) ] THEN LOOP IS` it prints an error then **does not consume anything** and just returns (lines 2077–2084).

That *can* be useful, but it’s also how you get:

* infinite loops (if the caller keeps expecting and you never advance),
* silently malformed AST (you proceed as if syntax was correct),
* cascaded nonsense requiring ad-hoc downstream checks.

You added `parser_check_progress()` (lines 1919–1958) to mitigate getting stuck; but that only helps if it’s actually called in the right parsing loops (we haven’t reached those loops yet in this chunk).

---

# 2) Duplicated parsing patterns (big bloat multipliers)

You have the same “choices / associations” parsing logic repeated **several times**:

* `parse_primary()` for parenthesized aggregates (lines 2267–2356)
* `parse_primary()` for operator-string calls (lines 2403–2461)
* `parse_name()` for qualified expressions `X'( ... )` (lines 2529–2577)
* `parse_name()` for calls / indexing `X( ... )` (lines 2594–2656)
* `parse_simple_expression()` repeats parts of name parsing + constraint-ish parsing (lines 2807–2915)

This is the exact kind of duplication where a *single helper with flags* pays off:

* `parse_association_list(allow_named, allow_choices_bar, allow_slice_iterator, context_string_for_errors)`
* `parse_postfix_chain(base_node, flags)` for `.`, `'attr`, calls/indexing.

Right now, any grammar tweak will require you to update 4–5 places and you’ll miss one.

---

# 3) Against GNAT-ish “do it right” practices (even if you’re not trying to be GNAT)

### A) Generic formals are parsed but mostly **discarded**

In `parse_generic_formal_part()` (lines 2990–3124):

* For generic types: you detect things like `digits`, `delta`, `range`, `limited`, `array`, `record`, `access`, `private`… and then you **do not store any of it**. You create `N_GTP` and only keep `name`.
* For generic objects: you parse mode `in/out` and optional default expression, and then you **don’t store mode or default** in the created `N_GVL` node (you only store identifiers + type).

So the parser accepts syntax but does not preserve meaning. GNAT-style frontends are obsessive about preserving enough structure for later legality checks and instantiation.

### B) END-name checking exists but isn’t used here

You wrote `parser_check_end_identifier()` (lines 2111–2127), which is exactly the kind of thing you want for packages/subprograms. But in `parse_generic_formal()`’s package branch (lines 3147–3168) you do:

```c
parser_expect(T_END);
if (parser_at(T_ID)) parser_next(parser);
parser_expect(T_SC);
```

No match check. That’s another “accept wrong program silently” behavior.

---

# 4) Function-by-function breakdown + quality ratings (1–10)

### Node / top-level allocators

* `node_new` (1853–1859): **7/10** — fine bump-alloc node creation, but no zero-init; relies on union fields being explicitly set.
* `reference_counter_new` (1860–1866): **4/10** — tiny and misnamed; should be `representation_clause_new`.
* `label_use_new` (1867–1874): **4/10** — tiny and misnamed; should be `library_unit_new`.
* `generic_type_new` (1875–1880): **5/10** — tiny and misnamed; does too little to justify existing.

### Token movement / lookahead

* `parser_next` (1890–1904): **7/10** — clean merging of `AND THEN`→`T_ATHN` and `OR ELSE`→`T_OREL`. Works, but it’s a bit “lexer policy in parser”.
* `parser_at` (1905–1908): **8/10** — simple.
* `parser_match` (1909–1917): **8/10** — simple.

### Error handling / recovery

* `parser_check_progress` (1919–1958): **7/10** — practical stuck detection using (token, line, col). But it increments error_count and consumes tokens; must be used consistently.
* `parser_synchronize` (1960–1982): **6/10** — standard panic-mode scanning to sync tokens. Token set is plausible.
* `parser_expect_error` (1984–2065): **6/10** — useful hints; but the giant if/else chain should be table-driven.
* `parser_expect` (2067–2100): **3/10** — the “pretend token existed” policy is too broad and dangerous; can easily lead to non-advancing loops and malformed trees.

### Small helpers

* `parser_location` (2101–2104): **8/10**
* `parser_identifier` (2105–2110): **2/10** — duplicates before validating; returns bogus “identifiers”.
* `parser_check_end_identifier` (2113–2127): **7/10** — good idea; needs to be used consistently.
* `parser_attribute` (2128–2255): **5/10** — necessary to accept reserved-word attributes, but implemented as a long ladder; should be a lookup table mapping token→string.

### Expression / name parsing

* `parse_primary` (2266–2489): **5/10**

  * Good: handles literals, `new`, `null`, `others`, unary `not/abs`, deref `all`.
  * Bad: aggregate parsing is overloaded with parenthesized expression parsing; heavy duplication; ad-hoc “illegal postfix on aggregates” skipping.
* `parse_name` (2490–2661): **6/10**

  * Good: postfix chain supports `.`, `.all`, `'attr`, qualified expressions, calls/indexing; supports operator symbols in selectors.
  * Bad: repeats association parsing logic; no clear distinction between call vs indexing vs slice semantics.
* `parse_power_expression` (2662–2675): **8/10** — correct right associativity.
* `parse_term` (2676–2691): **8/10**
* `parse_signed_term` (2692–2720): **7/10** — handles unary +/- then additive ops. OK, but unary binding is slightly awkward (unary wraps the term, then you do additive loop).
* `parse_relational` (2721–2750): **7/10** — ok; stores `NOT IN` as op `T_NOT` which later code must interpret.
* `parse_and_expression` (2751–2766): **8/10**
* `parse_or_expression` (2767–2782): **8/10**
* `parse_expression` (2783–2786): **9/10** — thin wrapper.
* `parse_range` (2787–2806): **6/10** — supports `<>` as empty range node, otherwise parses `a .. b`. Fine.
* `parse_simple_expression` (2807–2915): **4/10** — looks like a second, partially-overlapping name parser + constraint parser. This is a duplication magnet.

### Parameters / specs / generics

* `parse_parameter_mode` (2916–2956): **7/10** — reasonable; but shares the same “parse list + associations” patterns seen elsewhere.
* `parse_procedure_specification` (2957–2971): **7/10** — supports operator-symbol names.
* `parse_function_specification` (2972–2988): **7/10** — ditto.
* `parse_generic_formal_part` (2990–3124): **3/10** — parses a lot, stores almost none; has dead `isp`.
* `parse_generic_formal` (3125–3170): **5/10** — structure is there; missing END-name checking; package branch copies/merges declarations in a slightly sloppy way.
* `parse_if` (3171–3196): **6/10** — works, but uses the *same* `location` for elsif nodes (doesn’t refresh at `ELSIF`), so diagnostics/locations will be wrong.

---

# 5) Concrete “fix it” items that reduce bloat and raise correctness

### A) Fix `parser_identifier` first

Change it to:

* only duplicate if token is `T_ID` (or `T_STR` if you allow operator symbol names in some contexts),
* otherwise return empty slice and consume/sync appropriately.

This one change prevents “random token text becomes identifier”.

### B) Replace the repeated association parsing with one helper

You have 4 nearly-identical chunks building vectors of:

* positional args
* `choices | choices => value`
* special-case `id range ... => value`

Make a single helper returning a `Node_Vector` of either values or `N_ASC` nodes.

### C) Replace the long `parser_attribute` ladder with a table

Example idea (conceptual):

* a static array mapping `Token_Kind` → `String_Slice` for reserved-word attributes and operator tokens
* fallback: identifier case

That removes ~100 lines of if/else and makes it harder to introduce inconsistencies.

### D) Stop “pretending” for `THEN/LOOP/IS` in `parser_expect`

Missing delimiter recovery for `; ) ]` is somewhat defensible.
Pretending `THEN`, `LOOP`, `IS` existed is much more likely to create “valid-looking” but wrong trees. Better recovery:

* report,
* try to synchronize,
* or consume one token to ensure progress.

---

# 6) Competing implementations (conceptual)

### 1) Expression parsing: current precedence ladder vs Pratt parser

Current: `power → term → signed_term → relational → and → or`.
That’s fine, but you’re already mixing unary handling and special-case `not in`.
A Pratt parser would:

* unify unary/binary/postfix (`.`, `'attr`, call/index) in one place,
* reduce function count and duplication.

### 2) Postfix chaining: duplicated in `parse_primary`, `parse_name`, `parse_simple_expression`

Alternative: parse “atom” once, then parse a generalized postfix loop:

* `.selector`
* `'(qualified | attribute)`
* `(call/index)`
  with flags to enable/disable forms depending on context.

### 3) Generic parsing: “accept and discard” vs “preserve structure”

Right now you accept generic constraints but throw away meaning.
Alternative: store exact parsed constraint nodes in the `N_GTP/N_GVL/N_GSP` nodes (even if later phases ignore them initially). That matches GNAT-like frontend discipline and prevents a rewrite later.

## Part 3 — lines 3201–4800 (read & analyzed)

This chunk is basically **“statements + declarations”**: `case`, loops/blocks, `select` (plus accept/delay variants), statement parsing, exception handlers, big `type_definition` parser (records/arrays/access/etc), representation clauses/pragmas, and a large slice of `parse_declaration` + start of `parse_declarative_part`.

---

# 1) Biggest “this will hurt you later” issues

### A) Massive duplication of the same *accept / entry / parentheses disambiguation* hack

You repeat the same pattern in **at least**:

* `parse_statement_list` (lines ~3318–3358 and ~3400–3436),
* `parse_statement_or_label` (lines ~3557–3593),
* task entry declarations in `parse_declaration` (lines ~4613–4673).

The pattern is:

1. see `(` after an entry name
2. if peek is `ID` (or ID/INT/CHAR), **snapshot parser state**
3. advance twice
4. decide “parameters vs index constraints” based on whether you hit `,` or `:`
5. restore and parse accordingly

This is *very* brittle and bloaty:

* it assumes a particular token layout,
* it fails weirdly with defaults, subtype marks, attributes, etc.,
* it’s copy/paste in 3 places.

**Predicate/helper that would kill 150+ lines**:

* `static bool looks_like_param_profile(Parser *p)` that does the snapshot/advance logic once.
* Then a single `parse_entry_profile_or_index_constraints(...)` helper used everywhere.

GNAT-ish frontends avoid “advance twice and guess” at multiple call sites; they either implement a disciplined lookahead function or keep a single grammar path and resolve ambiguity later.

---

### B) Node union fields are being reused inconsistently (semantic garbage risk)

Inside `parse_statement_or_label`:

* `T_AB` (abort?) builds `ND(AB)` but stores into `raise_stmt.exception_choice` (line ~3621).
* `T_DEL` builds `ND(DL)` but stores into `exit_stmt.condition` (line ~3613).
* A bunch of statement kinds share random union slots.

This is a **structural design smell**, not just naming:

* it guarantees downstream phases need “if kind then interpret field X as Y”.
* it leads to bugs where you read the wrong union member and get silent nonsense.

GNAT-style ASTs keep node payloads tight and specific. If you want a “compact union”, fine—but then enforce **one consistent payload layout per node kind**.

---

### C) `parse_loop` discards subtype marks in `for I in Subtype range ...`

In the `for` loop branch:

```c
Syntax_Node *rng = parse_range(parser);
if (parser_match(parser, T_RNG)) { rng = RN(low..high); }
```

(lines 3249–3257)

If the source is `for I in Integer range 1 .. 10`, then `parse_range` likely returns the name (`Integer`) and then you overwrite `rng` with a plain `RN(1..10)`. **The subtype mark is lost.**

That’s not an edge case; it’s valid Ada syntax. You need a node that represents **subtype_mark + range_constraint**, not “pick one”.

---

### D) `use` clause AST shape depends on count

`use A;` returns a single `N_US`.
`use A, B;` returns `N_LST` of `N_US` nodes. (lines 4702–4722)

That forces every consumer to handle two shapes for the same construct. Keep it uniform: always return list, even if size 1.

---

### E) `parse_representation_clause` is mostly “parse and discard”

* Many branches return `0` after consuming syntax (record rep clause, expression form, etc.). (lines 4082–4100)
* Only a few pragmas create a `Representation_Clause*` and even those store minimal info.

So you “accept syntax” but preserve little meaning. If this is intentional (“parse-only”), then don’t pretend you’re producing a clause object sometimes and `0` others—pick a consistent representation.

Also: it still uses the **misnamed allocator** `reference_counter_new(...)` from earlier, which is already a maintainability hazard.

---

# 2) Unnecessary / low-value functions or blocks (in this chunk)

There aren’t many “one-liner pointless functions” here, but there **are pointless structural choices**:

* Returning different node shapes for `use` (single vs list) is pure complexity with no payoff.
* The repeated “generic instantiation actual-parameter parsing” blocks in procedure/function/package could be one helper:

  * `parse_generic_actuals(Parser*, Token_Kind closing)` returning `Node_Vector ap`.
* The repeated “parse a comma-separated identifier list then `:` then type then optional init” occurs in record components and object decls. You can factor:

  * `parse_identifier_list(Parser*)`
  * `parse_object_like_decl_tail(ids, allow_init, allow_renames, ...)`

---

# 3) Duplicated code patterns (best targets to collapse)

### 1) “ID list” parsing appears everywhere

Examples:

* record fields (3934–3947),
* variant fields (3992–4006),
* object/exception decl (4739–4777),
* discriminant specs (4269–4289),
* etc.

This should be one helper returning `Node_Vector` of `N_ID` nodes or `String_Slice`s.

### 2) Generic instantiation actuals are copy/pasted 3 times

Procedure (4367–4396), function (4425–4454), package (4533–4562). Same structure, same association logic.

### 3) Accept / entry parsing duplicated 3 times (see §1A)

This is the biggest bloat multiplier in this chunk.

### 4) Record component creation duplicated in normal and variant parts

You create `N_CM` nodes and fill identical fields in two places (3949–3963, 4008–4021). That’s a prime candidate for a helper:

* `make_component_decl(name, ty, init, offset, discriminants)`.

---

# 4) Sketchy hacks to “ignore incorrectness”

### A) “Convert call to index if it appears on LHS”

In assignment parsing:

```c
if (expression && expression->k == N_CL) { expression->k = N_IX; ... }
```

(lines 3681–3688)

This is an understandable hack (Ada ambiguity: `A(I)` could be call or indexing), but it’s also **a semantic decision in the parser** based on context.

Better model:

* keep a single node kind like `N_APPLY(prefix, args)` for all `X(...)`,
* later resolution decides whether it’s an index or call,
* in contexts where only “name” is legal (assignment target), you *validate* rather than mutate the node kind.

Mutation like this creates weird states if the same node is shared or reused.

### B) “Offsets” are not offsets

Record components set `component_decl.offset = of++` (3954 etc). That’s an ordinal, not a byte/bit offset. The field name will mislead every future reader and likely cause layout bugs later.

### C) Variant part `size = of`

`vp->variant_part.size = of;` (4027) is not a size; it’s “number of components seen so far”. Again: misleading naming that will force hacks later.

---

# 5) Against GNAT LLVM practices (in this chunk)

GNAT/LLVM frontends are very strict about:

* preserving structure needed for legality checks and later semantic phases,
* not throwing away important syntactic information.

Here you frequently:

* parse valid syntax but **drop subtype marks** (`for` loop range),
* parse rep clauses/pragmas but **mostly discard**,
* encode multiple constructs into reused union fields (harder to reason / verify),
* choose AST shapes based on list length (`use`).

These are exactly the kinds of shortcuts that create downstream “special cases everywhere”, which GNAT tries hard to avoid.

---

# 6) Function-by-function breakdown + quality ratings (1–10)

### Statements

* `parse_case` (3201–3236): **5/10**

  * Good: structure is clear; collects alternatives.
  * Bad: “choice parsing” is ad-hoc (`N_ID + RANGE` special-case); uses `exception_handler.statements` to store alternative statements (weird field reuse).

* `parse_loop` (3237–3274): **4/10**

  * Good: supports `while` and `for`, reverse, range.
  * Bad: loses subtype mark on `Subtype range ...`; iterator represented as a binary `IN` node (odd AST design); end label not checked.

* `parse_block` (3275–3292): **7/10**

  * Straightforward; label support; exception handlers.

* `parse_statement_list` (3293–3479): **3/10**

  * Clearly doing Ada `select` forms, but node fields are confusing (`abort_stmt` vs `select_stmt`), and it’s the epicenter of duplicated accept parsing + token-snapshot guessing.

* `parse_statement_or_label` (3481–3709): **4/10**

  * Good: supports `<<label>>` and `id:` labels and wraps some labeled statements into blocks.
  * Bad: union field misuse (abort/delay/raise); call/index ambiguity hacked; label stack only pushed here (no visible pop discipline).

* `parse_statement` (3710–3726): **7/10**

  * Good: uses `parser_check_progress` properly to prevent infinite loops.

* `parse_handle_declaration` (3727–3760): **7/10**

  * Reasonable handler parsing; good progress checks; supports `others`.

### Type definitions / representation

* `parse_type_definition` (3761–4066): **6/10**

  * Good: surprisingly wide coverage: enums, ranges, mod, digits, delta, arrays, records with discriminants + variant parts, access/private/limited.
  * Bad: record/variant component duplication; misleading “offset/size”; lots of structure packed into generic list vectors; still relies heavily on “name vs expression” confusion.

* `parse_representation_clause` (4067–4253): **4/10**

  * Good: recognizes many pragmas; parses record rep clauses.
  * Bad: mostly discards information; huge string-compare ladder; misnamed allocator; inconsistent return (`Representation_Clause*` sometimes, `0` often).

### Declarations

* `parse_declaration` (4254–4779): **5/10**

  * Good: covers many declaration forms; bodies/separate/renames/generic instantiations; uses `parser_check_end_identifier` sometimes.
  * Bad:

    * generic instantiation actuals copy/pasted 3 times,
    * task entry disambiguation hack duplicated again,
    * `use` clause shape depends on count,
    * type derivation constraint parsing partially discarded (digits/delta cases just “parse and ignore”).

* `parse_declarative_part` (4780–4800… continues later): **7/10 so far**

  * Good: progress-guarded loop; correct stop tokens; handles rep clauses specially.

---

# 7) Competing implementations (conceptual) that would simplify this chunk

### 1) Unify “apply” nodes (`X(...)`) and defer call-vs-index resolution

Instead of generating `N_CL` sometimes and `N_IX` other times and then mutating:

* parse `X(args)` into `N_APPLY(prefix, args)` always
* semantic pass decides:

  * if `prefix` is an array/object → indexing
  * if callable → call
  * if used as assignment target and callable → error

This matches how Ada ambiguity is normally handled.

### 2) Centralize the parentheses disambiguation into one helper

You can keep the current heuristic if you must, but **do it once**.

### 3) Make declaration/field parsing table-driven

A lot of the “identifier list + colon + type + init + semicolon” logic can be shared across:

* object decls
* record components
* variant components
* discriminant specs (with slight tweaks)

## Part 4 — lines 4801–6400 (read & analyzed)

This chunk transitions from **parser top-level** into **type system + symbol manager + freezing + overload resolution heuristics**.

---

# High-impact problems (correctness / maintainability)

### 1) “Store a pointer by memcpy into the wrong union field” (real bug / UB-risk)

In `parse_declarative_part` when it sees rep clauses (`FOR`, `PGM`) it does:

* creates `ND(RRC, …)` then
* `memcpy(&n->aggregate.items.data, &r, sizeof(Representation_Clause *));` (lines 4802–4804 and 4813–4815)

That’s writing a `Representation_Clause*` into a field that *looks like* a `Node_Vector`’s `data` pointer. This is a **struct layout / union abuse** that will eventually explode when code assumes `aggregate.items` is a real vector.

If you want rep clauses in the AST, give `N_RRC` a proper payload (`Representation_Clause *rc;`) instead of smuggling it into unrelated storage.

---

### 2) `parse_context` uses one location for everything

`Source_Location lc = parser_location(parser);` is taken once (line 4825), and then all `with`/`use` nodes reuse that same `lc` (lines 4833–4845). Diagnostics later will point to the wrong token for almost every clause.

Same issue shows up elsewhere in the parser earlier; here it’s blatant.

---

### 3) Separate compilation recursively parses another file with minimal hygiene

In `parse_compilation_unit`’s `SEPARATE` handling (lines 4875–4925):

* It searches include paths + extensions and reads file into `psrc` (lines 4885–4913).
* No checks for `malloc`, `fread` short read, or `ftell` failure.
* It creates a nested `Parser pp = {lexer_new(psrc, sz, fn), …};` and calls `parse_compilation_unit(&pp)` recursively (lines 4914–4923).
* `psrc` is never freed (maybe “arena compiler” makes this acceptable, but here it’s plain heap).

Also: mixing compilation-unit parsing as a side-effect of encountering `separate (...)` is… *very* non-GNAT-like. GNAT treats separate units as their own compilation units, with controlled dependency management.

---

### 4) Type metrics documentation contradicts code (bits vs bytes) and it bleeds into layout

* `Type_Info`’s comment says `size/alignment` are **in bits** (lines 5115–5126).
* `type_new` assigns `t->size = DEFAULT_SIZE_BYTES; t->alignment = DEFAULT_ALIGN_BYTES;` (lines 5785–5788) and the note above says “Sizes are stored in BYTES internally” (lines 5775–5777).
* `find_type`’s record layout uses `align_to(offset, c_align)` and offsets are treated as bytes (lines 6145–6168).

So: the *system can’t decide units*. That’s the kind of mismatch that produces “works for some tests, fails on record layout / rep clauses / packed” later.

GNAT LLVM keeps these invariants extremely explicit (bits vs bytes) and enforces them consistently.

---

### 5) `tk_repr_category` and `tk_llvm_type` disagree with the declared representation categories

You define `REPR_CAT_STRUCT`, but:

* `tk_repr_category` returns FLOAT, POINTER, else INTEGER (lines 5067–5071) — it never returns `REPR_CAT_STRUCT`.
* `tk_llvm_type` returns `LLVM_PTR` for records/arrays/access/string (lines 5083–5086).

So the “representation category” abstraction is currently decorative; the implementation collapses composites into pointer-like representation. That’s a valid strategy (pass-by-reference), but then your enums and docs should reflect that, not pretend you support struct-valued representation.

---

### 6) `symbol_find_use` recursion-guard is almost certainly wrong

This line is the tell:

```c
uint32_t h = symbol_hash(nm) & 63, b = 1ULL << (symbol_hash(nm) & 63);
```

(lines 5464–5467)

You’re using the **same 6 bits** both as the array index and the bit position. That means you effectively track only 64 “visited classes”, not 4096 distinct names. It’ll spuriously block legitimate nested `use` processing, and also fail to block some cycles.

If you wanted 4096 bits, the usual pattern is:

* `idx = hash >> 6; bit = 1ULL << (hash & 63);`

---

### 7) Pointer-value hack in overload scoring

In `symbol_find_with_args`:

```c
if (c->parent and (uintptr_t)c->parent > 4096 and c->parent->k == 6) ...
```

(lines 5737–5739)

That’s a giant red flag: it implies `parent` is sometimes **not a real pointer** (or may be uninitialized / poisoned), so you’re guarding dereference with “address looks plausible”.

This is exactly the kind of “ignore incorrectness” hack you asked to be called out: it’s papering over a deeper invariant violation (parent must either be NULL or a valid `Symbol*`).

---

# Duplicated / bloaty patterns worth collapsing (in this chunk)

### A) Two parallel “representation category” computations

* `tk_repr_category(Type_Kind)` (5067–5071)
* `representation_category(Type_Info*)` (6289–6313)

These overlap but differ. Pick one abstraction level and delete the other, or make `representation_category` the single source of truth and keep `tk_repr_category` as a tiny helper used only there.

### B) Two parallel “canonical/base type” traversals

* `type_canonical_concrete` (6252–6265)
* `semantic_base` (6314–6330)

They’re close enough that they’ll drift. Unify them or make one call the other with clear rules (derived vs subtype vs universal types).

### C) Two symbol-lookup overload resolvers

* `symbol_find_with_arity` (5551–5670)
* `symbol_find_with_args` (5702–5766)

Both do candidate collection, scope filtering, overload scoring. This should be a single core routine parameterized by:

* whether to enforce arity
* whether to enforce named-arg matching
* whether to consider `tx` compatibility

Right now fixes must be applied twice.

---

# What goes against GNAT LLVM practices (here)

1. **Unit consistency** (bits/bytes) is non-negotiable in GNAT-style lowering; here it’s contradictory across comments and code and will break layout/rep clauses.
2. **Parser doing dependency loading** (`separate` pulls in files and re-parses inside parse loop) mixes phases and makes elaboration / dependency ordering harder.
3. **Heuristic pointer guards** like `(uintptr_t)ptr > 4096` are the opposite of GNAT’s “maintain invariants, assert early”.

---

# Function-by-function quality ratings (1–10)

### Parser / compilation unit

* `parse_declarative_part` (4780–4822): **3/10** — progress guard is good, but the `memcpy` pointer-smuggling is a serious structural flaw.
* `parse_context` (4823–4857): **5/10** — works, but wrong locations; also weirdly stores `PGM` decls into `use_clauses`.
* `parse_compilation_unit` (4858–4937): **4/10** — merges contexts and handles `separate`, but does IO + recursive parsing unsafely.
* `parser_new` (4938–4944): **8/10** — simple and clean.

### Type kind/predicates/representation

* `tk_is_*` predicates (5011–5048): **8/10** — good, cheap, readable.
* `tk_repr_category` (5067–5071): **4/10** — doesn’t honor `REPR_CAT_STRUCT`; misleading API.
* `tk_llvm_type` (5074–5089): **6/10** — pragmatic, but inconsistent with the representation-category story.

### Symbol table core

* `symbol_hash` (5298–5301): **8/10**
* `symbol_new` (5302–5312): **7/10** — fine for arena allocation; but doesn’t initialize many fields explicitly (relies on arena returning zeroed? it doesn’t).
* `symbol_add_overload` (5313–5344): **6/10** — workable; UID scheme is ad-hoc and truncates to 32-bit.
* `symbol_update_uid` (5345–5368): **5/10** — exists only because `parent` is set late; better: set parent before hashing, or compute UID lazily once.
* `symbol_find` (5369–5391): **6/10** — visibility logic is messy and duplicates passes.
* `suggest_similar_identifiers` (5393–5460): **7/10** — decent “did you mean”; but O(N) over all buckets each time.
* `symbol_find_use` (5462–5530): **3/10** — recursion guard indexing is likely wrong; also “make ALL symbols whose parent matches visible” is a blunt instrument.
* `generic_find` (5531–5549): **7/10**

### Overload resolution helpers

* `symbol_find_with_arity` (5551–5670): **4/10** — big, duplicated logic; scope/visibility rules are hard to reason about; returns arbitrary fallback.
* `params_match_named` (5673–5700): **6/10** — useful, but assumes association format and checks only first choice node.
* `symbol_find_with_args` (5702–5766): **3/10** — same complexity plus the `(uintptr_t) > 4096` hack.

### Type construction / predefined environment

* `type_new` (5780–5789): **4/10** — unit confusion (bytes vs bits) + wrong comment (“64-bit aligned” but uses defaults).
* `symbol_manager_init` (5808–5903): **6/10** — fine bootstrap, but hard-codes TEXT_IO + builtins in a way that will sprawl.

### Generated operators (very shaky)

* `generate_equality_operator` (5904–6000): **2/10**

  * parameter names are nonsense (`Source_Location`, `Rational_Number`)
  * for record case it sets top node to `T_EQ` but never assigns the RHS, and then builds `AND` chain into the LHS. That’s structurally wrong.
* `generate_assignment_operator` (6001–6070): **4/10** — same naming weirdness; workable for shallow records/arrays but very ad-hoc.
* `generate_input_operator` (6071–6099): **5/10** — limited and record-only; ok as a stub.

### Freezing / layout

* `find_type` (6121–6193): **6/10** — the record layout loop is coherent, but relies on the byte/bit mess and defaults too often; array sizing for unconstrained is “0 bytes” which is representation-dependent.
* `find_symbol` (6194–6202): **7/10**
* `find_ada_library` (6203–6215): **6/10**
* `symbol_compare_parameter` (6216–6226): **4/10** — name doesn’t match behavior (it mutates scope/stack), unclear intent.
* `symbol_compare_overload` (6227–6245): **4/10** — visibility mutation pass is hard to justify; likely papering over earlier visibility design.

### Canonicalization / compatibility

* `type_canonical_concrete` (6252–6265): **7/10**
* `is_system_address` (6276–6288): **6/10**
* `representation_category` (6289–6313): **6/10** — better than `tk_repr_category`, but still muddled with “arrays/records as pointer”.
* `semantic_base` (6314–6330): **6/10**
* `is_integer_type / is_real_type / is_discrete / is_array / is_record / is_access` (6331–6356): **7/10**
* `is_check_suppressed` (6357–6365): **7/10**
* `type_compat_kind` (6366–6408): **5/10** — the “generic instantiation = same name+kind ⇒ same type” rule is dangerous; needs stronger identity.
* `type_scope` (6409–6430): **6/10**
* `type_covers` (6431–6455): **6/10** — pragmatic; string special-casing is fine but should be centralized.

---

# Competing implementations (conceptual improvements)

### 1) Kill the pointer-smuggling rep clause hack

Make `N_RRC` store a real `Representation_Clause*` payload. If you want everything “as Syntax_Node”, then wrap `Representation_Clause` in a node kind with a dedicated union member.

### 2) Replace the `use` visibility hacks with a proper visibility model

Right now `symbol_find_use` mutates lots of unrelated symbols’ `visibility`. A GNAT-like approach:

* maintain a `Use_Clause` stack per scope
* symbol lookup consults the use-stack rather than globally flipping bits on symbols

### 3) Make overload resolution a single engine with a scoring callback

Unify `symbol_find_with_arity` + `symbol_find_with_args`. Provide:

* candidate enumeration (visibility/scope)
* per-overload scoring (arity, named args, type compatibility)
* stable tie-breakers (scope, declaration order)

…and drop the “pointer > 4096” hack by enforcing `parent` invariants.

## Part 5 — lines 6401–8000 (read & analyzed)

This chunk is where the frontend really starts to “be a compiler”: **subtype resolution**, **constraint checks**, **aggregate normalization**, a bolted-on **validation pass** for operator typing, and the first half of **`resolve_expression`** (where you also do constant folding + AST mutation).

---

# 1) The most serious correctness hazards

### A) Bounds are stored as `int64_t`, but sometimes you bit-cast doubles into them

In `resolve_subtype` for subtype/range nodes you compute bounds like:

* if bound is `N_REAL`, you do `((union { double d; int64_t i; }){.d = lo->float_value}).i` (and variants with unary minus).
* later there’s `type_bound_double(int64_t b)` that reinterprets `int64_t` back to `double`.

So `low_bound/high_bound` are **“sometimes integer values, sometimes IEEE-754 bits”** depending on the type kind and which path produced them.

That’s an extremely fragile implicit convention:

* nothing in `Type_Info` enforces which interpretation is valid,
* you can easily compare an “int bound” to a “float-bit bound” and get nonsense,
* it forces every caller to remember “if float: decode”.

This is exactly the kind of hidden invariant GNAT avoids: it would represent float bounds as float expressions or rational bounds, not a tagged bit-cast stored in an integer slot.

**Bloat symptom:** the same giant ternary chain to compute `lov/hiv` is duplicated 3 times inside `resolve_subtype`.

---

### B) `is_compile_valid()` calls `resolve_expression(0, ...)` with a null symbol_manager

```c
if (node->k == N_CL) {
  for (...) resolve_expression(0, arg, 0);
}
```

If any argument contains `N_ID`, `resolve_expression` calls `symbol_find(symbol_manager, ...)` → dereference null → crash.

Even if this happens “rarely”, it’s an outright structural bug. If you wanted a “shape-only pass”, it must not call the resolver, or it must pass a real manager.

---

### C) `has_return_statement()` is flat-out wrong by implementation

```c
for (...) if (statements[i]->k != N_PG) return 1;
return 0;
```

This returns true for **any** non-pragma statement (assignments, null, if…), not for returns.

If it’s used for “function must return” checking, it will accept nonsense. If it’s unused, it’s dead weight.

---

### D) `normalize_record_aggregate()` uses a fixed `bool cov[256]`

Records can exceed 256 components, and your component “offset” is just an ordinal index.
This becomes:

* out-of-bounds writes (memory corruption), or
* silent under-checking.

This is one of the biggest “quiet corruption” issues in this chunk.

---

### E) Attribute typing is often *semantically wrong*

Examples in `resolve_expression` / `validate_attribute`:

* `'FIRST` / `'LAST` for arrays should be about **index bounds**; you often return `ptc->element_type` if `element_type` exists. That’s conceptually wrong.
* `'ACCESS` sets `node->ty = type_new(TYPE_ACCESS, N);` but **does not set designated type** → a meaningless access type.
* `'ADDRESS` builds a synthetic `SYSTEM.ADDRESS` selection node to look it up. That’s a hacky dependency on the package model; it should just reference the predefined symbol/type.
* Validation checks for `'FIRST/'LAST/'LENGTH/'RANGE` say “array or discrete type”; but the actual typing logic returns “maybe element type, maybe pt, maybe int” in ways that don’t match Ada rules.

---

# 2) “Sketchy hacks designed to ignore incorrectness”

### A) Permissive-by-default typing in `types_compatible_for_operator`

```c
if (!left || !right) return 1; // Be permissive on missing types
default: return 1; // Unknown operator - be permissive
```

That turns the validation pass into “best-effort suggestions” rather than enforcement.

If this is intentional (a “lint-ish pass”), fine—but then don’t increment `error_count++` (you do), and don’t call it “systematic semantic checking”.

### B) Constraint checks compare against TY_INT bounds

In `chk()`:

```c
if ((is_discrete(t) or is_real_type(t)) and
    (node->ty->low_bound != TY_INT->low_bound or node->ty->high_bound != TY_INT->high_bound) ...)
  return make_check(...);
```

That’s not “range check”: it’s “anything not using the universal-int bound sentinel triggers a check”.
It also breaks badly if bounds are stored as float-bitcasts.

---

# 3) Duplicated code patterns that should be collapsed

### A) Bound extraction is duplicated repeatedly

The `lov/hiv` calculation in `resolve_subtype` is duplicated in:

* constraint.constraints[0] range path,
* constraint.range_spec path,
* direct `N_RN` path,
  and similar logic repeats again in array subtype cases.

This should be one helper:

* `static bool eval_static_bound(Symbol_Manager*, Syntax_Node*, int64_t *out, bool allow_float_bits, ...)`

### B) Two separate “operator typing” systems

* `resolve_expression` sets types ad-hoc (often “type of left operand”).
* then a separate `types_compatible_for_operator` + validation traversal tries to enforce LRM-ish constraints.

These will drift. Pick one of:

1. enforce operator compatibility during resolution (recommended), OR
2. make resolution minimal and validation authoritative.

Right now you get both *and* contradictions.

### C) String compatibility logic duplicated

You already special-case strings in `type_covers`, then do it again in `types_compatible_for_operator` equality.

---

# 4) GNAT LLVM mismatches (conceptual)

GNAT’s frontend discipline is basically:

* resolve names/types without mutating the tree into a different shape,
* keep explicit representations for constraints and bounds,
* ensure invariants (no “sometimes bitcast” fields),
* don’t mix semantic resolution with normalization passes that can crash on missing context.

This chunk goes the opposite way:

* `resolve_expression` changes node kinds (`ID → INT/REAL`, `CHAR → ID`),
* constraints are stored in overloaded `int64` slots with implicit “bits-as-double” conventions,
* aggregate normalization and checking call into resolution with null context.

---

# 5) Function-by-function breakdown + ratings (1–10)

### Type compatibility / coverage

* `type_scope` (6409..): **6/10** — workable scoring, but depends on shaky `type_compat_kind` semantics.
* `type_covers` (31–55 in this excerpt): **5/10** — extremely permissive (`all discrete cover all discrete`), plus string special cases.

### Subtype resolution

* `resolve_subtype` (56–417): **3/10**

  * Good: tries to construct constrained array/subtype types.
  * Bad: defaults to `TY_INT` on many failures; duplicates bound logic; stores float bounds as bitcasts into `int64_t`; mixes package lookup logic manually; calls `resolve_expression` inside (phase mixing).

### Literals / checks helpers

* `symbol_character_literal` (418–436): **7/10** — reasonable enum literal lookup; slightly brute-force but ok.
* `make_check` (437–444): **8/10** — clean.
* `is_unconstrained_array` (445–450): **8/10** — good sentinel documentation.
* `base_scalar` (451–461): **6/10** — does the job, but the loop condition is odd; easy to misread.
* `is_unc_scl` (462–468): **6/10** — relies on base_scalar + bound equality (again: bound representation hazard).
* `type_bound_double` (469–478): **4/10** — the bitcast convention is the problem; as a helper it’s fine but the design is hazardous.
* `descendant_conformant` (479–494): **4/10** — name suggests “conformant”, but returns true when defaults differ; confusing and likely inverted logic.
* `chk` (495–513): **3/10** — uses TY_INT as universal bound sentinel; mixes multiple check kinds; bound representation makes it unreliable.
* `range_size` (514–517): **8/10**
* `is_static` (518–522): **7/10** — good for integer staticness; doesn’t include enum literals etc.

### Aggregate normalization

* `find_or_throw` (523–535): **7/10** — finds `others =>`; ok.
* `normalize_array_aggregate` (536–632): **5/10**

  * Good: fills positional + named choices, supports `others`, detects duplicates.
  * Bad: hard cutoff `asz > 4096` silently bails; assumes range-choice bounds are ints; alloc/oom unchecked.
* `normalize_record_aggregate` (633–670): **2/10** — fixed `cov[256]` is a memory-corruption bug for bigger records; also uses `offset` as array index.
* `universal_composite_aggregate` (671–684): **5/10** — plausible heuristic, but Ada index bounds aren’t “size-based” in general.

### “Compile validity”

* `is_compile_valid` (685–702): **2/10** — null symbol_manager bug; name doesn’t match behavior (it mutates aggregates).
* `has_return_statement` (703–709): **1/10** — wrong logic, misnamed.

### Operator typing + validation pass

* `types_compatible_for_operator` (712–777): **5/10** — decent intent, but too permissive and not aligned with resolver behavior.
* `validate_binary_operation` (788–838): **6/10** — good error messages; but depends on the permissive compat routine and increments `error_count` again.
* `validate_attribute` (841–877): **5/10** — minimal checks; incomplete attribute set.
* `validate_expression` (880–933): **6/10** — traversal ok, but doesn’t validate association nodes inside aggregates.
* `validate_statement` (936–1017): **6/10** — basic coverage; ok.
* `validate_statement_list` (1020–1025): **8/10**
* `validate_compilation_unit` (1028–1067): **7/10** — decent entry point.

### Expression resolution (partial)

* `resolve_expression` (1069–1600 shown): **4/10**

  * Good: resolves identifiers to symbols/types; handles enum literal by context; does some folding.
  * Bad:

    * heavy AST mutation (ID→INT/REAL), which blurs “parse tree” vs “typed tree”,
    * typing for many ops devolves to “type of left operand”,
    * attribute typing is ad-hoc and often wrong,
    * package selection resolution is huge and duplicated from `resolve_subtype`,
    * constant folding assigns `TY_UINT` even for negative results (naming/type intent mismatch).

---

# 6) Best “no-shortcut” improvements (big payoff, minimal rewrites)

### 1) Fix the bound representation

Do **not** store float bounds as bitcasts in `int64_t`.
Options:

* store bounds as `Syntax_Node*` expressions (preferred for Ada), or
* store a tagged union `{ kind: INT|FLOAT, i64, f64 }` in `Type_Info`.

This single change will simplify `chk`, `type_bound_double`, and half of `resolve_subtype`.

### 2) Remove the null-context resolver call

`is_compile_valid` must not call `resolve_expression(0, ...)`. Either:

* pass a real `Symbol_Manager*`, or
* split “normalize” from “resolve”.

### 3) Replace `normalize_record_aggregate` coverage tracking

Use a dynamically-sized bitmap/byte-array sized to `rt->components.count`, not 256.

### 4) Unify operator typing

Pick: resolver enforces operator type legality OR the validation pass does.
Right now you have two partially conflicting systems.

## Part 6 — lines 8001–10100 (read & analyzed)

This chunk finishes the remaining cases of **`resolve_expression`**, defines **`resolve_statement_sequence`**, applies rep clauses via **`runtime_register_compare`**, then introduces the whole **generic-instantiation cloning subsystem** (`node_clone_substitute`, `generate_clone`, etc.) and starts a big **`resolve_declaration`** (objects/types/subprograms/packages).

---

# 1) Biggest correctness / design hazards

### A) Aggregate resolution still calls the null-context resolver (crash path)

In `resolve_expression` for `N_AG` you do:

* resolve all items (good),
* then `is_compile_valid(tx, node);` (line ~8074 in your local numbering).

But `is_compile_valid()` (from the prior chunk) calls `resolve_expression(0, ...)` for call args, which **will crash** on any `N_ID` lookup. This is now a *direct* path from normal expression resolution into a null deref.

**Fix:** split “normalize aggregate” from semantic resolution and never call the resolver with a null symbol manager.

---

### B) Call-vs-indexing ambiguity is still resolved by mutating node kind

In `N_CL` you:

* resolve `function_name` and args,
* if `ft` is `TYPE_ARRAY` and the symbol isn’t a function (`fn_sym->k != 5`) you rewrite the node into `N_IX` and recurse.

That’s the same parser-time hack you had earlier, now repeated at semantic time. It creates weird states and forces downstream passes to assume “calls are *really* calls”.

**Better model:** parse/resolve `X(...)` as a single `APPLY(prefix, args)` node; resolve to call vs indexing in one place (or later).

---

### C) Type checking for assignment is explicitly disabled as a workaround

In `resolve_statement_sequence` for `N_AS`, you compute `tgt/vlt`, detect mismatches, then intentionally do nothing with a long comment about generic instantiation scoping issues.

That’s a *big* “ignore incorrectness” hack: you’re suppressing real type errors to hide symbol-table contamination. Then you forcibly set `value->ty = target->ty` and emit runtime checks.

**This will accept invalid programs** and produce IR that “sort of runs” until it doesn’t.

---

### D) Representation/layout units are still internally inconsistent (bits vs bytes)

You still mix:

* `t->size = of * 8` (later in this function’s record layout logic),
* and in `runtime_register_compare` you set `t->size = to_bytes_u32(bt)` and comment “Bits → bytes”.

So even within the same phase you treat `t->size` as bits sometimes and bytes other times.

**This is a structural time bomb** for record layout, packed records, array sizing, attribute `'Size`, etc.

---

### E) Record rep clause size computation is likely wrong

In `runtime_register_compare` case 3 (record rep clause):

* you loop components, and do `bt += cp->component_decl.bit_offset;`
* then `t->size = to_bytes_u32(bt);`

But earlier your “bit_offset” was used more like a *position* or a *range* depending on parsing; here it’s treated like a *width*. There’s no overlap/ordering validation, and “size = sum(bit_offset)” is almost never the correct rule for rep clauses.

---

# 2) Duplicated code patterns to collapse (big bloat reducers)

### A) Stub-symbol lookup is duplicated (procedures vs functions)

`N_PB` and `N_FB` contain almost identical “find existing stub” scans:

* full table walk,
* parent-kind filter,
* name match.

This should be one helper:

* `find_existing_subprogram_stub(kind, name, parent_sym, allow_generic_inst)`.

### B) `.adb` body loading repeats (generic template load + nested instantiation load)

Both `generate_clone` and `resolve_declaration(N_GINST)` re-implement:

* build lowercased path,
* try include paths,
* parse compilation unit,
* search for body node.

Factor into one loader/cacher keyed by `(unit-name, kind)`.

### C) Repeated “only pragmas” check uses misnamed helper everywhere

The code uses `has_return_statement()` as “has any non-pragma statement”. That works for the current *purpose*, but the name is wrong and it’s called all over.

Rename + implement properly:

* `has_executable_statement()` for pragma-only rule,
* `contains_return_statement()` if you later need real return checking.

---

# 3) Sketchy hacks / papering over invariants

### A) `node_clone_substitute` depth guard returns original nodes (aliasing across instantiations)

If clone depth exceeds 1000, it returns the **original** node (not a partial clone). That means multiple generic instantiations can end up sharing subtrees → later mutations (type setting, symbol links, checks inserted) bleed between instantiations.

If you need a depth guard, you still must return a valid clone (or fail the instantiation), not reuse original nodes.

### B) Cloning does not zero-init node payloads

Arena nodes aren’t zeroed, and `node_clone_substitute` only fills fields for the handled `switch` cases. For any partially-handled node kind, you can leave garbage pointers in union members.

### C) “TYPE_STRING” is used as the *subprogram type container*

You repeatedly create `Type_Info *ft = type_new(TYPE_STRING, ...)` for subprograms. That’s a misrepresentation: it makes later code and predicates lie and encourages more hacks (`tk_llvm_type` etc. already look odd because of this).

---

# 4) GNAT LLVM mismatches (in this chunk specifically)

* **Generics via AST cloning** is the opposite of how GNAT-style frontends maintain correctness. GNAT keeps the generic template + an instantiation environment and does semantic resolution with proper symbol maps; it does not rely on “clone AST, substitute identifiers, hope no aliasing”.
* **File parsing during semantic resolution** (`generate_clone` and nested generic body chasing) mixes phases and makes dependency ordering nondeterministic.
* **Turning off type errors** (assignment) to hide symbol-table bugs is exactly what GNAT avoids; GNAT fixes the symbol identity rules instead.

---

# 5) Function-by-function quality ratings (1–10)

### `resolve_expression` (remaining cases shown here: `N_CL/N_AG/N_ALC/N_RN/N_ASC/N_DRF/N_CVT/N_CHK`)

**4/10**

* Good: named-arg resolution tries `symbol_find_with_args`; allocator discriminant/default handling is thoughtful.
* Bad: mutates node kinds for call/index; calls `is_compile_valid` crash path; weak typing defaults (`TY_INT` everywhere).

### `resolve_statement_sequence`

**4/10**

* Good: generally walks and resolves subexpressions; introduces loop iterator symbol; handles accept parameters with scoped compare calls.
* Bad: assignment type checking explicitly neutered; iterator resolved with `TY_BOOL` even though it’s not boolean; continues union-field misuse (`N_DL` uses `exit_stmt.condition`, etc.).

### `runtime_register_compare`

**4/10**

* Good: at least applies some pragmas/clauses (enum rep, address, pack, inline, import).
* Bad: hard-coded numeric clause kinds; record rep size computation likely wrong; contributes to bits/bytes inconsistency; weak validation.

### `is_higher_order_parameter`

**5/10**

* Intent is clear (inherit ops), but it shallow-copies bodies/specs and risks aliasing; also injects nodes into `operations` without clear ownership rules.

### `resolve_array_parameter`

**6/10**

* Does useful mapping for formal subprogram actuals; but it linearly scans tables and relies on magic `Symbol->k` meanings.

### `normalize_compile_symbol_vector`

**6/10**

* Practical clone-and-substitute helper, but arbitrary cutoffs (`>100000`), heap snapshot copy, and silent failure mode.

### `node_clone_substitute`

**3/10**

* Central to generics, but depth-guard aliasing + partial initialization + symbol handling inconsistencies make it fragile.

### `match_formal_parameter`

**7/10**

* Simple and readable.

### `generate_clone`

**4/10**

* Works in some cases, but mixes parsing/loading into semantics and relies on scope heuristics to decide which body “belongs” to which generic.

### `eval_const_expr`

**5/10**

* Useful, but unsafe: no overflow checks, division-by-zero returns 0, negative exponent returns 1, attributes rely on the shaky bound representation.

### `get_pkg_sym`

**7/10**

* Reasonable “find canonical package symbol” helper.

### `resolve_declaration` (portion covered here: `N_GINST`, `N_RRC`, `N_OD`, start of `N_TD`, `N_GSP`, `N_PB/N_PD`, `N_FB/N_FD`, `N_PKS`, start of `N_PKB`)

**5/10**

* Good: tries to reconcile separate stubs vs bodies; tracks parent/uid; manages generic instantiations.
* Bad: enormous phase mixing + repeated full-table scans + continued pointer-smuggling for rep clauses + many heuristics.

---

# 6) Competing implementations (what would simplify this entire section)

### 1) Replace AST-clone generics with an instantiation environment

Instead of cloning + substituting:

* keep the generic AST once,
* build an environment mapping formal → actual symbols/types,
* resolve with scoped symbol lookup using that environment.
  This eliminates aliasing bugs, depth guards, and most of `node_clone_substitute`.

### 2) Centralize “separate body” and “generic body” discovery

Have a single unit loader that:

* caches parsed units by normalized name,
* separates parse from semantic resolution,
* provides stable “spec/body pairs”.

### 3) Make type checking real again by fixing symbol identity

The fact you had to disable assignment compatibility implies type identity across instantiations is broken.
Fix that at the root (unique instantiation IDs, parent chain, UID rules) rather than suppressing errors.

## Part 7 — lines 10101–12000 (read & analyzed)

This chunk is **(1) elaboration & unit loading**, then **(2) “use clause” visibility hacking**, then **(3) the entire start of LLVM IR generation** (type mapping, mangling, static-link access, aggregates, fat pointers, and early `generate_expression`).

---

# 1) Highest-impact problems

### A) `read_file()` / `lookup_path()` leak source buffers by design (but without admitting it)

* `read_file()` allocates `malloc(z+1)` and returns it (10178–10191). No size return, no error checks.
* `lookup_path()` calls `read_file(af)` and returns the pointer directly (10310–10331).
* That returned `.ads` source is **never freed** anywhere here.

If the intention is “keep all sources resident” that’s fine, but then:

* it should be explicitly documented as an arena/cache (not raw `malloc`),
* or cached by unit name so repeated `with` doesn’t re-read and re-leak.

Right now it’s “accidental permanent heap growth”.

### B) Importing `.ali` “X lines” builds fake ASTs + fake types

`read_ada_library_interface()` (10192–10309) parses `.ali` lines and creates:

* for `pc==1`: a **variable object decl** node `N_OD` with type guessed from `I64/F64/PTR` substrings (10265–10283)
* else: a **procedure/function** node `N_PD/N_FD` with a spec node `N_FS`, but parameters are all named `"p"` and types are omitted (10286–10301)

This is a giant semantic shortcut that will break overload/type checking later (and it already forces “permissive” hacks elsewhere). If you want C interop stubs, you need a real interface model (types, calling conv, etc.), not “count params from tokens and pretend”.

### C) `pks2()` recursively parses `with` dependencies with no cycle guard

`pks2()` (10333–10371) does:

* parse unit
* for each `with_clause`, call `pks2(...)` again (10341–10345)

No visited-set → a cyclic with graph can recurse forever. GNAT has explicit unit dependency management; this is “hope the world is acyclic”.

### D) “use” clause visibility is implemented by mutating symbol visibility bits globally

`symbol_manager_use_clauses()` (10380–10490) walks packages referenced by `use` and does:

* `sym->visibility |= 2; sv(&uv, sym);`

This is the same blunt approach you used earlier (`symbol_find_use` etc.)—it’s not a scope stack, it’s global mutation that you later try to undo by tracking `uv`.

It will mis-handle nested scopes, shadowing, and “use inside a block” semantics unless you are extremely disciplined about pushing/popping exactly the right set (and the codebase isn’t).

### E) Static storage rules are violated for string literals

In `generate_expression`:

* `N_STR` allocates the bytes with `alloca` and returns a fat pointer to that stack memory (11775–11800).
* In `N_ID` when the symbol is a constant string, it does the same “inline alloca literal” (11830–11853).

That’s semantically wrong if the string escapes the expression (assigned to a longer-lived object, returned, captured, etc.). GNAT puts string literals into a global constant (or at least static storage), not on the stack.

### F) “pointer validity” hacks (`(uintptr_t)ptr > 4096`) are now baked into codegen too

You now use the same “is this pointer plausible” hack in:

* `is_truly_nested()` (11163–11166)
* `generate_expression(N_ID)` when deciding package-level/global access (11868–11875 and beyond)

This is not a defensible invariant. Either `parent` is a valid `Symbol*` or it isn’t; codegen shouldn’t be guessing by pointer value.

---

# 2) Duplicated code patterns worth collapsing

### A) Static-link chain traversal is duplicated 3 times in `generate_expression(N_ID)`

You implement “walk N hops up the static link chain, then index by elaboration_level, load” in:

* SK_FUNCTION non-0-arg path (11972–12002)
* non-function variable path (12006–12041)
* and more continues past 12040

This should be **one helper**:

* `ptr resolve_static_link(Symbol *s)` returning the “frame pointer” for `s->level`
* `ptr frame_slot(ptr frame, unsigned elab_level)` returning slot address

Right now it’s copy/paste, so any fix to the frame layout must be repeated in multiple branches.

### B) Aggregate element stores repeat “trunc-if-narrow” logic everywhere

`generate_aggregate()` repeats:

* compute element pointer
* if elem_size < 8: trunc then store
* else store i64

You already created `emit_typed_store()` (11243–11262), but you *don’t use it* for these array aggregate stores (11526+). That’s duplicated logic + missed opportunity to centralize type width rules.

### C) Symbol mangling is partially duplicated outside `encode_symbol_name()`

In global variable/function access you sometimes build `nb` manually from parent name + `_S%dE%d__` + symbol name (11883–11891) and sometimes call `encode_symbol_name()` (11900, 11917, 11960). That’s two naming systems, which guarantees collisions or “can’t find symbol” bugs later.

---

# 3) Sketchy hacks / correctness holes

### A) `emit_typed_store()` is wrong for non-integer storage widths

For `v.k != INTEGER`, it emits:

```c
store <value_llvm_type_string(v.k)> %t, ptr ptr_str
```

It ignores `target_type`’s actual storage type. So:

* storing a `float` value into an Ada `Float` (32-bit) would still store `double`
* storing into smaller ints via non-integer paths is unsupported
* unsigned vs signed extension is ignored entirely

Same for `emit_typed_load()`: it always `sext` narrow ints (11278–11283), even for unsigned/enums where you likely want `zext`.

### B) Array aggregates force everything through integer

In `generate_aggregate()` array path, every element is:

```c
Value v = value_cast(..., VALUE_KIND_INTEGER);
```

So arrays of float/pointer/etc are not handled correctly. At best you’ll get lossy conversions; at worst IR type mismatches.

### C) Record aggregates also force integer first, then patch up

Record aggregate stores do:

* cast expression to integer
* if target field is float: `sitofp` (integer→float)
* if target field is pointer: `inttoptr`

This is not “store the value”; it’s “reinterpret through integer”. It breaks legitimate float aggregates and pointer aggregates badly.

### D) “Fixed lower bound optimization” ignores constant lower bound 0

`get_fat_pointer_bounds()` treats FLB as `low_bound != 0 && low_bound != INT64_MIN` (11439–11441). A constant lower bound of 0 is still fixed and still optimizable, but you forbid it.

---

# 4) What goes against GNAT LLVM (specifically in this chunk)

You added GNAT-LLVM comments and some good ideas (widen to i64 for computation, store/load with trunc/extend), but the implementation still diverges in critical ways:

1. **Value carries real type**: you *introduce* `Value { id, kind, Type_Info* }` (10497–10502) but then much code still relies on `Value_Kind` only (`value_llvm_type_string`, integer-for-everything aggregates, etc.). GNAT LLVM’s whole point is *don’t lose the Ada type*.

2. **Static storage for literals**: GNAT uses global constants; you use `alloca` for strings.

3. **Invariants over heuristics**: GNAT doesn’t do `(uintptr_t)ptr > 4096` to guess validity.

4. **Unit management**: GNAT has an explicit unit graph; your `pks2` recursion + `lookup_path` side effects is “parse on demand in the middle of semantic setup”.

---

# 5) Unnecessary / low-value functions in this slice

### `value_llvm_type_string()` and `value_computation_type()`

They’re explicitly marked legacy/deprecated (10700–10728), and they *are* harmful because they keep creeping into new code (you still use them in function call emission around 11902, 11919, 11962).

If you keep them, you need a hard rule: **only allowed when `v.t == NULL`**, otherwise they should be compile-time illegal (assert).

### `find_label()` (10586–10592)

Looks unused once `get_or_create_label_basic_block()` exists. If you never call `find_label`, delete it.

### `normalize_name()` (10558–10561)

If metadata IDs are already `md++`, it’s fine, but this wrapper doesn’t add safety. It’s borderline “function bloat”.

---

# 6) Function-by-function ratings (1–10)

### Elaboration / unit loading

* `elaborate_compilation` (10136–10177): **6/10**

  * Simple, but only handles a few node kinds; “elaboration_level < 0 ⇒ set to eo” is too ad-hoc.
* `read_file` (10178–10191): **3/10**

  * No error checks, no size, ignores short reads, encourages leaks.
* `read_ada_library_interface` (10192–10309): **4/10**

  * Clever parsing, but produces fake types/decls; fragile string munging.
* `lookup_path` (10310–10331): **4/10**

  * Side-effect reads `.ali`, returns heap string with no ownership model.
* `pks2` (10333–10371): **4/10**

  * Works for happy-path, but no cycle detection; mixes phases heavily.
* `parse_package_specification` (10372–10379): **6/10**

  * The “skip placeholder packages” guard is good.
* `symbol_manager_use_clauses` (10380–10490): **5/10**

  * Does what it says, but global visibility mutation is semantically brittle.

### Codegen scaffolding

* `emit_all_metadata` (10566–10585): **7/10**

  * Reasonable; guarded access to `lopt`.
* `get_or_create_label_basic_block` (10601–10611): **6/10**

  * Works, but heap allocation per label and no free; fine for compiler lifetime.
* `add_declaration` (10617–10628): **6/10**

  * Dedup works, but copies with malloc, no arena; ok.

### Type mapping / value model

* `computation_type_for` (10674–10698): **7/10**
* `value_llvm_type_string` (10707–10718): **3/10**

  * Known-wrong and still used.
* `ada_to_c_type_string` (10736–10778): **7/10**

  * Good direction, but relies on the project’s shaky “size units” elsewhere.
* `token_kind_to_value_kind` (10787–10797): **6/10**
* `type_hash` (10806–10829): **4/10**

  * Too collision-prone to distinguish overloads robustly.
* `encode_symbol_name` (10830–10953): **6/10**

  * Useful, but mixes multiple hashing schemes and still depends on questionable parent validity.

### Nested detection / local package procedure extraction

* `has_nested_function*` (10954–10993): **6/10**

  * Practical, but misses some constructs and encodes semantics as “node kind list”.
* `generate_local_package_procedures_in_*` (10994–11043): **5/10**

  * A codegen-phase patch for earlier phase-mixing.

### IR emission helpers

* `generate_block_frame` (11046–11055): **4/10**

  * Scans entire symbol table, ignores scope; frame size can be wrong/huge.
* `generate_index_constraint_check` (11069–11086): **7/10**
* `value_cast` (11103–11137): **7/10**
* `elem_size_and_type` (11175–11236): **7/10**

  * Good “no fallbacks” stance; still depends on consistent type model.
* `emit_typed_store` (11243–11262): **4/10**

  * Wrong for float/ptr storage widths; ignores unsigned extension rules.
* `emit_typed_load` (11269–11285): **5/10**

  * Always `sext` narrow ints; should be signedness-aware.
* Range checks: `generate_float_range_check` **6/10**, `generate_discrete_range_check` **7/10**, `generate_array_bounds_check` **5/10**

### Fat pointers / aggregates / early expression gen

* `generate_fat_pointer`, `get_fat_pointer_*` (11403–11467): **6/10**

  * FLB rule is wrong for low bound 0.
* `generate_aggregate` (11487–11714): **3/10**

  * Hardwired integer path breaks floats/pointers and relies on buggy normalizers.
* `symbol_body` / `symbol_spec` (11715–11741): **7/10**
* `get_attribute_name` (11742–11754): **6/10**

  * Static buffer = not re-entrant/thread-safe.
* `generate_expression` (11755–12000 portion): **4/10**

  * The static-link/global resolution logic is large, duplicated, and still relies on pointer validity hacks; string literal allocation is semantically wrong.

---

# 7) Competing implementations (what to change conceptually)

### 1) Make string literals global constants (GNAT-style)

Emit a private global `@.str.N = constant [len x i8] ...` and return a fat pointer pointing at it. No `alloca`, no lifetime bugs.

### 2) Replace “use clause toggles visibility bits” with a use-stack

Instead of mutating symbols:

* maintain `Use_Clause` stacks per scope (`enter_scope/leave_scope`),
* lookup consults these stacks dynamically.

This removes the need for `uv` “undo vectors” and prevents scope leaks.

### 3) Unify static-link access into one helper

Write one function:

* `ptr get_frame_for_level(int target_level)`
* `ptr load_slot(ptr frame, unsigned elab_level)`
  Then use it for variables and nested subprogram references. You’ll delete ~80–120 lines of duplication.

### 4) Stop forcing aggregates through integer

Use `Value`’s `Type_Info*` and `emit_typed_store()` (after fixing it) for each element/field. That gives correct float/pointer behavior and matches the GNAT LLVM “typed values” philosophy you comment about.

## Part 8 — lines 12001–14000 (read & analyzed)

This slice is basically **the rest of `generate_expression()`**: finishing `N_ID` cases, then big coverage for `N_BIN`, `N_UN`, `N_IX`, `N_SL`, `N_SEL`, `N_AT`, `N_QL`, and the start of `N_CL` (call emission).

This is *also* where several ABI / representation inconsistencies become undeniable.

---

# 1) Highest-impact correctness hazards

### A) **Static-link (nested call) ABI is inconsistent**

You pass the static link in **different positions** depending on node kind:

* In `N_IX` “actually a function call” path: you print static link **first**:

  ```c
  if (needs_slnk) fprintf(o, "ptr %%__slnk");
  // then ", <args...>"
  ```
* In `N_CL` normal call path: you print static link **last**:

  ```c
  fprintf(o, "call ...(");
  // args...
  if (is_truly_nested(s)) { if(args>0) ", "; ptr %__frame or %__slnk }
  ```

That means the same Ada call can be lowered with two different LLVM call signatures depending on whether it came through `N_IX` vs `N_CL` (and you still rewrite between these node kinds elsewhere). That is a **hard miscompile** waiting to happen.

**Fix:** pick one convention and enforce it everywhere (ideally “static link first” like GNAT-LLVM does for trampoline-ish nested subprograms).

---

### B) **`'ACCESS` is wrong (often becomes “inttoptr(value)”)**

In `N_AT`:

```c
else if (… "ACCESS") {
  Value p = generate_expression(prefix);
  r = value_cast(p, VALUE_KIND_POINTER);
}
```

But `generate_expression(N_ID)` for a scalar variable returns the **loaded value** (i64), not its address. So `X'Access` becomes `inttoptr i64 <X_value>` which is nonsense.

This is not a small spec gap — it breaks access-to-object semantics across the compiler.

**Fix:** implement “address-of” properly:

* either add an `generate_lvalue_address()` routine,
* or make `generate_expression` optionally return an lvalue pointer (GNAT-LLVM has a clear separation).

---

### C) **Slices are broken for fat pointers + non-1-based/non-0-based arrays**

`N_SL` does:

* `Value p = generate_expression(prefix);`
* `sp = getelementptr elem_type, ptr %t<p.id>, i64 lo.id` (no low-bound adjustment, no fat-pointer data extraction)
* memcpy into stack buffer
* return fat pointer pointing at stack buffer

Problems:

1. If `prefix` is a fat pointer array (`low_bound==0 && high_bound<=0` convention), `p.id` is the fat pointer struct, not the data pointer.
2. No bounds check.
3. No subtraction of array `low_bound`.
4. Result points to stack (`alloca i8, i64 …`) → lifetime bugs if slice escapes.

This is a cluster of “works for a demo” behavior, not compiler correctness.

---

### D) **Packed-record bit extraction has UB and conflates fields**

Packed-field path in `N_SEL`:

```c
uint64_t mk = (1ULL << c->component_decl.bit_offset) - 1;
```

If `bit_offset == 64`, shift is UB. If `bit_offset == 0`, mask is 0. Also: you’re using `bit_offset` as *width* here, but elsewhere it’s been used as other meanings (rep clause, record layout logic earlier).

Also the load is always `load i64` from the containing 8-byte chunk, but you don’t handle:

* fields spanning 64-bit boundaries,
* signed fields,
* endian/layout rules beyond your ad-hoc offset.

So even “simple packed records” are shaky.

---

### E) **String concatenation allocates bytes but ignores element size and bounds rules**

In `N_BIN` for `T_AM` when pointer-ish:

* compute lengths as `(hi-lo+1)` in elements, but then treat them as **bytes** for malloc/memcpy.
* always sets new bounds to `1..len-1`.

So this only “kind of works” for `Character` arrays that are 1-byte and intended as NUL-terminated, and even then you’re forcing 1-based bounds.

---

### F) **Signed/unsigned handling is still wrong everywhere**

Across this slice you frequently:

* `sext` narrow loads unconditionally
* use signed compares for bounds (`slt/sle/sge`)
* treat all discrete as signed

For Ada modular types / unsigned integer types, that’s semantically wrong.

---

# 2) Duplicated code patterns (clear bloat + bug multipliers)

### A) Membership (`IN`) and “NOT IN” are copy/pasted 2×

`T_IN` and `T_NOT` cases both:

* peel `N_CHK`
* handle `N_RN` range
* handle `N_ID` treated as type with bounds
* build two compares + `and` + zext

They differ only in final `xor with 1`.

**Collapse:** one helper:

* `emit_membership(x, lo, hi)` returning i1/i64, and let caller negate.

---

### B) Repeated “load bounds from type symbol” block

The `rr->k == N_ID` membership path appears twice (IN/NOT). Same for:

* `tlo/thi` temp emission
* compare sequence

Again: extract helper.

---

### C) N_SEL “resolve prefix pointer” has 3 separate implementations

In `N_SEL` you handle prefix being:

1. `N_ID` variable (global vs slnk vs local),
2. package prefix + component symbol,
3. fallback generate_expression

Each branch repeats:

* parent/global name mangling
* slnk access logic
* bitcast-vs-load decisions for records

This should be:

* `Value gen_prefix_addr_or_value(prefix_symbol, desired_mode)` returning pointer to storage or pointer to record blob consistently.

Right now any tweak requires editing 3 copies.

---

### D) Call argument truncation logic duplicated (and inconsistent)

* `N_IX` has `arg_types/arg_regs` trunc path.
* `N_CL` has **two** versions: `c_external` and “Ada non-external”, both implementing basically the same trunc-to-declared-type logic.

Centralize “convert computation-value to declared param type” into one helper; otherwise you’ll keep fixing one path and forgetting the others.

---

# 3) Sketchy hacks designed to ignore incorrectness

### A) Pointer validity heuristic keeps infecting codegen

You still do `(uintptr_t)ptr > 4096` checks inside codegen paths (global/package detection). That is not a compiler invariant; it’s a bug band-aid. When it fails, you miscompile silently.

---

### B) Qualified expression (`N_QL`) discards type

`N_QL` just casts the aggregate to `r.k` — but `r.k` is the default initialized kind (integer) unless something set it earlier. In practice this makes qualified expressions frequently “become integers”.

Qualified expressions are *type assertions* in Ada — they must preserve the qualified subtype, not discard it.

---

# 4) What directly conflicts with GNAT-LLVM approach (in this slice)

1. **No LValue/RValue separation**: GNAT-LLVM cleanly distinguishes addresses vs values. Here you try to “guess” and cast.
2. **ABI stability**: GNAT-LLVM keeps nested-call conventions consistent. Here static link position changes across node kinds.
3. **Composite typing**: GNAT-LLVM doesn’t force aggregates/records through i64 as an intermediate. Here packed/unpacked field loads and aggregates are often “integer-first then reinterpret”.

---

# 5) Function quality ratings (this slice)

### `generate_expression` (portion 12001–14000)

**3/10**

* **Good:** you’re trying to implement real Ada semantics (short-circuit, attributes, packed fields, out params) and you added several GNAT-LLVM-inspired “truncate args / sext returns” patches.
* **Bad:** it’s still internally inconsistent on:

  * call ABI (static link placement),
  * lvalue/address semantics (`'Access`, slicing, record field addressing),
  * array representation (fat pointer vs normal vs NUL-terminated),
  * signedness,
  * and it contains multiple duplicated sub-implementations that drift.

Most of the “works sometimes” behavior comes from those inconsistencies, not from missing features.

---

# 6) Competing implementation that would simplify *everything* here

### A) Introduce **two** codegen entry points:

* `Value gen_rvalue(expr)` → returns computed value
* `Value gen_lvalue(expr)` → returns address (ptr) to storage

Then:

* assignment target, `'Access`, `in out` args, record fields, indexing → use `gen_lvalue`
* arithmetic/compare → use `gen_rvalue`

This single architectural shift fixes:

* `'Access` correctness
* slice implementation (compute base address + bounds properly)
* record selection (field address vs field load)
* out-parameter passing (always address)

### B) Standardize nested call ABI

Define:

* “if nested: static link is **first** argument”
  and enforce for all call emission paths (`N_CL`, `N_IX`, selected calls, external stubs if applicable).

### C) Make array representation explicit

Right now “fat pointer” is encoded by `low_bound==0 && high_bound<=0`, but code doesn’t consistently honor it.
Have explicit `TYPE_FAT_ARRAY` (or a flag) so every site can branch on a real invariant.

## Part 9 — `ada83.c` (lines ~14001–16000)

This slice finishes the remaining call-handling in `generate_expression` (notably `N_SEL` calls), then introduces **`generate_statement_sequence`** and a big chunk of statement lowering: assignment, `if`, `case`, loops, `exit`, `goto`, `return`, `raise`, call-as-statement, “code statement” calls (`N_CLT`), and the beginning of block (`N_BL`) elaboration/exception setup + initialized object decls.

---

# 1) High-impact correctness problems (not style)

### 1. `case` statement is wrong for alternatives with multiple choices

In `case N_CS`, for each alternative you iterate `choices.items`, but **each choice emits a terminating conditional branch to either `la` or “next alternative”**. That means **only the first choice in each alternative is ever tested**; later choices are dead code because the first branch leaves the block.

This breaks common Ada like:

```ada
when 1 | 2 | 3 => ...
```

Only `1` is actually checked.

**GNAT-LLVM behavior:** it effectively ORs the tests for all choices in an alternative (or uses a switch/range table), then branches to that alternative if *any* match.

**Fix direction:** build per-choice test blocks that fall through to the *next choice of the same alternative*, not the next alternative. Only after all choices fail do you go to the next alternative.

---

### 2. Reverse loops don’t implement reverse semantics

`N_LP` sets metadata with `n->loop_stmt.is_reverse ? 7 : 0` but **the actual induction update is always `+ 1`** and the bound check is always `icmp sle cv, hv`.

So a `for I in reverse ...` will run the wrong direction (or not terminate correctly).

**Fix direction:** for reverse loops:

* init = high
* compare `icmp sge cv, low`
* step `cv - 1`

Metadata alone doesn’t fix semantics.

---

### 3. Constrained array detection is inconsistent and often wrong (low bound 0 bug)

Several places treat “constrained array” as `low_bound != 0`. That’s not a valid discriminator: Ada arrays can be constrained with low bound 0.

Examples:

* assignment: `is_array_assign = st && st->k==TYPE_ARRAY && v.k==PTR && st->low_bound != 0 && st->high_bound > 0;`
* return: `vt->low_bound != 0 && vt->high_bound > 0 ...`

So a constrained array `(0 .. 10)` is mishandled and may fall into fat-pointer paths or scalar store paths.

**Fix direction:** use the sentinel rule you already used elsewhere: unconstrained = `(low==0 && high<=0)` (your code uses that). Constrained should be `!(low==0 && high<=0)` **and** `high >= low`.

---

### 4. Packed record field store assumes it fits in one `i64`

Packed assignment path loads/stores `i64` at `offset/8`, masks bits, writes back one word.

That fails for:

* packed fields spanning >64 bits
* packed record larger than 64 bits
* fields not contained within a single 64-bit chunk (crossing word boundary)

**GNAT-LLVM:** does correct bitfield extraction/insertion across appropriate storage units.

**Fix direction:** operate on the *declared storage unit(s)* (likely bytes) and handle cross-boundary updates, or lower packed records to an explicit integer type sized to the record when possible.

---

### 5. Element-size heuristics are incomplete (memcpy sizing can be wrong)

You compute element sizes differently in different places, and often only handle a subset:

* assignment array memcpy uses: integer=8, char/bool=1, else=4 (floats become 4 even if `double`)
* return array memcpy uses: integer=8 else=4
* block init memcpy uses a more detailed 1/2/4/8 heuristic

This can produce incorrect copy sizes, especially for floats, nested arrays, records, etc.

**Fix direction:** centralize “type size in bytes” calculation using the type system (including float sizes, records, nested arrays, alignment).

---

# 2) Big duplicated patterns (should be factored)

These show up repeatedly in this slice and earlier:

### A. Name mangling / global symbol name building

The “parent package prefix + `_S%dE%d__` + uppercased name” block repeats many times.

**Refactor:** one helper that returns the correct global name string (and one for local `%v...` vs `%lnk...`).

---

### B. Static-link / frame-chain walking

The “level_diff, hop via `getelementptr ptr` then `load ptr` repeatedly” pattern repeats in array assignment, parameter passing, etc.

**Refactor:** a helper like:

* `ptr emit_static_link_hop(Symbol *s)` (or `emit_frame_ptr_for_level(level)`).

---

### C. OUT/in out parameter handling

The logic “if mode & 2 then pass pointer; after call load/store back; handle narrow ints” appears in:

* expression call path (`N_CL`)
* statement call path (`N_CLT` and also selected component procedure calls)

**Refactor:** one call builder that:

1. classifies args (by mode, constrained/unconstrained array handling),
2. emits coerces/trunc/sext,
3. emits call + static link consistently,
4. emits write-back for `out/in out`.

---

### D. Constrained array copy via `memcpy`

Implemented in at least three different ways with different size rules.

**Refactor:** `emit_array_copy(dst_ptr, src_ptr, Type_Info *array_type)`.

---

# 3) “Sketchy hack” style items worth fixing

### 1) `(uintptr_t) s->parent > 4096` checks

This appears in global-name construction. That’s a brittle “is pointer plausible?” trick.

**Fix:** store an explicit flag/validity field, or ensure `parent` is always either null or a valid allocated symbol.

---

### 2) Using a temp register id to name an alloca (`%v.__for_hi_%d`)

It works, but it’s conceptually messy and ties naming to register allocation order.

**Fix:** use a separate monotonically increasing “unique id for named allocas” counter.

---

### 3) The `case` lowering uses `goto cs_sw_done`

Not inherently wrong, but given the *real* bug above, it’s a red flag that control-flow structure isn’t being built carefully.

---

# 4) Things that go against “what GNAT LLVM does” (conceptually)

* **Case alternatives**: GNAT preserves correct multi-choice semantics; yours currently doesn’t.
* **Reverse loops**: GNAT implements reverse via induction direction, not metadata.
* **Packed records**: GNAT lowers packed fields robustly; yours is a one-word bit-twiddle.
* **Array sizes/copies**: GNAT uses type layout; yours uses inconsistent heuristics.

---

# 5) Competing implementations (same concept, multiple inconsistent variants)

### Calls

You now have at least these “call emitters”:

* `generate_expression` → `N_CL` direct name
* `generate_expression` → `N_CL` selected component (`N_SEL`) + TEXT_IO fast paths
* `generate_statement_sequence` → `N_CL` (delegates to expression)
* `generate_statement_sequence` → `N_CLT` (multiple branches: body present, external, SK_PROCEDURE/SK_FUNCTION, plus `N_SEL` variant)
* TEXT_IO in expression uses `@stdout` + suffix `.i64/.f64`; statement path uses `ptr null` + `_i64/_f64`

These differences are likely to cause drift/bugs.

**Recommendation:** one unified “emit_call” path with parameters:

* callee symbol + encoding info
* argument nodes
* “result needed?” flag
* “statement vs expression” only affects whether you capture the result

---

# 6) Function breakdown + rating (1–10)

### `generate_statement_sequence(Code_Generator*, Syntax_Node*)` — **4/10**

**What it does well**

* Covers many Ada statements in one place.
* Uses `emit_typed_store` in several paths (good direction for truncation correctness).
* Starts to address constrained array copies with `memcpy`.

**Why it scores low**

* Major semantic bugs: **case multi-choice**, **reverse loops**.
* Array constrained/unconstrained detection is inconsistent (`low_bound != 0`).
* Multiple duplicated call/copy/out-param implementations drifting apart.
* Packed record handling is structurally unsafe beyond trivial cases.

## Part 10 — lines 16001–17550 (read & analyzed)

This chunk closes out **block/exception lowering**, then adds a **label/goto discovery + label emission subsystem**, then a *large* rework of **`generate_declaration`** (alloca strategy incl. fat pointers and runtime-sized arrays), plus the start of **package-body elaboration functions** + **global_ctors**, and the beginning of the runtime prelude emission.

---

# 1) High-impact correctness hazards

### A) Exception choice matching is “string compare current exception name”

Inside block exception handlers, each exception choice is checked by:

* `load ptr @__ex_cur` then `strcmp(%t, @.ex.<NAME>)` and branch on `eq 0` (16025–16037).

Problems:

* This assumes exceptions are represented solely by **C strings**, not by identity/type.
* You uppercase the exception name into `eb[256]` (16028–16032) but **no namespace / package qualification** is applied. Collisions between same-named exceptions in different packages become indistinguishable.
* This doesn’t match GNAT-like exception identity semantics.

### B) Structural union/field misuse in statement nodes (again)

You use `N_DL` (delay) but read `n->exit_stmt.condition` (16064–16068).
You use `N_DL` in a `select` path and then iterate `st->exception_handler.statements` (!!) (16096–16102).

That’s not “clever reuse”; it’s **reading the wrong union arm**. If the node layout isn’t exactly aligned, this becomes memory garbage; even if it “works” today, it’s brittle.

This is the same class of bug as the earlier rep-clause `memcpy` into unrelated storage.

### C) Label-collection stores `String_Slice*` inside a `Node_Vector` by casting to `Syntax_Node*`

In `emit_labels_block_recursive`, you do:

```c
String_Slice *lb = malloc(sizeof(String_Slice));
*lb = s->goto_stmt.label;
nv(lbs, (Syntax_Node *) lb);
```

(16199–16202)

So a `Node_Vector` meant for `Syntax_Node*` is now holding `String_Slice*` pretending to be nodes.

This is dangerous:

* any future code that assumes `lbs.data[i]` is a node will blow up,
* it repeats the project-wide anti-pattern: “stuff pointer into unrelated type and hope.”

### D) `generate_declaration(N_OD)` introduces a new fat-pointer layout variant, but nothing is consistently wired to read it

Runtime-sized array handling (16337–16433) builds a fat pointer `{ptr,ptr}` and then stores:

* data pointer → field 0,
* bounds pointer → field 1,

But the bounds pointer is **either**:

* `alloca i64` storing only the high bound (FLB optimization) (16391–16399), **or**
* `alloca {i64,i64}` storing low+high (16401–16414).

Earlier, your fat-pointer helpers/heuristics were already inconsistent (low_bound==0/high<=0 sentinel, “FLB means low_bound != 0” etc). Here you introduce a *second* runtime bounds representation without a clear discriminator other than “lo is constant int”.

So now any consumer must somehow know whether bounds points at `i64` or `{i64,i64}`. I don’t see a reliable tag/invariant for that in the type system—so this is a **latent miscompile**.

### E) Constrained-array detection uses `low_bound != 0` (wrong)

During object initialization, you special-case “constrained array, copy via memcpy” using:

```c
if (at && at->k==TYPE_ARRAY && at->low_bound != 0 && at->high_bound >= at->low_bound)
```

(16943–16946)

That’s wrong for constrained arrays with lower bound 0 (perfectly valid Ada). Same bug shows up elsewhere (you already had it in Part 9).

This will make `(0 .. N)` arrays take the **wrong assignment/initialization path**.

### F) OUT/IN OUT parameters are *not* modeled as by-reference in the callee

In procedure/function body generation (`N_PB` and `N_FB`), for `mode & 2` you do:

* function signature: `ptr %p.name` (16731–16735, 17109–17113)
* then you **load the value from the pointer** and store into a local alloca of the value type (16791–16818 and 16969+).

That destroys by-reference semantics unless you later write-back to `%p.name` on every assignment or at return. I don’t see that here; and your normal variable stores will hit the local `%v.*` / `%lnk.*`, not the original parameter pointer.

So `out`/`in out` inside the callee likely becomes “local copy only” → caller never sees updates.

GNAT-LLVM keeps OUT/IN OUT as an addressable object (lvalue) backed by the incoming pointer.

### G) Frame-slot population relies on shaky symbol identity

For `N_PB` you store locals into the frame only when:

* `s->parent == n->symbol` and `s->scope == parent.scope+1` and `s->level == lv` (16859–16861).

But in the `N_FB` variant you explicitly comment that “parent pointer may not be set correctly” and switch to weaker matching (17235–17240). That contradiction is telling: your symbol invariants aren’t stable enough to support this representation cleanly.

### H) Package elaboration copies locals to globals using pointer-plausibility hacks + bad sizing

Inside package-body elaboration:

* you require `(uintptr_t)s->parent > 4096` (17464–17466)
* you compute array element sizes with a heuristic that sets integer/enum = 4 bytes (17491–17495), but elsewhere you treat “integer computation” as i64.
* for non-arrays you load/store using `value_llvm_type_string(k)` (17505–17511), which tends to be i64 for integers; that is wrong for character/boolean and other narrow types unless carefully truncated.

So this “copy locals → globals” block is both heuristic and type-unsafe.

### I) `emit_global_ctors` silently drops elaboration beyond 64

Elab functions collected only if `elab_count < 64` (17520–17525). Past that, packages won’t be elaborated and nothing warns.

That’s a correctness failure that becomes “random program breaks when it grows.”

### J) `__ada_i64str_to_cstr` hardcodes 1-based indexing adjustment

Runtime helper subtracts 1 (`adj=sub i64 %idx, 1`) (17571–17572). That assumes arrays start at 1. It will be wrong for strings with other lower bounds (including 0), which Ada allows.

---

# 2) Duplicated / bloaty patterns (bug multipliers)

### A) Same body scaffolding duplicated between `N_PB` and `N_FB`

These blocks are near-copy-paste:

* generic body skip scan (16668–16684 and 17045–17060)
* local package procedure generation + label scanning
* name mangling storage
* setjmp wrapper around statements (16991–17024 and 17342–17375)
* discriminant default checks for initialized records (16909–16940 and 17286–17317)

This should be one “emit_subprogram_body_common(…, returns_value?)” routine with hooks.

### B) Two different “store locals into frame” implementations

`N_PB` uses strict parent-based matching (16859–16861).
`N_FB` uses a looser scope/level match because parent pointer may be wrong (17235–17240).
That guarantees drift and bugs (and it’s already admitting the invariant is broken).

### C) Label discovery does O(n²) string compares and mallocs

`emit_labels_block_recursive` does a linear “already present?” scan for every goto/label (16187–16202 etc.). This is fine for tiny programs, but quickly becomes quadratic. Also: `malloc` per label, never freed.

A simple hash-set of labels (even fixed-size open addressing) would be smaller and faster.

---

# 3) Unnecessary / misleading functions (in this slice)

### `has_basic_label()` (16270–16304)

Despite the name, it doesn’t “check for labels.” It recursively walks statements and **generates declarations** for nested procedure/function bodies in blocks (16277–16285). That’s a side-effecting codegen pass hidden behind a misleading name.

This is exactly the kind of “phase mixing” that makes the compiler hard to reason about.

---

# 4) What goes against GNAT-LLVM (specifically here)

1. **OUT/IN OUT modeling**: GNAT keeps them as addresses; here you eagerly load into locals, breaking aliasing and copy-back requirements.
2. **Exception identity**: GNAT doesn’t dispatch by comparing exception-name strings.
3. **Elaboration**: GNAT has a proper unit graph and doesn’t silently cap elaboration to 64 ctors.
4. **Array bounds**: GNAT never treats “low_bound != 0” as a constrainedness test, and doesn’t hardcode 1-based runtime helpers.

---

# 5) Function-by-function ratings (1–10)

* Block/handler tail in `generate_statement_sequence` (16001–16116): **4/10**
  Works mechanically, but string-based exception identity + union-field misuse are serious.

* `is_runtime_type` (16117–16137): **5/10**
  Functional, but a brittle ever-growing whitelist (should be table-driven or use a prefix convention).

* `has_label_block` (16138–16166): **7/10**
  Reasonable recursion; does what it says.

* `emit_labels_block` (16168–16180): **6/10**
  Simple, but depends on the unsafe storage trick from the recursive collector.

* `emit_labels_block_recursive` (16181–16268): **4/10**
  Works for now, but stores the wrong pointer type in `Node_Vector`, does O(n²), and leaks.

* `has_basic_label` (16270–16304): **2/10**
  Misnamed + side-effecting codegen pass = maintainability hazard.

* `generate_declaration` (16305–17416): **4/10**
  Big progress on allocating real array storage and adding fat pointers, but:

  * introduces incompatible fat-bound representations,
  * still uses incorrect constrained-array tests (`low_bound != 0`),
  * and OUT/IN OUT parameter handling is broken by design.

* `generate_expression_llvm` (17417–17527): **6/10**
  The “generate bodies + optional elab function” structure is sensible, but the variable copy-to-globals is heuristic and unsafe.

* `emit_global_ctors` (17528–17543): **6/10**
  Mechanically fine; the real issue is the upstream hard cap of 64.

* Start of `generate_runtime_type` (17544–17575 shown): **5/10**
  Useful runtime glue, but bakes in 1-based assumptions.

---

# 6) Concrete competing implementations (high leverage)

### A) Fix OUT/IN OUT once: introduce lvalue vs rvalue in codegen

Add a helper to represent “addressable object”:

* `Value gen_lvalue(expr)` → returns `ptr` to storage
* `Value gen_rvalue(expr)` → loads/computes a value

Then for `mode & 2` parameters:

* store `%p.name` (the incoming pointer) into a local pointer slot, and always dereference it on reads/writes.

This matches GNAT-LLVM’s discipline and will delete a lot of ad-hoc logic elsewhere.

### B) Make fat-pointer bounds representation explicit

Pick exactly one:

* always `{i64 low, i64 high}`, OR
* `{i64 high}` + a separate tagged flag stored alongside (not inferred from “lo is constant”).

Do not rely on hidden conventions; every reader must know the exact layout.

### C) Replace label collector with a real set

Instead of `Node_Vector` + casts:

* define a small `Label_Set { String_Slice* items; uint32_t count; }` with hashing
* store `String_Slice` directly, not via `malloc` per label

This removes the unsafe type pun and the quadratic scan.

## Part 11 — lines 17551–18746 (read & analyzed)

This last chunk is: **runtime prelude emission**, **.ali writer + forward decls**, a heavy **on-demand “compile with’ed unit” path**, and `main()`.

---

# 1) High-impact correctness bugs

### A) `__ada_finalize` allocates **16 bytes** but uses **3 pointers** (needs 24)

Runtime emitter prints:

* `__ada_finalize(ptr obj, ptr fn)` does `malloc(i64 16)` and then writes indices 0,1,2 with `getelementptr ptr, ptr %1, i64 N`.
* That is **3 ptr slots**. On 64-bit, that’s **24 bytes**.

So this is immediate heap OOB write + list corruption. `__ada_finalize_all` then walks/free’s the corrupted list.

**Fix:** allocate `24` (or `3*sizeof(ptr)` via target datalayout), and treat node as `{ptr, ptr, ptr}` instead of “ptr array”.

---

### B) Calendar runtime uses **struct tm as “i32 array”** + writes into `[64 x i8]`

Functions like `__ada_year/month/day/seconds` do `getelementptr i32, ptr %tm, i32 K` on the result of `localtime()`. `__ada_time_of` builds a `[64 x i8]`, then writes fields via `getelementptr i32, ptr %tm, …`, and passes it to `mktime()`.

This is not a valid LLVM representation of `struct tm` and is not portable even across libc/targets. It’s basically “hope the ABI matches”.

GNAT doesn’t do this; it relies on a real Ada runtime interface.

---

### C) More 1-based assumptions baked into runtime helpers

`__ada_i64str_to_cstr` does `adj = idx - 1`. That assumes the Ada “string” is 1-indexed and stored as `i64` elements. This is not generally true (Ada strings can have any lower bound).

You also bake 1-based bounds into `__ada_image_int` / `__ada_image_enum` by always allocating bounds `{1, len}` and storing chars at indices `1..len`.

---

### D) `.ali` timestamping is effectively broken for `W` lines

`write_ada_library_interface()` lowers the unit name and calls `find_type_symbol(pf)` where `pf` is just the **lowercased unit name with no extension/path**. On most systems this will `stat()` fail and write `0` timestamps, meaning any “rebuild-if-changed” scheme can’t work.

---

### E) Writer/reader type token mismatch risk (`value_llvm_type_string`)

Your writer prints parameter/return types using `value_llvm_type_string()` (likely `"i64"`, `"double"`, `"ptr"`). Earlier your `.ali` reader logic was based on detecting `"I64" / "F64" / "PTR"`-style tokens. If that reader is case-sensitive or expects those exact tags, you’ll silently mis-import signatures.

Even if it “kind of works” today, it’s two inconsistent formats.

---

### F) `label_compare()` is a full compiler pipeline with **no cycle guard** and **no mtime invalidation**

* It short-circuits only if `Library_Unit.is_compiled` is set.
* It never compares stored timestamps to source timestamps to recompile.
* It recursively compiles dependencies (via `symbol_manager_use_clauses` + parsing again) without an explicit “visited/in-progress” set.

So: stale outputs + potential infinite recursion on cyclic `with` graphs.

---

# 2) Duplicated code patterns (clear bloat + bug multipliers)

### A) Two file readers (`read_file` earlier, `read_file_contents` here)

Same pattern: `fseek/ftell/malloc/fread`, no short-read handling, no failure checks for `ftell`, and both return heap pointers with no ownership policy. One should be deleted, and the remaining one should return `(ptr,size)` (or at least check `fread`).

### B) Global emission logic duplicated in `label_compare()` and `main()`

The big “emit globals, handle string constants, arrays as fat pointer vs typed array” block is basically repeated with minor variations. Any correctness fix (array constrainedness, string literal storage, fat bounds layout) now needs to be made twice.

### C) You now have **three** library-unit workflows:

* `pks2()` (earlier) parses with-graph.
* `lookup_path()` + `read_ada_library_interface()` loads `.ali`.
* `label_compare()` re-parses and fully compiles a unit into a separate `.ll`.

They overlap but aren’t consistent (timestamps, formats, caching). This is exactly where compilers go off the rails.

---

# 3) Sketchy hacks / “ignore incorrectness” in this chunk

### A) Pointer-plausibility checks are still everywhere

`(uintptr_t)s->parent > 4096` is used in `.ali` writing and global name emission. This is not a real invariant; it’s papering over broken symbol parent pointers.

### B) `print_forward_declarations()` does semantic resolution in codegen

It calls `resolve_subtype()` while emitting declarations. That means codegen can allocate/normalize types and potentially mutate type state. GNAT keeps semantic resolution out of emission.

### C) External procedures don’t use the C type mapping path

You only use `ada_to_c_type_string()` for external *functions* (`is_c_external(s)`), not procedures. So C-imported procedures will often be declared with the Ada/LLVM “computation” type choices (i64 everywhere), which is ABI-wrong.

---

# 4) What diverges from GNAT-LLVM here

1. **Runtime ABI correctness**: GNAT doesn’t “fake struct tm layout” in IR.
2. **Stable unit graph**: GNAT has an explicit dependency graph with timestamps/checksums; here you have multiple ad-hoc loaders and a “compile-on-demand” that doesn’t invalidate.
3. **No heuristic parent-pointer checks**: GNAT maintains symbol invariants; it doesn’t guess pointer validity.
4. **One consistent interface format**: GNAT’s `.ali` is stable and self-consistent; here writer/reader look like they drift.

---

# 5) Unnecessary / low-value functions (this slice)

* `read_file_contents()` — duplicates `read_file()`; keep one.
* `find_type_symbol()` — thin wrapper; if kept, it should take a full path (and your call sites should pass one).
* Much of `generate_runtime_type()` should not be hard-coded as giant `fprintf` blobs inside the compiler; it should be a separate runtime module or embedded resource, otherwise it becomes unreviewable and error-prone (as shown by the 16-vs-24 finalize bug).

---

# 6) Function-by-function quality ratings (1–10)

* `generate_runtime_type()` (remainder shown here): **3/10**
  Useful glue, but has a concrete heap-corruption bug (`__ada_finalize`), bakes in 1-based assumptions, and uses non-portable libc struct layouts.

* `read_file_contents()`: **3/10**
  Same weaknesses as the earlier reader; should be unified and hardened.

* `print_forward_declarations()`: **5/10**
  Good intent (dedup, skip runtime redecls), but ABI rules are inconsistent (C extern procedures), and it performs semantic resolution during emission.

* `lfnd()`: **8/10**
  Simple, correct, readable.

* `find_type_symbol()`: **6/10**
  Fine as a helper, but current call sites pass nonsense “paths”.

* `write_ada_library_interface()`: **4/10**
  Generates useful export metadata, but timestamps are wrong, type tokens may not match the reader, and still uses parent-pointer plausibility hacks.

* `label_compare()`: **3/10**
  It’s a full compile pipeline hidden in a “compare” function name, with no cycle detection and no rebuild invalidation.

* `main()`: **5/10**
  Pipeline is coherent, but it duplicates large emission logic, compiles dependencies in an ad-hoc way, and still depends on fragile invariants (`parent` validity, 1-based helpers, etc.).

---

# 7) Competing implementations (high-leverage fixes)

### A) Make the unit system single-source-of-truth

Pick one approach:

* **Either** always compile `with` units explicitly (like a build system), and make the compiler only handle one unit at a time,
* **Or** implement a real internal unit graph with:

  * visited/in-progress sets (cycle handling),
  * timestamps/checksums,
  * cached parsed AST + symbol info.

Right now you have 3 half-systems.

### B) Fix `.ali` to be self-consistent

Define one stable token set for types and enforce it in both writer and reader (including casing). Also store full paths + mtimes (or checksums) for `with` units.

### C) Replace runtime blobs with a real runtime module

Keep runtime IR in a separate `.ll` (or embed as a resource) and include/link it, rather than printing it with 500 `fprintf`s. It becomes reviewable, testable, and you stop introducing silent memory-layout bugs.

### D) Kill the `(uintptr_t)>4096` checks by fixing symbol invariants

Make `Symbol.parent` either:

* always valid `Symbol*` (set at creation), or
* always null for top-level,
  and assert it. Stop guessing in emission.
