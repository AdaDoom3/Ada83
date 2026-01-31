# Master Plan: GNAT-Style Granular Runtime Checks

## Current State

The compiler has a **uniform-width** approach to runtime checks:

- **Internally**: All bounds/moduli stored as `int128_t`/`uint128_t` (good — prevents
  compile-time overflow).
- **IR emission**: Types are correctly sized per-type (`i8`, `i16`, `i32`, `i64`).
- **Constraint checks**: Done at `Integer_Arith_Type(cg)` width (typically `i64`) —
  wider than necessary but functionally correct.
- **Arithmetic overflow**: **Not checked at all** — plain `add`/`sub`/`mul` instructions
  with no overflow detection. Ada RM 4.5 requires `Constraint_Error` on overflow for
  signed integer types.
- **Division by zero**: **Not checked** — LLVM `sdiv`/`udiv` with zero divisor is UB.
- **Index checks**: Done implicitly via constraint checks on some paths, missing on others.
- **Length checks**: Not systematically emitted for array assignments/concatenation.

## Target State (GNAT LLVM Model)

GNAT LLVM (`gnatllvm-exprs.adb`, `gnatllvm-codegen.adb`) uses **per-check, per-type**
granularity:

| Check Category      | GNAT Approach                                              |
|---------------------|------------------------------------------------------------|
| Overflow (signed)   | `llvm.sadd.with.overflow.iN` intrinsics → `{result, i1}`  |
| Overflow (unsigned) | Not checked (modular types wrap by definition, RM 3.5.4)  |
| Range/Constraint    | Compare at wider-of-two-types, not at fixed width          |
| Division by zero    | `icmp eq divisor, 0` → branch to `Constraint_Error`       |
| Index check         | Compare index against array bounds at index type width     |
| Length check        | Compare source/target lengths for array assignment         |
| Access check        | `icmp eq ptr, null` → branch to `Constraint_Error`        |
| Discriminant check  | Verify discriminant matches expected value                 |
| Elaboration check   | Verify package body elaborated before call                 |

Each check category can be individually suppressed via `pragma Suppress(Check_Name)`.

---

## Architecture: Check Emission Functions

We need a family of `Emit_*_Check` functions, each respecting `pragma Suppress`:

```
┌─────────────────────────────────────────────────────────────────────┐
│                     CHECK EMISSION LAYER                           │
│                                                                    │
│  Emit_Overflow_Check(cg, left, right, op, type)                   │
│    → uses llvm.sadd/ssub/smul.with.overflow.iN                    │
│    → suppressed by OVERFLOW_CHECK (bit 2)                         │
│                                                                    │
│  Emit_Range_Check(cg, val, target_type, source_type)              │
│    → compare at wider of source/target width                      │
│    → suppressed by RANGE_CHECK (bit 1)                            │
│                                                                    │
│  Emit_Division_Check(cg, divisor, divisor_type)                   │
│    → icmp eq divisor, 0                                           │
│    → suppressed by DIVISION_CHECK (bit 16, new)                   │
│                                                                    │
│  Emit_Index_Check(cg, index, array_type, dimension)               │
│    → compare index against array bounds at native index width     │
│    → suppressed by INDEX_CHECK (bit 4)                            │
│                                                                    │
│  Emit_Length_Check(cg, src_len, dst_len, type)                    │
│    → compare lengths for array assignment                         │
│    → suppressed by LENGTH_CHECK (bit 8)                           │
│                                                                    │
│  Emit_Access_Check(cg, ptr)                                       │
│    → icmp eq ptr, null                                            │
│    → suppressed by ACCESS_CHECK (bit 32, new)                     │
│                                                                    │
│  Emit_Discriminant_Check(cg, actual, expected, disc_type)         │
│    → icmp ne actual, expected                                     │
│    → suppressed by DISCRIMINANT_CHECK (bit 64, new)               │
│                                                                    │
│  Check_Is_Suppressed(type_or_symbol, check_bit) → bool            │
│    → consults suppressed_checks bitmask                           │
│    → checks type, then base_type, then enclosing scope            │
└─────────────────────────────────────────────────────────────────────┘
```

---

