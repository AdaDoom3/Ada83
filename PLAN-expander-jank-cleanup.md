# Master Plan: Expander Jank Cleanup

**Goal:** Eliminate remaining code smells, `strcmp`-on-LLVM-type-string dispatch,
hardcoded type widths, duplicated logic, and magic numbers throughout the
code-generation / expansion section (~14760-26000) of `ada83.c`.

Each item is categorized by severity (how likely it is to produce wrong IR
on a non-x86-64 target or when types are not the "expected" size) and
grouped into self-contained patches that can land independently.

---

## Phase 1: Semantic Predicates Replace strcmp Dispatch

**Problem:** ~35 call sites compare LLVM type *strings* (via `strcmp`) to decide
codegen behavior.  This couples IR emission to string representation —
fragile, slow, and semantically backwards.  The Ada type system already
knows whether something is a pointer, float, or integer; the LLVM string
is a *consequence*, not a *source of truth*.

### 1A. Introduce `Llvm_Type_Is_Pointer(const char *ty)` helper
Replace every `strcmp(x, "ptr") == 0` (19 call sites) with a single
inline predicate.  Locations:
- `Emit_Convert_Ext` (15582, 15585, 15588, 15594, 15599)
- Binary comparisons (17968, 17974, 18068, 18069)
- Function return widening (18878)
- `Generate_Type_Conversion` (19214, 21116)
- Allocator (21160)
- Array bound guards (20599, 20617, 21258, 21278, 23595, 23613)

Most of these guards exist to prevent nonsensical conversions
(`sext ptr ...`).  Long term, the *caller* should pass a semantic flag
("this is a pointer, not an integer") so the guard is unnecessary.

### 1B. Replace `Is_Float_Type(const char *ty)` body
Currently `strcmp(ty, "float") == 0 || strcmp(ty, "double") == 0`
(line 15421).  Replace with `ty[0] != 'i' && ty[0] != 'p'` or better,
add `ty[0] == 'f' || ty[0] == 'd'` quick check — still string-level but
O(1) instead of O(n).

Better yet: introduce `enum Llvm_Type_Class { LTC_INT, LTC_FLOAT, LTC_PTR, LTC_FAT, LTC_STRUCT }` and have `Type_To_Llvm` return a `(const char *, Llvm_Type_Class)` pair, so callers never need strcmp at all.

### 1C. Eliminate `Type_Bits(const char *ty)` + `atoi`
`Type_Bits()` (line 15397) parses `"i64" → 64` via `atoi(ty+1)`.
Used by `Wider_Int_Type`, `Emit_Convert_Ext`, and
`Emit_Signed_Division_Overflow_Check` (line 15281).

Replace with `Type_Size_In_Bits(Type_Info *t)` that reads from
`t->size * 8` directly.  Where only an LLVM string is available, keep
`Type_Bits` as the fallback but add an assertion that `ty[0] == 'i'`.

### 1D. Replace `Bounds_Type_For()` / `Bounds_Alloc_Size()` strcmp chains
Lines 6309-6328: five `strcmp` calls each mapping `"i8"` → `"{ i8, i8 }"`.
Replace with `snprintf` (compute once) or a lookup table indexed by
bit-width: `alloc = 2 * (bits / 8)`, struct = `sprintf("{ i%d, i%d }", bits, bits)`.

---

## Phase 2: Hardcoded LLVM Types → Semantic Derivation

**Problem:** ~10 locations emit hardcoded `"double"`, `"i64"`, `"i1"`, or
`"float"` instead of deriving the LLVM type from the Ada type system.
This is wrong when a `FLOAT` type maps to `float` (32-bit) instead of
`double`, or when `INTEGER` is not `i64`.

### 2A. Float constraint check bound expressions (15737, 15758)
```c
lo = Emit_Convert(cg, lo, "double", flt_type);  // assumes double
```
Should derive source type from the expression:
```c
lo = Emit_Convert(cg, lo, Expression_Llvm_Type(cg, bound_expr), flt_type);
```

