# Master Plan: Replace Hardcoded Float/Double Strings with Semantic Type Info

## Problem Statement

The codegen layer uses hardcoded `"double"` and `"float"` string literals and
`strcmp()` checks throughout, instead of deriving LLVM types from the semantic
`Type_Info` already present in the AST.  This is fragile, incorrect for
single-precision types, and diverges from GNAT LLVM's approach of operating at
native precision.

### Two classes of bug:

1. **Hardcoded `"double"` default** — Assumes every real value is 64-bit.
   Wrong for `FLOAT` (32-bit), future `LONG_LONG_FLOAT` (80/128-bit), and
   any user-defined float type with non-64-bit precision.

2. **String comparison on LLVM type names** — Uses `strcmp(ty, "double")` or
   `Is_Float_Type(str)` to decide codegen paths instead of querying
   `Type_Info.kind` / `Type_Info.size`.  Brittle and loses semantic info.

---

## Guiding Principles

- **Semantic over syntactic**: Use `Type_Is_Float(t)` / `Type_Is_Float_Representation(t)`
  and `Llvm_Float_Type(To_Bits(t->size))` instead of string matching.
- **`Emit_Convert` is the exception**: It is a low-level LLVM string→string
  converter.  `Is_Float_Type()` and `Type_Bits()` are legitimate there because
  it has no `Type_Info`.  Do NOT change `Emit_Convert_Ext`.
- **`Llvm_Float_Type()` stays**: It is the single blessed mapping from bit-width
  to LLVM type name.  All float LLVM type strings should flow through it.
- **Fixed-point uses `double` intermediary intentionally**: Ada fixed-point
  semantics scale through real arithmetic.  The `double` there is a deliberate
  precision choice for the intermediate, not a bug.  Leave fixed-point paths
  alone unless a `Type_Info` is trivially available.
- **Duration is always `double`**: Ada `DURATION` maps to `double` per the
  runtime ABI.  The delay/duration paths (lines 22693, 22897) are correct.

---

## Phase 0: Add Helper — `Float_Llvm_Type_Of(Type_Info *t)`

Add a small wrapper that extracts the LLVM float type string from a Type_Info,
with a safe fallback:

```c
static inline const char *Float_Llvm_Type_Of(const Type_Info *t) {
    if (t && t->size > 0)
        return Llvm_Float_Type((uint32_t)To_Bits(t->size));
    return "double";  /* UNIVERSAL_REAL / unknown → 64-bit */
}
```

This replaces the repeated pattern:
```c
const char *fty = "double";
if (t && t->kind == TYPE_FLOAT && t->size > 0)
    fty = Llvm_Float_Type((uint32_t)To_Bits(t->size));
```

---

## Phase 1: Eliminate Hardcoded `"double"` Defaults (10 sites)

Replace every `const char *xxx = "double"` that is immediately followed by a
conditional refinement from `Type_Info` with a single call to
`Float_Llvm_Type_Of(type)`.

### 1.1 — Emit_Constraint_Check float path (line ~15686)
```c
// BEFORE:
const char *flt_type = "double";
if (target->kind == TYPE_FLOAT && target->size > 0)
    flt_type = Llvm_Float_Type(...);
// AFTER:
const char *flt_type = Float_Llvm_Type_Of(target);
```

### 1.2 — Emit_Constraint_Check source float type (line ~15692)
```c
// BEFORE:
const char *src_flt = "double";
if (source->kind == TYPE_FLOAT && source->size > 0)
    src_flt = Llvm_Float_Type(...);
// AFTER:
const char *src_flt = Float_Llvm_Type_Of(source);
```

### 1.3 — Binary expression result float type (line ~17665)
```c
const char *float_type_str = "double";
// ... refined from left_type
```
→ `Float_Llvm_Type_Of(result_type)` or `Float_Llvm_Type_Of(left_type)`.

### 1.4 — Binary expression lhs float type (line ~17676)
→ `Float_Llvm_Type_Of(left_type)`

### 1.5 — Binary expression rhs float type (line ~17680)
→ `Float_Llvm_Type_Of(right_type)`

### 1.6 — Binary comparison float type (line ~17980)
```c
const char *float_type = "double";
```
→ Derive from left operand's Type_Info.

### 1.7 — Binary comparison right float type (line ~17986)
→ Derive from right operand's Type_Info.