## Phase 0: Check Suppression Infrastructure

**Goal**: Extend the existing `suppressed_checks` bitmask and add a uniform query function.

### 0.1 Extend check bit definitions

Current bits (in pragma Suppress handler, ~line 13742):
```c
RANGE_CHECK      = 1
OVERFLOW_CHECK   = 2
INDEX_CHECK      = 4
LENGTH_CHECK     = 8
ALL_CHECKS       = 0xFFFFFFFF
```

Add new bits:
```c
DIVISION_CHECK      = 16
ACCESS_CHECK        = 32
DISCRIMINANT_CHECK  = 64
ELABORATION_CHECK   = 128
STORAGE_CHECK       = 256
```

Define as named constants (enum or `#define`) near the Type_Info struct.

### 0.2 Add `Check_Is_Suppressed()` helper

```c
static bool Check_Is_Suppressed(Type_Info *type, Symbol *sym, uint32_t check_bit) {
    if (type && (type->suppressed_checks & check_bit)) return true;
    if (type && type->base_type && (type->base_type->suppressed_checks & check_bit)) return true;
    if (sym && (sym->suppressed_checks & check_bit)) return true;
    return false;
}
```

### 0.3 Parse additional pragma Suppress check names

Add `DIVISION_CHECK`, `ACCESS_CHECK`, `DISCRIMINANT_CHECK`, `ELABORATION_CHECK`,
`STORAGE_CHECK` to the pragma Suppress handler.

---

## Phase 1: Arithmetic Overflow Checks (LLVM Intrinsics)

**Goal**: Signed integer `+`, `-`, `*` raise `Constraint_Error` on overflow.
Modular types are **exempt** (they wrap, RM 3.5.4).

### 1.1 LLVM overflow intrinsics

LLVM provides:
```llvm
declare {i32, i1} @llvm.sadd.with.overflow.i32(i32 %a, i32 %b)
declare {i32, i1} @llvm.ssub.with.overflow.i32(i32 %a, i32 %b)
declare {i32, i1} @llvm.smul.with.overflow.i32(i32 %a, i32 %b)
```

These return `{result, overflow_flag}`. The overflow flag is `i1`: true if overflow occurred.

For unsigned (modular) types, we do NOT check — the existing plain `add`/`sub`/`mul`
already wraps naturally.

### 1.2 Create `Emit_Overflow_Checked_Op()`

```c
// Returns the temp ID holding the checked result.
// For suppressed checks or modular types, falls back to plain instruction.
static uint32_t Emit_Overflow_Checked_Op(
    Code_Generator *cg,
    uint32_t left, uint32_t right,
    const char *op,          // "add", "sub", "mul"
    const char *llvm_type,   // "i8", "i16", "i32", "i64"
    Type_Info *result_type)
{
    bool is_unsigned = Type_Is_Unsigned(result_type);
    bool suppressed = Check_Is_Suppressed(result_type, NULL, OVERFLOW_CHECK);

    if (is_unsigned || suppressed) {
        // Plain operation (wraps for modular, or check suppressed)
        uint32_t t = Emit_Temp(cg);
        Emit(cg, "  %%t%u = %s %s %%t%u, %%t%u\n", t, op, llvm_type, left, right);
        // Modular non-power-of-2: add urem as before
        if (is_unsigned && result_type->modulus != 0 && !Is_Power_Of_2_128(result_type->modulus)) {
            uint32_t wrapped = Emit_Temp(cg);
            Emit(cg, "  %%t%u = urem %s %%t%u, %s\n",
                 wrapped, llvm_type, t, U128_Decimal(result_type->modulus));
            return wrapped;
        }
        return t;
    }

    // Signed overflow check via LLVM intrinsic
    const char *intrinsic_op;
    if      (strcmp(op, "add") == 0) intrinsic_op = "sadd";
    else if (strcmp(op, "sub") == 0) intrinsic_op = "ssub";
    else if (strcmp(op, "mul") == 0) intrinsic_op = "smul";
    else { /* fallback: no intrinsic for div/rem */ ... }

    // Emit: %pair = call {iN, i1} @llvm.sOP.with.overflow.iN(iN %left, iN %right)
    uint32_t pair = Emit_Temp(cg);
    Emit(cg, "  %%t%u = call {%s, i1} @llvm.%s.with.overflow.%s(%s %%t%u, %s %%t%u)\n",
         pair, llvm_type, intrinsic_op, llvm_type, llvm_type, left, llvm_type, right);

    // Extract result and overflow flag
    uint32_t result = Emit_Temp(cg);
    Emit(cg, "  %%t%u = extractvalue {%s, i1} %%t%u, 0\n", result, llvm_type, pair);
    uint32_t ovf = Emit_Temp(cg);
    Emit(cg, "  %%t%u = extractvalue {%s, i1} %%t%u, 1\n", ovf, llvm_type, pair);

    // Branch on overflow
    uint32_t raise_label = cg->label_id++;
    uint32_t cont_label = cg->label_id++;
    Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n", ovf, raise_label, cont_label);
    Emit(cg, "L%u:  ; overflow\n", raise_label);
    Emit_Raise_Constraint_Error(cg, "arithmetic overflow");
    Emit(cg, "L%u:\n", cont_label);
    cg->block_terminated = false;

    return result;
}
```