### 2B. `Emit_Float_Type_Limit` (19413-19425)
Uses `type->size <= 4 ? "float" : "double"` instead of
`Float_Llvm_Type_Of(type)`.  The helper already exists — use it.

### 2C. Record component default stores (23691-23695)
```c
val = Emit_Convert(cg, val, val_type, "double");
Emit(cg, "  store double %%t%u, ptr %%t%u\n", val, comp_ptr);
```
Should be:
```c
const char *ft = Float_Llvm_Type_Of(comp_type);
val = Emit_Convert(cg, val, val_type, ft);
Emit(cg, "  store %s %%t%u, ptr %%t%u\n", ft, val, comp_ptr);
```
Same for the boolean branch — use `Type_To_Llvm(comp_type)`.

### 2D. RTS `__ada_integer_value` calls (20014-20022)
Hardcoded `i64` return type.  Should use `Integer_Arith_Type(cg)`.

### 2E. Float attribute emissions all hardcode `double` (20302, 20324, 20351, 20365, 20380)
`EPSILON`, `SMALL`, `LARGE`, `SAFE_SMALL`, `SAFE_LARGE` all emit
`fadd double 0.0, 0x...` regardless of whether the target type is single
precision.  Should emit at `Float_Llvm_Type_Of(prefix_type)` precision.

For single-precision types, this means emitting `float` constants via
`bitcast i32 <hex> to float` instead of `fadd double 0.0, 0x<hex>`.

---

## Phase 3: Eliminate Duplicated Code

### 3A. `Emit_Fat_Pointer_Dynamic` ≡ `Emit_Fat_Pointer_From_Temps`
Lines 15979-16006 vs 16309-16337 are *byte-for-byte identical* in logic.
Delete one, alias the other.

### 3B. `Emit_Store_Fat_Pointer_Fields_To_Symbol` / `_To_Temp`
Lines 16081-16112 vs 16117-16147 share the same alloca+GEP+store body.
Extract common logic into a single helper that takes a `ptr`-emitting
callback (or just a temp ID for the destination).

### 3C. Float attribute `digits → mantissa → emax` calculation
This 4-line block is copy-pasted 5 times across lines 20242-20345:
```c
int digits = prefix_type->flt.digits;
if (digits <= 0) digits = (prefix_type->size <= 4) ? 6 : 15;
int64_t mantissa = (int64_t)ceil(digits * 3.321928094887362) + 1;
int64_t emax = 4 * mantissa;
```
Extract to:
```c
static void Float_Model_Parameters(Type_Info *t, int64_t *mantissa, int64_t *emax);
```

### 3D. `prefix_type->size <= 4` as float/double discriminator
Used at **13 call sites** to pick between single and double precision.
Replace with `Float_Is_Single(Type_Info *t)` that checks
`Float_Llvm_Type_Of(t)` or `t->flt.digits <= 6` consistently.

---

## Phase 4: Named Constants for Magic Numbers

### 4A. IEEE 754 structural constants
```c
#define LOG2_OF_10           3.321928094887362
#define IEEE_DOUBLE_MANTISSA 53
#define IEEE_FLOAT_MANTISSA  24
#define IEEE_DOUBLE_EMAX     1024
#define IEEE_FLOAT_EMAX      128
#define IEEE_DOUBLE_EMIN     (-1021)
#define IEEE_FLOAT_EMIN      (-125)
#define IEEE_DOUBLE_EPSILON  2.220446049250313e-16
#define IEEE_DOUBLE_MIN_NORMAL 2.2250738585072014e-308
#define IEEE_FLOAT_MIN_NORMAL  1.1754943508222875e-38
#define MACHINE_RADIX        2
```
Currently these appear as raw literals at ~30 locations in the attribute
handler (lines 20220-20500).