### 1.8 — Membership test float type (line ~18145)
```c
const char *mem_float_type = "double";
```
→ Derive from left operand's Type_Info.

### 1.9 — Unary operator float type (line ~18317)
```c
const char *float_type = "double";
```
→ Derive from operand's Type_Info.

### 1.10 — Assignment source float type (line ~21033)
```c
const char *src_ftype = "double";
```
→ `Float_Llvm_Type_Of(src_type_info)`.

---

## Phase 2: Replace String Guards with Semantic Predicates (8 sites)

These sites use `strcmp(llvm_str, "double") != 0 && strcmp(llvm_str, "float") != 0`
to exclude float types from integer bound-extension paths.  Replace with
`Type_Is_Float_Representation(bound_expr->type)` checks.

### 2.1 — For-loop low bound guard (line ~20621)
### 2.2 — For-loop high bound guard (line ~20638)
### 2.3 — Allocation/constrained low bound guard (line ~21277)
### 2.4 — Allocation/constrained high bound guard (line ~21296)
### 2.5 — Object decl low bound guard (line ~23614)
### 2.6 — Object decl high bound guard (line ~23631)

Pattern:
```c
// BEFORE:
const char *low_llvm = Expression_Llvm_Type(cg, low_bound.expr);
if (strcmp(low_llvm, iat) != 0 && strcmp(low_llvm, "ptr") != 0 &&
    strcmp(low_llvm, "double") != 0 && strcmp(low_llvm, "float") != 0) {
    low = Emit_Convert(...);
}
// AFTER:
if (!Type_Is_Float_Representation(low_bound.expr->type)) {
    const char *low_llvm = Expression_Llvm_Type(cg, low_bound.expr);
    if (strcmp(low_llvm, iat) != 0 && strcmp(low_llvm, "ptr") != 0) {
        low = Emit_Convert(...);
    }
}
```

### 2.7 — Global variable zero-init (line ~23209)
### 2.8 — Array global variable zero-init (line ~24529)

Pattern:
```c
// BEFORE:
} else if (strcmp(type_str, "double") == 0 || strcmp(type_str, "float") == 0) {
    Emit(cg, " = linkonce_odr global %s 0.0\n", type_str);
// AFTER:
} else if (Type_Is_Float_Representation(ty)) {
    Emit(cg, " = linkonce_odr global %s 0.0\n", type_str);
```

---

## Phase 3: Fix Binary Float Operator Type Matching (lines ~18035-18043)

The binary operator path does explicit `strcmp` between `float_type` and
`right_float_type` strings to decide fptrunc/fpext direction.  Replace with
`Emit_Convert` which already handles this correctly:

```c
// BEFORE:
if (strcmp(float_type, "float") == 0 && strcmp(right_float_type, "double") == 0) {
    conv = Emit_Temp(cg);
    Emit(cg, "  %%t%u = fptrunc double %%t%u to float\n", conv, right);
} else if (strcmp(float_type, "double") == 0 && strcmp(right_float_type, "float") == 0) {
    conv = Emit_Temp(cg);
    Emit(cg, "  %%t%u = fpext float %%t%u to double\n", conv, right);
}
// AFTER:
if (strcmp(float_type, right_float_type) != 0) {
    right = Emit_Convert(cg, right, right_float_type, float_type);
}
```

This uses `Emit_Convert` which already dispatches fpext/fptrunc based on bit
widths, removing all hardcoded `"float"`/`"double"` string matching.

---

## Phase 4: Fix Float Exponentiation to Use Native Precision (lines ~17814-17827)

Currently forces conversion to `double` to call `llvm.pow.f64`.  Should use
the native precision intrinsic:

```c
// BEFORE:
if (strcmp(lhs_ftype, "double") != 0)
    left_d = Emit_Convert(cg, left, lhs_ftype, "double");
Emit(cg, "  call double @llvm.pow.f64(double %t, double %t)\n", ...);
if (strcmp(lhs_ftype, "double") != 0)
    Emit(cg, "  fptrunc double ... to %s\n", lhs_ftype);
// AFTER:
const char *pow_intrinsic = strcmp(lhs_ftype, "float") == 0
    ? "llvm.pow.f32" : "llvm.pow.f64";
// Convert exponent to match base type
uint32_t exp_f = Emit_Convert(cg, right, right_int_type_str, lhs_ftype);
Emit(cg, "  call %s @%s(%s %t, %s %t)\n",
     lhs_ftype, pow_intrinsic, lhs_ftype, left, lhs_ftype, exp_f);
```