### 1.3 Integrate into binary op codegen (~line 17453)

Replace:
```c
case TK_PLUS:  op = is_float ? "fadd" : "add"; break;
case TK_MINUS: op = is_float ? "fsub" : "sub"; break;
case TK_STAR:  op = is_float ? "fmul" : "mul"; break;
```

With overflow-checked variants for integer operations. Float operations don't need
overflow checks (IEEE 754 semantics apply). Fixed-point multiplication/division has
its own scaling path.

### 1.4 Declare LLVM intrinsics

At the top of the generated `.ll` file, declare all overflow intrinsics we might use:
```llvm
declare {i8,  i1} @llvm.sadd.with.overflow.i8(i8, i8)
declare {i8,  i1} @llvm.ssub.with.overflow.i8(i8, i8)
declare {i8,  i1} @llvm.smul.with.overflow.i8(i8, i8)
declare {i16, i1} @llvm.sadd.with.overflow.i16(i16, i16)
; ... etc for i32, i64, i128
```

Alternatively, emit declarations lazily (on first use per type width).

### 1.5 Unary negation overflow

`-Integer'First` overflows for two's-complement signed types. Handle as:
```llvm
%pair = call {iN, i1} @llvm.ssub.with.overflow.iN(iN 0, iN %operand)
```

### 1.6 Integer exponentiation overflow

`__ada_integer_pow` needs to check for overflow internally, or we need to post-check
the result. Options:
- (a) Make `__ada_integer_pow` return a wider type and check after
- (b) Implement integer power inline with overflow-checked multiplication loop
- (c) Post-check against type bounds (fast but misses intermediate overflow)

Recommend (b) for correctness: inline loop with `Emit_Overflow_Checked_Op` for each
multiplication step.

---

## Phase 2: Division by Zero Checks

**Goal**: `sdiv`/`udiv`/`srem`/`urem` with zero divisor → `Constraint_Error`.

### 2.1 Create `Emit_Division_Check()`

```c
static void Emit_Division_Check(Code_Generator *cg, uint32_t divisor,
                                 const char *llvm_type, Type_Info *type) {
    if (Check_Is_Suppressed(type, NULL, DIVISION_CHECK)) return;

    uint32_t cmp = Emit_Temp(cg);
    Emit(cg, "  %%t%u = icmp eq %s %%t%u, 0\n", cmp, llvm_type, divisor);
    uint32_t raise_label = cg->label_id++;
    uint32_t cont_label = cg->label_id++;
    Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n", cmp, raise_label, cont_label);
    Emit(cg, "L%u:\n", raise_label);
    Emit_Raise_Constraint_Error(cg, "division by zero");
    Emit(cg, "L%u:\n", cont_label);
    cg->block_terminated = false;
}
```

### 2.2 Insert before TK_SLASH, TK_MOD, TK_REM

In the binary op switch (~line 17457), before emitting `sdiv`/`udiv`/`srem`/`urem`,
call `Emit_Division_Check(cg, right, common_type, result_type)`.

