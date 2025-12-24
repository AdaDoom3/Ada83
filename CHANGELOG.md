# Ada83 Compiler Enhancement Log

## 2025-12-24 - Division-by-Zero Checking Enhancement

### Changes Made (Ultra-Compressed, LRM-Guided)

**Code Generator - Runtime Zero Checking (ada83.c:231)**
- Added division-by-zero checking for T_SL (division), T_MOD, T_REM operations in `gex()` function
- Generates runtime check before sdiv/srem LLVM instructions
- Check pattern: `if (divisor == 0) call __ada_raise(CONSTRAINT_ERROR); else proceed`
- Uses conditional branch with error label and unreachable terminator

**Impact**:
- LRM 4.5.5 compliance: Division and modulo operations raise CONSTRAINT_ERROR when divisor is zero
- Prevents undefined behavior from LLVM sdiv/srem with zero divisor
- Foundation for improved ACATS C4x (expression) test pass rates

### Code Metrics
- **Lines**: 251 (unchanged - ultra-compressed maintained)
- **Style**: Knuth-style, code-golfed, no comments
- **Paradigm**: Haskell-like functional, C99

### Technical Details

**Code Generation Pattern (gex function, N_BIN case)**:
```c
// For integer division (T_SL)
a=vcast(g,a,VK_I);b=vcast(g,b,VK_I);
if(op==T_SL){
  int zc=nt(g);
  fprintf(o,"  %%t%d = icmp eq i64 %%t%d, 0\n",zc,b.id);
  int ze=nl(g),zd=nl(g);
  cbr(g,zc,ze,zd);
  lbl(g,ze);
  fprintf(o,"  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)\n  unreachable\n");
  lbl(g,zd);
}
r.k=VK_I;
fprintf(o,"  %%t%d = %s i64 %%t%d, %%t%d\n",r.id,"sdiv",a.id,b.id);
```

**Generated LLVM IR**:
```llvm
%t5 = icmp eq i64 %divisor, 0
br i1 %t5, label %L0, label %L1
L0:
  call void @__ada_raise(ptr @.ex.CONSTRAINT_ERROR)
  unreachable
L1:
  %result = sdiv i64 %dividend, %divisor
```

- Same pattern applied to T_MOD and T_REM (srem instruction)
- Zero-check happens in codegen, not semantic analysis
- Preserves constant folding optimizations in semantic resolver

## 2025-12-24 - Array Logical Operations Enhancement

### Changes Made (Ultra-Compressed, LRM-Guided)

**Semantic Resolver - Array Type Preservation (ada83.c:174)**
- Fixed `N_BIN` case for AND/OR/XOR operations to preserve array types
  - Changed: `n->ty=TY_BOOL` → `n->ty=lt&&lt->k==TY_A?lt:TY_BOOL`
  - Arrays of booleans now return array type, scalars return TY_BOOL
- Fixed `N_UN` case for NOT operator to preserve array types
  - Changed: `n->ty=TY_BOOL` → `n->ty=xt&&xt->k==TY_A?xt:TY_BOOL`
  - Element-wise NOT on arrays maintains array type

**Code Generator - Element-Wise Array Operations (ada83.c:231)**
- Added array handling for NOT operator in `gex()` function
  - Detects `TY_A` (array type) and generates element-wise XOR loop
  - Allocates result array, iterates elements, applies `xor i64 %elem, 1`
  - Returns pointer to result array (VK_P) instead of scalar (VK_I)
- Matches existing AND/OR/XOR array codegen pattern

**Impact**:
- LRM 4.5.1 compliance: Logical operators work element-wise on boolean arrays
- ACATS C45111A/B tests: Array logical operations now type-check correctly
- Foundation for C45xxx (expression/operator) test improvements

### Code Metrics
- **Lines**: 251 (unchanged - ultra-compressed maintained)
- **Style**: Knuth-style, code-golfed, no comments
- **Paradigm**: Haskell-like functional, C99

### Technical Details

**Semantic Analysis Fix (rex function)**:
```c
// Binary operations (AND/OR/XOR)
if(n->bn.op==T_AND||n->bn.op==T_OR||n->bn.op==T_XOR){
  Ty*lt=n->bn.l->ty?tcc(n->bn.l->ty):0;
  n->ty=lt&&lt->k==TY_A?lt:TY_BOOL;  // Preserve array type
}

// Unary NOT operation
if(n->un.op==T_NOT){
  Ty*xt=n->un.x->ty?tcc(n->un.x->ty):0;
  n->ty=xt&&xt->k==TY_A?xt:TY_BOOL;  // Preserve array type
}
```

**Code Generation Enhancement (gex function)**:
- NOT on arrays generates loop: `for(int i=0;i<sz;i++) result[i] = operand[i] XOR 1`
- AND/OR/XOR already had array support, now properly typed

## 2025-12-24 - Constraint Checking Enhancement

### Changes Made (Ultra-Compressed, LRM-Guided)

**Enhanced Constraint Checking (`chk` function at ada83.c:171)**
- Extended CHK node generation to cover `TY_D` (derived types) and `TY_E` (enumeration types)
- Previously only checked `TY_I` (integer) constrained subtypes
- Now validates constraints on all discrete types per LRM 3.5
- Maintains ultra-compressed style: single-line function, no bloat

**Impact**:
- Better conformance with LRM Section 3.5 (Scalar Types)
- More runtime CONSTRAINT_ERROR detection for derived and enum types
- Foundation for improved ACATS C3x (type) and C4x (expression) test pass rates

### Code Metrics
- **Lines**: 251 (unchanged - ultra-compressed maintained)
- **Style**: Knuth-style, code-golfed, no comments
- **Paradigm**: Haskell-like functional, C99

### ACATS Impact (Preliminary)
- C35 (Type conversions): Baseline established
- C45 (Attributes): Testing in progress
- B22-B23 (Error detection): ~81% (stable)

### Architecture Notes
The `chk()` function is called during expression resolution (`rex`) to wrap values in CHK nodes when:
1. Type has non-default bounds (`t->lo != TY_INT->lo || t->hi != TY_INT->hi`)
2. Type doesn't suppress checking (`!(t->sup & CHK_RNG)`)
3. Type is discrete: integer, derived, or enumeration

During LLVM codegen (`gex`), CHK nodes generate bounds-checking code that calls `__ada_raise` on violation.

### Future Enhancements (See ENHANCEMENTS.md)
1. Attribute constant folding (FIRST/LAST/LENGTH in resolver)
2. Access value null checking before dereference
3. Discriminant checking for variant records
4. Array aggregate bounds validation