This eliminates the round-trip through double for 32-bit floats, matching
GNAT LLVM behavior.  Note: LLVM's `llvm.pow.f32` is valid and well-defined.

---

## Phase 5: Fix Float Bound Emission (line ~16541)

The `Emit_Bound_Value` function hardcodes `fptosi double` for float bounds:

```c
// BEFORE:
Emit(cg, "  %%t%u = fptosi double %e to %s\n", t, b.float_value, iat);
// AFTER:
// Float bounds should not normally appear here (bounds are integer indices).
// If they do, use "double" as the safe intermediate (UNIVERSAL_REAL precision).
// This is intentional — bound values are semantic constants, not typed expressions.
```

This one is actually correct as-is for bound conversion (converting a real-valued
bound to an integer index), but we should document why.  The `double` literal
`%e` in the format string produces a double-precision constant, which matches
the `fptosi double` instruction.  No change needed, just add a comment.

---

## Phase 6: Fix `fptosi` at Float-to-Integer Assignment (line ~21887)

```c
// BEFORE:
Emit(cg, "  %%t%u = fptosi double %%t%u to %s\n", t, value, type_str);
// AFTER:
const char *val_ftype = Float_Llvm_Type_Of(src_type_info);
Emit(cg, "  %%t%u = fptosi %s %%t%u to %s\n", t, val_ftype, value, type_str);
```

The value may be `float` not `double`, so the `fptosi` source type must match.

---

## Phase 7: Fix Attribute Float Bound Conversion (line ~19493)

```c
// BEFORE:
if (strcmp(fty, "float") == 0 && strcmp(expr_fty, "double") == 0)
    Emit(cg, "  fptrunc double ...");
// AFTER:
if (strcmp(expr_fty, fty) != 0) {
    v = Emit_Convert(cg, v, expr_fty, fty);
}
```

Use `Emit_Convert` instead of hand-rolled strcmp + fptrunc.

---

## Phase 8: Cleanup — Remove Dead `Is_Float_Type()` Callers

After all the above, audit remaining callers of `Is_Float_Type()`.  The function
itself stays (used legitimately in `Emit_Convert_Ext`), but any codegen call
sites that have a `Type_Info` available should use `Type_Is_Float_Representation()`
instead.

---

## Phases NOT Changed (Intentionally Left Alone)

### Fixed-Point Small Division (lines ~15779, 17735, 17748)
These use `double` as an intermediate for fixed-point scaling arithmetic.
This is intentional: Ada fixed-point `SMALL` is a real value, and the division
`value / small` is computed in `double` precision before converting to the
integer mantissa via `fptosi`.  The `double` here is a precision choice for
the intermediate computation, not a type derived from any Ada type.

### Duration/Delay Paths (lines ~22693, 22897)
Ada `DURATION` is a fixed-point type, but the runtime ABI represents it as
`double` (seconds).  The `fmul double ... 1.0e6` and `fptoui double` are
correct per the runtime convention.

### `Emit_Convert_Ext` (lines ~15532-15609)
This is the low-level LLVM type-string converter.  It legitimately works with
type strings, not `Type_Info`.  `Is_Float_Type()` and `Type_Bits()` are correct
here.

### `Llvm_Float_Type()` and `Type_Bits()` Definitions
These are the canonical string-producing / string-parsing helpers.  They stay.

---

## Build & Verify

After each phase: `gcc -c ada83.c -std=gnu2x` — verify 31 pre-existing errors,
0 new errors.

## Commit Strategy

One commit per logical group (phases can be batched where they touch related code):
- Phases 0–1: "Replace hardcoded double defaults with Float_Llvm_Type_Of()"
- Phase 2: "Replace string guards with semantic Type_Is_Float predicates"
- Phase 3: "Use Emit_Convert for binary float type matching"
- Phase 4: "Use native-precision pow intrinsic for float exponentiation"
- Phases 5–7: "Fix remaining hardcoded float/double in codegen emission"
- Phase 8: "Audit and remove unnecessary Is_Float_Type() string checks"