### 2.3 Signed division overflow

`Integer'First / (-1)` overflows on two's complement. This is a special case:
```c
// After division-by-zero check, also check for MIN_INT / -1
if (!is_unsigned) {
    // Check: divisor == -1 AND dividend == type_min → overflow
    // Or use llvm.sadd.with.overflow on the negation path
}
```

GNAT handles this by checking `dividend = Type'First AND divisor = -1`.

---

## Phase 3: Granular Range/Constraint Checks

**Goal**: Perform constraint checks at the **wider of source/target type** instead of
uniformly at `Integer_Arith_Type`.

### 3.1 Refactor `Emit_Constraint_Check()`

Current signature:
```c
static uint32_t Emit_Constraint_Check(Code_Generator *cg, uint32_t val, Type_Info *target)
```

New signature:
```c
static uint32_t Emit_Constraint_Check(Code_Generator *cg, uint32_t val,
                                       Type_Info *target, Type_Info *source)
```

The `source` parameter tells us the type width of `val`. The check compares at:
```c
const char *check_type = Wider_Int_Type(cg,
    Type_To_Llvm(source), Type_To_Llvm(target));
```

This means: if assigning `Long_Integer` (i64) to `Short_Integer` (i16), we compare
at i64 width. If assigning `Short_Integer` to `Integer` (i32), we compare at i32.

### 3.2 Respect `pragma Suppress(Range_Check)`

```c
if (Check_Is_Suppressed(target, NULL, RANGE_CHECK)) return val;
```

### 3.3 Update all call sites

Every call to `Emit_Constraint_Check` needs to pass the source type. There are ~10
call sites to update (assignment, parameter passing, function return, object init).

### 3.4 Separate concerns: range check vs overflow check

Ada distinguishes:
- **Overflow check**: the arithmetic operation itself overflowed (e.g., `Integer'Last + 1`)
- **Range check**: the result is valid for the operation's type but not for the target
  subtype (e.g., assigning 300 to a `Natural` that holds 0..255)

With overflow intrinsics handling the first, `Emit_Constraint_Check` only handles the
second.

---

## Phase 4: Index Checks

**Goal**: Array indexing checks index against actual array bounds at native index width.

### 4.1 Create `Emit_Index_Check()`

```c
static uint32_t Emit_Index_Check(Code_Generator *cg, uint32_t index,
                                  uint32_t low_bound, uint32_t high_bound,
                                  const char *index_type, Type_Info *array_type) {
    if (Check_Is_Suppressed(array_type, NULL, INDEX_CHECK)) return index;

    bool is_unsigned = Type_Is_Unsigned(/* index subtype */);
    const char *lt = is_unsigned ? "ult" : "slt";
    const char *gt = is_unsigned ? "ugt" : "sgt";

    // index < low?
    uint32_t cmp_lo = Emit_Temp(cg);
    Emit(cg, "  %%t%u = icmp %s %s %%t%u, %%t%u\n", cmp_lo, lt, index_type, index, low_bound);
    // index > high?
    uint32_t cmp_hi = Emit_Temp(cg);
    Emit(cg, "  %%t%u = icmp %s %s %%t%u, %%t%u\n", cmp_hi, gt, index_type, index, high_bound);
    uint32_t out_of_range = Emit_Temp(cg);
    Emit(cg, "  %%t%u = or i1 %%t%u, %%t%u\n", out_of_range, cmp_lo, cmp_hi);

    uint32_t raise_label = cg->label_id++;
    uint32_t cont_label = cg->label_id++;
    Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n", out_of_range, raise_label, cont_label);
    Emit(cg, "L%u:\n", raise_label);
    Emit_Raise_Constraint_Error(cg, "index check");
    Emit(cg, "L%u:\n", cont_label);
    cg->block_terminated = false;
    return index;
}
```

### 4.2 Insert at array indexing sites

In `Generate_Expression` for `NK_APPLY` array indexing (~line 16157), after computing
the index value, emit the index check against the array's actual bounds.

For constrained arrays: bounds are static (compile-time known).
For unconstrained arrays: bounds come from the fat pointer's bounds struct.

---

## Phase 5: Length Checks