### 4B. LLVM hex for ±FLT_MAX / ±DBL_MAX (19416-19418)
Give them names:
```c
static const char *FLT_MAX_HEX_POS = "0x47EFFFFFE0000000";
static const char *FLT_MAX_HEX_NEG = "0xC7EFFFFFE0000000";
static const char *DBL_MAX_HEX_POS = "0x7FEFFFFFFFFFFFFF";
static const char *DBL_MAX_HEX_NEG = "0xFFEFFFFFFFFFFFFF";
```

### 4C. `BITS_PER_BYTE` for `* 8` multiplications
Lines 19608, 19623, 20534 use `size * 8`.  Replace with
`To_Bits(size)` (the helper already exists!) — verify it's used
consistently.

---

## Phase 5: Hardcoded Target Assumptions

### 5A. `target datalayout` / `target triple` (25030-25031)
Currently hardcoded to x86-64 Linux.  Should be a command-line option
or derived from the host.  Low priority since Ada83 currently only
targets x86-64, but architecturally wrong.

### 5B. `STRING_BOUND_TYPE "i64"` macro (line 74)
Hardcodes string bounds to `i64`.  Should derive from the type system
like `String_Bound_Type(cg)` already does (line 14844).  Grep for users
of the macro and replace with the dynamic version.

### 5C. Minimum frame size `8` (line 23861)
Hardcoded to pointer size on x86-64.  Should be
`sizeof(void *)` equivalent or derived from data layout.

---

## Phase 6: Dead Code and Warning Hygiene

### 6A. Remove `Emit_Fat_Pointer_Copy_To_Ptr` (line 16036)
Marked `__attribute__((unused))`.  If unused, delete it.  If it will be
needed soon, remove the attribute and add a caller.

### 6B. Audit `fprintf(stderr, "warning: ...")` in codegen (26 instances)
Several of these silently emit *wrong code* then continue:
- Line 18242: "unhandled binary operator" → defaults to `add`
- Line 18403: "unhandled unary operator" → returns operand unchanged
- Line 24894: "unknown composite type" → "assumes always equal"

These should either `abort()` (internal error), emit `unreachable` in the
IR, or at minimum set an error flag so the compilation is marked failed.

Warnings that indicate missing semantic info (e.g., "no type, defaulting
to 0") are acceptable as defensive fallbacks but should be tracked with a
counter and reported in the compilation summary.

---

## Phase 7: Comparison Dispatch Cleanup

### 7A. Float/bool/pointer/integer comparison switch (18041-18117)
Four near-identical switch blocks each:
1. Switch on `node->binary.op` for EQ/NE/LT/LE/GT/GE
2. fprintf warning on default
3. snprintf into `cmp_buf`

Extract to `Comparison_Instruction(op, type_class) → const char *`.

### 7B. Exponentiation float dispatch (17827)
`strcmp(lhs_ftype, "float") == 0` to pick `llvm.pow.f32` vs `llvm.pow.f64`.
Replace with `Llvm_Float_Intrinsic("pow", ftype)` or use
`Float_Llvm_Type_Of` to build the intrinsic name.

---

## Execution Order

| Priority | Phase | Risk if skipped | Effort |
|----------|-------|-----------------|--------|
| 1 | 2A-2E | Wrong IR for non-double floats | Small |
| 2 | 3C-3D | Hard to maintain, copy-paste bugs | Small |
| 3 | 4A-4C | Unreadable, error-prone constants | Small |
| 4 | 3A-3B | Code bloat, divergent fixes | Small |
| 5 | 1A-1D | Fragile, slow, conceptually wrong | Medium |
| 6 | 6A-6B | Silent wrong code on edge cases | Medium |
| 7 | 7A-7B | Code duplication | Small |
| 8 | 5A-5C | Only matters for non-x86-64 targets | Low |

Phases 1-4 can each land as an independent commit.  Phase 2 is highest
priority because it produces *incorrect IR* for 32-bit float types today.
