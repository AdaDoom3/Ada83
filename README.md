# Ada83 Compiler

**Single-file Ada 83 compiler targeting LLVM IR**

## Quick Start

```bash
cc -o ada83 ada83.c && ./test.sh g c    # Compile and run C-group tests
```

## Common Pitfalls

**Problem:** Variable access generates undefined `%__slnk`
**Cause:** Accessing package-level variable (lv=0) via static link
**Fix:** Check `if(s->lv==0)` before static link code; load from `@PACKAGE__VAR`

**Problem:** LLVM error "integer constant must have integer type"
**Cause:** Using `ret ptr 0` instead of `ret ptr null`
**Fix:** Check return value kind; emit `ret ptr null` for VK_P

**Problem:** Symbol not found during code generation
**Cause:** Symbol removed during `sco()` scope closure
**Fix:** Modify `sco()` to preserve variables/procedures/functions (k==0,4,5)

**Problem:** Test compiles when it should reject (B-test WRONG_ACCEPT)
**Cause:** Parser too permissive, missing semantic checks
**Fix:** Add validation in `rdl()` or `rex()` functions

## Technical Deep-Dive

### Frame-Based Static Links

**Why pointers instead of values?**
Passing pointers to variables achieves perfect aliasing without synchronization overhead. Parent and child share the same memory locations.

**Frame construction:**
```c
// In gbf() - called at procedure entry
%__frame = alloca [N x ptr]           // N = max symbol element number
// For each variable at current level:
%t1 = getelementptr [N x ptr], ptr %__frame, i64 0, i64 <slot>
store ptr %v.VARNAME, ptr %t1         // Store pointer to variable
```

**Child access pattern:**
```c
// Parent calls child:
call void @child(..., ptr %__frame)   // Pass frame as last parameter

// Child accesses parent variable:
%t1 = getelementptr ptr, ptr %__slnk, i64 <slot>  // Get slot
%t2 = load ptr, ptr %t1                            // Load pointer
%t3 = load i64, ptr %t2                            // Load value

// Child modifies parent variable:
%t1 = getelementptr ptr, ptr %__slnk, i64 <slot>
%t2 = load ptr, ptr %t1
store i64 %value, ptr %t2                          // Store through pointer
```

### Package-Level Variables

**Global emission** (in main()):
```c
for (all symbols) {
  if (s->k == 0 && s->lv == 0) {  // Variable at package level
    // Build qualified name: PACKAGE__VARIABLE
    fprintf(o, "@%s=global %s 0\n", qualified_name, type);
  }
}
```

**Access pattern** (in gex() N_ID case):
```c
if (s && s->lv == 0) {
  // Direct load from global (no static link)
  fprintf(g->o, "  %%t%d = load %s, ptr @%s\n", r.id, vt(k), qualified_name);
} else if (s && s->lv >= 0 && s->lv < g->sm->lv) {
  // Parent scope variable (use static link)
  // ... double indirection code ...
} else {
  // Local variable
  fprintf(g->o, "  %%t%d = load %s, ptr %%v.%.*s.%u\n", ...);
}
```