**Goal**: Array assignment checks that source and destination have matching lengths.

### 5.1 Create `Emit_Length_Check()`

```c
static void Emit_Length_Check(Code_Generator *cg,
                               uint32_t src_length, uint32_t dst_length,
                               const char *len_type, Type_Info *array_type) {
    if (Check_Is_Suppressed(array_type, NULL, LENGTH_CHECK)) return;

    uint32_t cmp = Emit_Temp(cg);
    Emit(cg, "  %%t%u = icmp ne %s %%t%u, %%t%u\n", cmp, len_type, src_length, dst_length);
    uint32_t raise_label = cg->label_id++;
    uint32_t cont_label = cg->label_id++;
    Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n", cmp, raise_label, cont_label);
    Emit(cg, "L%u:\n", raise_label);
    Emit_Raise_Constraint_Error(cg, "length check");
    Emit(cg, "L%u:\n", cont_label);
    cg->block_terminated = false;
}
```

### 5.2 Insert at array assignment sites

When assigning one array to another (whole-array assignment), compute both lengths
and compare. This applies to:
- Direct array-to-array assignment
- String assignment
- Slice assignment
- Parameter passing of constrained actuals to unconstrained formals (for OUT/IN OUT)

---

## Phase 6: Access (Null Pointer) Checks

**Goal**: Dereferencing a null access value → `Constraint_Error`.

### 6.1 Create `Emit_Access_Check()`

```c
static void Emit_Access_Check(Code_Generator *cg, uint32_t ptr_val, Type_Info *acc_type) {
    if (Check_Is_Suppressed(acc_type, NULL, ACCESS_CHECK)) return;

    uint32_t cmp = Emit_Temp(cg);
    Emit(cg, "  %%t%u = icmp eq ptr %%t%u, null\n", cmp, ptr_val);
    uint32_t raise_label = cg->label_id++;
    uint32_t cont_label = cg->label_id++;
    Emit(cg, "  br i1 %%t%u, label %%L%u, label %%L%u\n", cmp, raise_label, cont_label);
    Emit(cg, "L%u:\n", raise_label);
    Emit_Raise_Constraint_Error(cg, "access check");
    Emit(cg, "L%u:\n", cont_label);
    cg->block_terminated = false;
}
```

### 6.2 Insert before every `.all` dereference and implicit dereference

In the codegen for `NK_SELECTED` with `.ALL` and implicit dereferences through
access-to-record and access-to-array types.

---

## Phase 7: Discriminant Checks

**Goal**: Accessing a variant record component checks that the discriminant matches.

Already partially implemented via constraint checks on discriminant values. Needs
to be formalized with its own `DISCRIMINANT_CHECK` bit so it can be independently
suppressed.

---

## Implementation Order & Dependencies

```
Phase 0: Check suppression infrastructure     ← foundation, do first
    │
    ├── Phase 1: Overflow checks (intrinsics)  ← highest impact, most complex
    │       └── 1.1-1.4: binary ops
    │       └── 1.5: unary negation
    │       └── 1.6: exponentiation
    │
    ├── Phase 2: Division checks               ← small, high value
    │
    ├── Phase 3: Granular range checks         ← refactor existing code
    │
    ├── Phase 4: Index checks                  ← important for safety
    │
    ├── Phase 5: Length checks                  ← array assignment safety
    │
    ├── Phase 6: Access checks                 ← null pointer safety
    │
    └── Phase 7: Discriminant checks           ← formalize existing
```

Phases 0-2 are the critical path. Phase 0 is trivial. Phases 1 and 2 together give
us arithmetic safety. Phase 3 improves existing code. Phases 4-7 add missing checks.

---

## LLVM Intrinsic Declarations Needed

These need to be declared at the module level (top of `.ll` output). Emit lazily on
first use or eagerly for all widths in use:

```llvm
; Signed overflow intrinsics
declare {i8,  i1} @llvm.sadd.with.overflow.i8(i8, i8)
declare {i8,  i1} @llvm.ssub.with.overflow.i8(i8, i8)
declare {i8,  i1} @llvm.smul.with.overflow.i8(i8, i8)
declare {i16, i1} @llvm.sadd.with.overflow.i16(i16, i16)
declare {i16, i1} @llvm.ssub.with.overflow.i16(i16, i16)
declare {i16, i1} @llvm.smul.with.overflow.i16(i16, i16)
declare {i32, i1} @llvm.sadd.with.overflow.i32(i32, i32)
declare {i32, i1} @llvm.ssub.with.overflow.i32(i32, i32)
declare {i32, i1} @llvm.smul.with.overflow.i32(i32, i32)
declare {i64, i1} @llvm.sadd.with.overflow.i64(i64, i64)
declare {i64, i1} @llvm.ssub.with.overflow.i64(i64, i64)
declare {i64, i1} @llvm.smul.with.overflow.i64(i64, i64)
declare {i128, i1} @llvm.sadd.with.overflow.i128(i128, i128)
declare {i128, i1} @llvm.ssub.with.overflow.i128(i128, i128)
declare {i128, i1} @llvm.smul.with.overflow.i128(i128, i128)
```

---

## ACATS Tests That Exercise These Checks

These ACATS test groups specifically test runtime check behavior:

- **C4A**: Constraint checks on assignment and conversion
- **C45**: Arithmetic overflow detection
- **C46**: Type conversion constraint checks
- **C52**: Assignment constraint checks
- **C64**: Parameter passing constraint checks
- **C85**: Access check (null dereference)
- **CB**: Constraint_Error handling and propagation
- **CC**: Suppression of checks (pragma Suppress)

Running `run_acats.sh` after each phase will catch regressions and validate new checks.

---

## Risk Analysis

| Risk                              | Mitigation                                        |
|-----------------------------------|---------------------------------------------------|
| Overflow intrinsics slow codegen  | Only for signed non-suppressed; most code uses i32 |
| Breaking existing ACATS tests     | Run full suite after each phase                   |
| Modular wrapping + overflow check | Never check modular types (TYPE_MODULAR excluded) |
| Fixed-point overflow              | Check at scaled-integer level (pre-scaling)       |
| Universal_Integer overflow        | These are compile-time only, not runtime          |
| Nested check bloat (check inside check) | GNAT avoids via "Do_Overflow_Check" flag on AST nodes; we can use a simpler rule: only check at operation sites, not at every conversion |

---

## Code Locations Reference

| What                      | File        | Line(s)   |
|---------------------------|-------------|-----------|
| `Integer_Arith_Type()`    | ada83.c     | 14792     |
| `Expression_Llvm_Type()`  | ada83.c     | 15195     |
| `Emit_Constraint_Check()` | ada83.c     | 15387     |
| `Emit_Raise_Constraint_Error()` | ada83.c | 15097  |
| Binary op codegen switch  | ada83.c     | 17453     |
| Unary op codegen          | ada83.c     | 17961     |
| Assignment codegen        | ada83.c     | ~21500    |
| Array indexing codegen    | ada83.c     | ~16157    |
| `.all` dereference        | ada83.c     | ~18879    |
| pragma Suppress handler   | ada83.c     | 13741     |
| `suppressed_checks` field | ada83.c     | 5817,6447 |
| `Type_Is_Unsigned()`      | ada83.c     | ~5835     |
| Modular wrapping (urem)   | ada83.c     | after arith ops |
| `Wider_Int_Type()`        | ada83.c     | 15118     |
| `Type_To_Llvm()`          | ada83.c     | (§13 top) |
| LLVM decl emission        | ada83.c     | module header |

---

## Summary: What Changes

1. **New functions**: `Emit_Overflow_Checked_Op`, `Emit_Division_Check`,
   `Emit_Index_Check`, `Emit_Length_Check`, `Emit_Access_Check`,
   `Check_Is_Suppressed`

2. **Modified functions**: `Emit_Constraint_Check` (add source type param, use
   wider-of-two), binary op codegen (route through overflow-checked ops),
   unary negation codegen, division codegen, array indexing, `.all` dereference

3. **New infrastructure**: Check bit constants, LLVM intrinsic declarations,
   lazy intrinsic declaration tracking

4. **No changes to**: Parser, semantic analysis, type system internals,
   compile-time bound representation (int128_t stays)
