

## 1. LLVM unnesting (nested subprograms → LLVM functions)

### 1.1 What’s clearly in the file

From the symbol-table / scope machinery:

* `struct Sy` has fields like:

  * `Sy *nx, *pv;`  – chain, “previous” / parent symbol.
  * `int sc, ss;`   – scope / “statement” depth.
  * `int el; int lv;` – environment / lexical level markers.
  * `uint8_t vis; Sy *hm;` – visibility + hash-map link.
* `struct Sm` has:

  * `Sy* sy[4096]; int sc; int ss;`
  * `Sy* sst[256]; int ssd;` – a stack of “symbols defined at each scope”.
  * `No *pk;` – current package.
  * various vectors tracking global things (`gt`, `dps`, `ex`, etc.).

From codegen:

* You generate LLVM functions with `fprintf(o,"define ... @\"%s\"(", nb);` and `nb` is built from `n->sy->ext_nm` (external name).
* No explicit “nested function” support in LLVM is used; so nested Ada subprograms *must* be unnested to top-level functions with some environment mechanism.

The ingredients for unnesting are there (lexical levels, symbol stack).

---

### 1.2 Suggestions: make unnesting an explicit, structured phase

Right now, the pieces (lexical depth, symbol stack, ext names) are there but they’re entangled with parsing, resolution, and codegen. I’d suggest you treat **unnesting** as a distinct step between resolution and codegen:

#### Step 1 – Compute captured variables for each subprogram

Add a small analysis pass:

* For every subprogram node `N_SUBP`/`N_PROC`/`N_FUN` (whatever your AST tags are):

  1. Walk its body.
  2. For any reference to a symbol `Sy *s` where `s->sc < subp_scope` (or `s->lv < subp_lexical_level`), mark `s` as “captured” by this subprogram.

* Store, for each subprogram node:

  ```c
  struct EnvInfo {
      Sy **captures;
      uint32_t n;
      // maybe also: parent subprogram, lexical level, etc.
  };
  ```

  You can tuck an `EnvInfo*` pointer into the subprogram AST node.

Use your existing `Sy.sc`, `Sy.lv`, `Sm.sst` to find “outer” variables; don’t recompute from scratch at codegen time.

#### Step 2 – Synthesize environment types

For each subprogram that captures outer variables:

* Make a synthetic `Ty` representing its environment, something like:

  ```c
  struct EnvTy { Ty base; SV fields; };
  ```

* Fields correspond 1:1 to captured `Sy*`.

* In LLVM, this becomes:

  ```llvm
  %env.<ext_nm> = type { <field types> }
  ```

Give each such type a stable name derived from the subprogram’s external name (`ext_nm`) plus maybe a numeric suffix.

Keep this in a central table in `Sm` (e.g. extend `Sm->gt` or add another vector just for env types).

#### Step 3 – Lift nested subprograms to top-level LLVM functions

When generating code for a subprogram that *has* an `EnvInfo`:

* Emit an extra, first parameter:

  ```llvm
  define void @"pkg.subp"(ptr %env, <user params>...)
  ```

* Everywhere inside the body where you refer to captured variables, generate loads/stores relative to `%env` instead of relative to the outer frame.

In the C code:

* Extend whatever structure represents the function signature (probably the symbol’s `Ty`) with:

  ```c
  bool has_env;
  Ty *env_ty;
  ```

* And in codegen for subprograms, if `has_env` is true, treat the first LLVM argument as `%env`.

#### Step 4 – Build environment instances at enclosing scopes

Where you *define* / instantiate a nested subprogram (its binding occurrence):

* Allocate an environment object holding pointers (or by-value copies) of each captured variable:

  ```llvm
  %env = alloca %env.<ext_nm>
  ; for each captured Sy* s:
      %p = getelementptr %env.<ext_nm>, ptr %env, i32 0, i32 <field_index>
      store <type-of-s> %s_value, ptr %p
  ```

* When you pass the nested subprogram as:

  * A normal nested call: call `@"pkg.subp"(%env, args...)`.
  * An access-to-subprogram value: construct a record `{ptr func, ptr env}`.

That way, you don’t need any implicit “static link”; it’s all explicit.

#### Step 5 – Represent access-to-subprogram uniformly

In the front end:

* Your `access` to subprogram type should be represented as:

  ```c
  typedef struct { void *fn; void *env; } ASubp;
  ```

In LLVM:

```llvm
%asubp = type { ptr, ptr }  ; (function ptr, env ptr)
```

The call thunk:

* Generated IR will do:

  ```llvm
  ; given %a : %asubp and <actuals...>
  %fn  = load ptr, ptr (gep to first field)
  %env = load ptr, ptr (gep to second field)
  call void %fn(ptr %env, <actuals...>)
  ```

This works uniformly for:

* Top-level subprograms: you can use a null or dummy env pointer.
* Nested subprograms: env points at the environment instance.

This unifies unnesting, closure semantics, and access-to-subprogram.

---

### 1.3 Specific tweaks in this file to support that

Given the existing structures:

1. **Use `Sy.lv` / `Sy.el` plus `Sm.sst/ssd` to compute captures early**, not at codegen.
   Add an explicit `compute_envs(Sm *SM, No *root)` pass that fills `EnvInfo` on subprogram nodes.

2. **Extend the symbol’s type for subprograms** with:

   * a `bool has_env; Ty *env_ty;`
   * external naming that doesn’t encode nesting (or that does, but is consistent).

3. **Make codegen for subprograms always go through one helper** (e.g. `gen_subp(Gn*, No*)`) that:

   * looks at `env_ty` to decide whether to emit an extra `%env` argument,
   * uses `EnvInfo` to decide when to load from `%env` instead of outer variables.

4. **Factor out access-to-subprogram representation** in one place (type + call helper) instead of occasionally re-emitting function pointers without env.

This keeps “LLVM unnesting” from being scattered across your resolution and gex/gdl/gsp logic.

---

## 2. `with` / `use` packages

### 2.1 What’s clearly in the file

From the lexer/parser:

* Tokens:

  ```c
  T_USE, T_WITH, T_PKG, T_PROC, T_FUN, ...
  ```
* Keywords table maps `"use" → T_USE`, `"with" → T_WITH`.

From the parser:

* There’s a context-clause node `CX` and compilation unit node `CU`:

  ```c
  static No*pcx(Ps*p){
      L lc=pl(p);
      No*cx=ND(CX,lc);
      while(pa(p,T_WITH)||pa(p,T_USE)||pa(p,T_PRG)||pa(p,T_PGM)){ /* etc */
          if(pm(p,T_WITH)){
              do{
                  No*w=ND(WI,lc);
                  w->wt.nm=pi(p);
                  nv(&cx->cx.wt,w);
              }while(pm(p,T_CM));
              pe(p,T_SC);
          }else if(pm(p,T_USE)){
              /* build use-clause nodes into cx->cx.us */
          }else{
              /* other context forms */
          }
      }
      return cx;
  }

  static No*pcu(Ps*p){
      L lc=pl(p);
      No*n=ND(CU,lc);
      n->cu.cx=pcx(p);
      /* then parse package/procedure/body, etc. */
      return n;
  }
  ```

So `CX` has at least `cx.wt` (with-list) and `cx.us` (use-list).

From the symbol machinery:

* `Sm` includes:

  ```c
  Sy*sy[4096];
  No*ds;          // declarations?
  No*pk;          // current package
  SV uv;          // use-visible symbols
  SV ex;          // external symbols
  uint64_t uv_vis[64]; // bitset per bucket for use-visibility
  ...
  ```

* `Sy` has `uint8_t vis;` where bit 1 likely means “directly visible in this scope” and bit 2 “visible via use”.

* `syf(Sm*SM,S nm)` finds a symbol:

  * it keeps `Sy *imm = 0;` (best direct hit) and `Sy *pot = 0;` (best use-visible hit).
  * It loops through `Sy* s = SM->sy[hash(nm)]` and updates `imm` based on `(s->vis & 1)` and `pot` based on `(s->vis & 2)`, using `sc` as a tie-breaker.
  * In the end returns `imm ?: pot`.

* `sfu(Sm*SM,Sy*s,S nm)` appears to be the “apply use clause” helper:

  * It looks up bucket `h = syh(nm)&63`, builds `b = 1ULL << h`.
  * Probably walks the `Sy*`s inside some package/type symbol `s` and:

    * adds them to `SM->uv`,
    * sets `sy->vis |= 2`,
    * maybe records them in `SM->ex` as well.
  * Clears `SM->uv_vis[h]` at some point when leaving scope.

So the basic design is:

* `with` gives you a package symbol, stored in the env (probably `Sm->pk` or in `Sm->ex/dps`).
* `use` (via `sfu`) marks certain symbols from that package/type as *use-visible* (vis&2) in the current scope.

That maps pretty naturally to Ada’s rules.

---

### 2.2 Suggestions: clarify and harden `with` / `use` handling

#### 2.2.1 Separate “availability” vs “direct visibility”

Ada 83 has three levels relevant here:

1. **Library unit exists** – it’s in the environment / symbol table.
2. It is **with**’ed – it participates in elaboration and its full view is known.
3. It is **use**’d – some of its declarations become directly visible in the current scope.

In this file:

* You already have room for:

  * “exists” via the global symbol table (`SM->sy` buckets).
  * “with”ed units via context clause AST `CX` and global lists like `Sm->ex`, `Sm->dps`.
  * “use”d declarations via `SM->uv` and `vis & 2`.

**Suggestions:**

* Have a clean function that processes the context clause for a compilation unit:

  ```c
  static void apply_context(Sm *SM, No *cx) {
      if (!cx) return;
      for (each w in cx->cx.wt) {
          // resolve package name w->wt.nm to Sy* (library unit)
          // record in a 'with' list for elaboration:
          sv(&SM->ex, pkg_sy);
      }
      for (each use_clause in cx->cx.us) {
          // call sfu for that package/type
      }
  }
  ```

* Call `apply_context` once when starting to process a compilation unit (`CU`).

Keep the conceptual split:

* `apply_context` for “which units/types are with/use here”.
* `sfu` strictly for “mark syms use-visible according to a particular use clause”.

#### 2.2.2 Make scope push/pop manage use-visibility cleanly

You have `scp(Sm *SM)` / `sco(Sm *SM)` for entering and leaving scopes, and you already do:

```c
static void scp(Sm*SM){
    SM->sc++;
    SM->ss++;
    if (SM->ssd < 256) {
        int m = SM->ssd;
        SM->ssd++;
        SM->sst[m] = 0;
    }
}
```

and in `sco` you walk symbols at this scope and adjust `vis` / drop them.

Extend that pattern explicitly to `use` visibility:

* When entering a new scope, record that we have a new “use layer”:

  ```c
  static void scp(Sm *SM) {
      SM->sc++;
      SM->ss++;
      if (SM->ssd < 256) {
          int m = SM->ssd++;
          SM->sst[m] = NULL;
      }
      // Optionally snapshot uv_vis here if you want per-scope bitsets
  }
  ```

* When processing a `use P;` at scope S, have `sfu`:

  * Tag the relevant symbols with `vis |= 2`.
  * Record in `SM->uv` *and* logically associate them with this scope level (e.g. by pushing them into `SM->sst[SM->ssd-1]` as well).

* When leaving the scope in `sco`:

  * For each symbol that had `vis & 2` set *by this scope’s use clauses*, clear that bit (`vis &= ~2`).
  * Clean up `SM->uv` and `uv_vis` accordingly.

This ensures `use` only affects lookups within the correct scope, and you don’t end up with lingering use-visible symbols in outer scopes.

#### 2.2.3 Enforce Ada conflict rules in `syf`

Ada’s rule: if a name is made directly visible *through multiple use clauses* and becomes ambiguous, you must require a qualification and not pick arbitrarily.

In your `syf` you have:

* `imm` = best direct-visible candidate (`vis & 1`)
* `pot` = best use-visible candidate (`vis & 2`)

Suggestion: refine that:

* While scanning candidates:

  * Track:

    * `Sy *pot;`
    * `int pot_count;`
  * For each candidate with `vis & 2` in the same scope depth and matching `nm`:

    * If `pot == NULL` → `pot = s; pot_count = 1`.
    * Else if it’s a *different* declaration in a *different library unit* (or different `sc`) → `pot_count++`.

* After scanning:

  ```c
  if (imm) return imm;
  if (pot && pot_count == 1) return pot;

  if (pot_count > 1) {
      // ambiguous use-visible; require qualification
      // generate an error in resolution, or mark a special "ambiguous" symbol
  }
  ```

This prevents silent, non-Ada choices when two packages `use` bring in the same operator or object name.

#### 2.2.4 Distinguish `use P;` vs `use type T;`

If you support `use type T;`, the semantics are narrower:

* Only *operators* whose profiles involve `T` (or its descendants) should become directly visible.

Implementation suggestion in this file:

* Let `sfu` take an extra parameter indicating “kind of use”:

  ```c
  enum UseKind { USE_PACKAGE, USE_TYPE };
  static void sfu(Sm *SM, Sy *s, S nm, enum UseKind kind);
  ```

* For `USE_PACKAGE`, you do what you seem to do now: walk all visible declarations in package `s`.

* For `USE_TYPE`:

  * Find the defining package/type of `T`.
  * Iterate over its operators only (you can mark operators in `Sy.k` or via a type tag).
  * For each operator, check its parameter/result types; if they involve `T` (or its base subtype), then mark it `vis |= 2`.

This matches Ada’s idea that `use type` just makes operators on the type directly visible, not every random declaration in the package.

#### 2.2.5 Drive elaboration order from `with` dependencies

`with` affects the elaboration order of packages. You already track some “external” symbols via `SM->ex` and “deferred parts” via `SM->dps[256]`.

Suggestion:

* When you process context `CX`:

  * For each `with P;`, resolve `P` to a package symbol `Sy *pkg`.
  * Record `pkg` in a “dependencies” list for the current compilation unit.

* At the end, before emitting the main body / library-level code:

  * Topologically sort the dependency graph (packages with their `with` lists).

  * Emit an elaboration function for each package:

    ```llvm
    define void @elab.P() { … }
    ```

  * Emit a “main elaboration” that calls these in the correct order.

I’ll stick to two things here: **what this file actually does** with the secondary stack + access types, and **concrete suggestions** to make that design safer / more Ada-83-ish.

(Also: only `ada83.c` is available now; if you had other runtime files before, they’ve aged out.)

---

## 1. What’s in this file

### 1.1 Secondary stack runtime

This file *generates* LLVM IR for a simple bump-pointer “secondary stack”:

* Globals (all in IR strings):

  ```llvm
  @__ss_base = global ptr null
  @__ss_size = global i64 0
  @__ss_ptr  = global i64 0
  ```

* `__ada_ss_init`:

  From the snippets:

  ```c
  fprintf(o,
    "define linkonce_odr void @__ada_ss_init(){"
    "%%p=call ptr @malloc(i64 1048576)\n"
    "store ptr %%p,ptr @__ss_base\n"
    "store i64 1048576,ptr @__ss_size\n"
    "store i64 0,ptr @__ss_ptr\n"
    "ret void}\n");
  ```

  So: **one global 1 MiB region**, malloc’ed on first use.

* `__ada_ss_mark`:

  ```c
  fprintf(o,
    "define linkonce_odr i64 @__ada_ss_mark(){"
    "%%m=load i64,ptr @__ss_ptr\n"
    "ret i64 %%m}\n");
  ```

  Just returns the current bump pointer as a “mark”.

* `__ada_ss_release`:

  ```c
  fprintf(o,
    "define linkonce_odr void @__ada_ss_release(i64 %%m){"
    "store i64 %%m,ptr @__ss_ptr\n"
    "ret void}\n");
  ```

  Restores a previous mark (no checks).

* `__ada_ss_allocate` (assembled from the strings you emit):

  Roughly:

  ```llvm
  define linkonce_odr ptr @__ada_ss_allocate(i64 %sz) {
  entry:
    %0 = load ptr, ptr @__ss_base
    %1 = icmp eq ptr %0, null
    br i1 %1, label %init, label %after_init

  init:
    call void @__ada_ss_init()
    br label %after_init

  after_init:
    %4 = load i64, ptr @__ss_ptr
    %5 = add i64 %sz, 7
    %6 = and i64 %5, -8              ; align up to 8
    %7 = add i64 %4, %6
    %8 = load i64, ptr @__ss_size
    %9 = icmp ule i64 %7, %8
    br i1 %9, label %alloc, label %overflow

  alloc:
    %base = load ptr, ptr @__ss_base
    %p    = getelementptr i8, ptr %base, i64 %4
    store i64 %7, ptr @__ss_ptr
    ret ptr %p

  overflow:
    call void @__ada_raise(ptr @.ex.LAYOUT_ERROR)
    unreachable
  }
  ```

So: **single global secondary stack**, fixed size, bump-pointer allocator with `mark` / `release` and a `LAYOUT_ERROR` exception on overflow.

I don’t see the call sites here (most likely elided), but the runtime surface is clear.

---

### 1.2 Access types in this file

Types:

* `Tk_` enum has `TY_AC` for access types:

  ```c
  enum {
    TY_V=0, TY_I, TY_B, TY_C, TY_F, TY_E,
    TY_A, TY_R, TY_AC, TY_T, TY_S, TY_P,
    TY_UI, TY_UF, TY_D, TY_PT, TY_FT, TY_FX
  } Tk_;
  ```

* `struct Ty` has (among other fields) `Ty *el;` for designated type, and `Ty *prt;` etc. `TY_AC` uses `el` as the designated type.

Type compatibility `tco` includes:

```c
static bool tco(Ty *a, Ty *b){
    if (!a || !b) return false;
    if (a == b)   return true;
    ...
    if (a->k == TY_AC && b->k == TY_AC)
        return tco(a->el, b->el);
    ...
}
```

so two access types are compatible iff their designated types are.

Value-kind mapping `tk2v`:

```c
static Vk tk2v(Ty *t){
    if (!t) return VK_I;
    t = tcc(t);
    switch (t->k) {
    ...
    case TY_S:  return VK_P;   // string-like → pointer
    case TY_AC: return VK_I;   // access type -> integer(!)
    default:    return VK_I;
    }
}
```

So:

* **Access types are represented as `i64` integers** at the IR level (not as `ptr`).
* String-ish `TY_S` gets a real pointer (`VK_P`).

Allocator (`N_ALC` in codegen):

From the visible chunk:

```c
case N_ALC: {
    r.k = VK_P;                      // allocator returns pointer
    Ty *et = n->ty && n->ty->el ? tcc(n->ty->el) : 0;
    int asz = 64;
    if (et && et->dc.n > 0) asz += et->dc.n * 8;
    fprintf(o,
      "  %%t%d = call ptr @malloc(i64 %u)\n", r.id, asz);

    if (n->alc.in) {
        V v = gex(g, n->alc.in);
        v = vcast(g, v, VK_I);
        int op = nt(g);
        /* ... getelementptr into header/data and store 'v' ... */
    }
    break;
}
```

Dereference `N_DRF` in codegen (reconstructed from the snippet):

```c
case N_DRF: {
    V p = gex(g, n->drf.x);
    if (p.k == VK_I) {            // access value stored as integer
        int pp = nt(g);
        fprintf(o,
          "  %%t%d = inttoptr i64 %%t%d to ptr\n", pp, p.id);
        p.id = pp;
        p.k  = VK_P;
    }
    Ty *dt = n->drf.x->ty ? tcc(n->drf.x->ty) : 0;
    dt = dt && dt->el ? tcc(dt->el) : 0;
    /* then load/store using dt */
    ...
}
```

So:

* **IR representation**: access objects are `i64` values; they’re only turned into `ptr` with `inttoptr` when dereferenced.
* Allocation for `new` uses **plain `malloc`**, not the secondary stack.
* Separately, the compiler defines a secondary stack runtime API, probably used for temporaries / unconstrained results, not ordinary `new`.

---

## 2. Pathologies / risks I’d worry about

### 2.1 Secondary stack is global and not per-task

The IR for `__ss_base`, `__ss_size`, `__ss_ptr` is all in the global scope; there’s no per-task or per-thread indirection in this file.

Given you also map Ada tasks to `pthread_t` (from previous passes through this file), it’s very likely that:

* All tasks share the *same* secondary stack.
* `__ada_ss_mark/release/allocate` operate on shared globals.

That’s a problem:

* **Concurrency:** if two tasks call `__ada_ss_allocate` concurrently, they race on `__ss_ptr` with no locking.
* **Isolation:** one task’s `__ada_ss_release` can clobber another’s allocations.
* **Ada semantics:** each task has its own call stack; the “secondary stack” is expected to behave like another stack with *task-local* lifetimes, not a global arena.

**Implication:** unless you *only* use it in single-task programs, the current design is at best racy, at worst wildly wrong.

---

### 2.2 Mark/release isn’t wired to call boundaries (from what we can see)

You provide:

* `i64 mark = __ada_ss_mark()`
* `__ada_ss_release(mark)`

But in this file I don’t see (in the visible areas) any systematic pattern like:

* “on function entry: `mark = __ada_ss_mark()`”
* “on every exit path: `__ada_ss_release(mark)`”

Without that, you can easily:

* Allocate on the secondary stack inside a subprogram.
* Return from the subprogram without releasing, slowly leaking the secondary stack region.
* Or, worse, call `__ada_ss_release` in a caller that never marked, clobbering siblings.

Ada 83’s expectation for things like unconstrained function results is:

* Objects with lifetime “until the caller finishes” should be allocated in some caller-associated stack frame.
* Mark/release is the natural implementation, but it must be tied to call boundaries, not sprinkled ad-hoc.

Right now, the API is there, but this file doesn’t show a consistent strategy.

---

### 2.3 Access types as raw integers (VK_I)

Representing access values as `i64` has a few concrete issues:

1. **Pointer size assumptions**

   * You’re effectively assuming `sizeof(void*) == 8`.
   * On a 32-bit target, this is wrong; even on 64-bit, you’re tying yourself to the C ABI.

2. **Loss of IR-level type information**

   * LLVM sees an `i64` and later an `inttoptr`. It has no idea that some `i64` values are pointers:

     * alias analysis becomes weaker,
     * some sanitizers / safety passes won’t recognize these as pointers.

3. **More opportunities for nonsense operations**

   * Nothing prevents someone (or some future optimization) from doing integer arithmetic on access values in IR, because they’re just `i64`.
   * Ada does allow `System.Address`-ish fiddling, but regular access-to-T should be more strongly typed.

Ada’s model is conceptually: access types are **pointers to T**, not arbitrary integers.

---

### 2.4 Access + secondary stack interaction

Given:

* You have **access types** (TY_AC), with a normal `malloc`-based allocator.
* You also have a **secondary stack**.

Two dangerous patterns immediately pop up:

1. **Access values pointing into the secondary stack**
   If you ever:

   * Allocate some object on the secondary stack, and
   * Take an access value designating it (or treat it as aliased),
   * Then release the mark while that access still exists,

   you’ve created a dangling reference. Ada forbids many of these cases at the language level (e.g. access-to-variable of non-aliased local), but your implementation has to enforce that your *own* stack lifetimes don’t violate legal code.

2. **Unconstrained results vs heap vs secondary stack**
   For an unconstrained array/function result, the usual implementation options are:

   * Allocate on a secondary stack frame owned by the caller.
   * Heap-allocate and have the caller deallocate, possibly via a storage pool.

   Mixing these with a single global secondary stack and a simple `malloc` for `new` makes it hard to keep the lifetimes coherent. You must be certain that:

   * Anything you put on the secondary stack never escapes into a general access value.
   * Anything you allocate via `new` has the right lifetime and doesn’t depend on secondary stack marks.

The file as seen doesn’t enforce those distinctions; it just exposes both mechanisms.

---

### 2.5 Secondary stack size & error handling

* Hard-coded size: `malloc(1048576)` inside `__ada_ss_init`.
* On overflow: `__ada_raise(LAYOUT_ERROR)`.

Issues:

* 1 MiB may be tiny for realistic unconstrained objects.
* There’s no fallback to heap; either you fit or you get an exception.
* `LAYOUT_ERROR` is much more like a **Storage_Error**/storage pool issue than a subtype/layout problem; mapping it that way is arguable.

Not fatal, but worth tightening.

---

## 3. Concrete suggestions for this file

### 3.1 Make the secondary stack per-task

Given you already have a `TK` task kernel and use `pthread_t`, I’d change the IR you emit for the secondary stack globals to be *per-task*, not truly global:

**Option A: per-task struct + TLS pointer**

* Have a runtime struct:

  ```c
  struct ss_state { void *base; int64_t size; int64_t ptr; };
  ```

* In this file, instead of:

  ```llvm
  @__ss_base = global ptr null
  @__ss_size = global i64 0
  @__ss_ptr  = global i64 0
  ```

  emit:

  ```llvm
  @__ss_current = thread_local global ptr null ; -> struct ss_state*
  ```

* Rewrite `__ada_ss_init`, `mark`, `allocate`, `release` to:

  * Look up `@__ss_current` (per thread/task).
  * Allocate the state and initial buffer per thread.

This is all still pure IR printing in this file; the runtime C just needs to set `__ss_current` for each task’s thread.

**Option B: per-task field in TK**

* Keep the globals in the runtime C, but from this file don’t render them at all.

* Instead, have runtime functions:

  ```c
  struct ss_state *ada_get_ss_state(void);  // uses TLS or TK
  ptr __ada_ss_allocate(struct ss_state*, i64 sz);
  i64 __ada_ss_mark(struct ss_state*);
  void __ada_ss_release(struct ss_state*, i64);
  ```

* In this file, only emit calls like:

  ```llvm
  %s = call ptr @ada_get_ss_state()
  %p = call ptr @__ada_ss_allocate(ptr %s, i64 %sz)
  ```

Either way, the key is: **don’t let one task stomp on another’s secondary stack**.

---

### 3.2 Tie mark/release to subprogram prologue/epilogue

In this file you already compute an environment frame (`__frame = alloca [...]`) based on symbol `el` levels.

Do something similar for the secondary stack:

* In subprogram codegen (for any subprogram that might need the secondary stack):

  ```c
  // Prologue:
  int ss_mark = nt(g);
  fprintf(o,
    "  %%t%d = call i64 @__ada_ss_mark()\n", ss_mark);
  ```

* For each `return` or exit path:

  ```c
  fprintf(o,
    "  call void @__ada_ss_release(i64 %%t%d)\n", ss_mark);
  ```

That ensures:

* All allocations done during that call are released on exit.
* Nested calls get nested marks (LIFO), which fits the bump-pointer design.

You can optimize by only doing this if your earlier passes mark the subprogram as “needs secondary stack” (e.g. it returns unconstrained objects, or creates secondary-stack aggregates).

---

### 3.3 Use secondary stack only for *non-aliased, non-escapable* objects

In the front-end / resolution (same file), add a flag on `Ty` or `No`:

* e.g. `bool secstack_ok;` or a bit in `sup`.

Set it for:

* Temporaries and function results of unconstrained types whose lifetime is syntactically limited to the current call and which:

  * are not `aliased`,
  * are not designated by access types,
  * are not components of some object whose access value might escape.

Then in codegen:

* For those objects, instead of:

  ```llvm
  %p = alloca <type>         ; on C stack
  ```

  or `malloc`, emit:

  ```llvm
  %sz = <compute size in bytes>
  %p  = call ptr @__ada_ss_allocate(i64 %sz)
  ```

* Don’t feed those pointers into `TY_AC` values; treat them as anonymous temporaries.

This keeps secondary stack lifetimes and access-type semantics from colliding.

---

### 3.4 Represent access types as pointers at the IR level

In `tk2v`:

```c
case TY_AC: return VK_I;
```

I would change that to:

```c
case TY_AC: return VK_P;
```

and then simplify `N_DRF`:

* Drop the `inttoptr` dance; accesses are already `ptr`s in LLVM.
* You only need casts for “access all” / `System.Address` style fiddling, which you can handle explicitly.

That gives you:

* Better alias analysis.
* Correctness on platforms where pointer size != 64 bits.
* Cleaner IR for things like `null` access (just a `null` pointer).

If you *want* to preserve the idea that `System.Address` is an integer, treat that as its own special type (`TY_ADR`?) and keep the integer representation there, but not for ordinary `access T`.

---

### 3.5 Keep `new` on the heap unless you can prove stack lifetime

Your allocator for `N_ALC` already uses `malloc`:

```c
fprintf(o, "  %%t%d = call ptr @malloc(i64 %u)\n", r.id, asz);
```

That’s perfectly fine for general access types.

To integrate with the secondary stack safely:

* *Only* route an allocator through `__ada_ss_allocate` if:

  * The access type is **local and limited** in scope.
  * You can statically guarantee its value doesn’t escape the lifetime of the mark:

    * no assignment into longer-lived access objects,
    * no return,
    * no storing in global / heap objects.

If you can’t prove that easily, keep `new` on the heap and use the secondary stack strictly for anonymous temporaries and unconstrained results.

---

### 3.6 Make secondary stack size configurable / extensible

Instead of hard-coding `1048576` in the IR:

* Emit a reference to a global parameter, e.g.:

  ```llvm
  @__ss_default_size = global i64 1048576
  ```

* In `__ada_ss_init`:

  ```llvm
  %sz = load i64, ptr @__ss_default_size
  %p  = call ptr @malloc(i64 %sz)
  store i64 %sz, ptr @__ss_size
  ```

That lets you later:

* Override the size from the runtime C, or
* Provide a configuration pragma (`pragma Secondary_Stack_Size`) and have the compiler fill in the constant.

Not critical for semantics, but much nicer in practice.

---

## 4. Summary

From this file:

* You *do* have a simple secondary stack runtime (`__ada_ss_*`), but it’s currently global and not obviously tied to call boundaries.
* Access types (`TY_AC`) are represented as raw `i64` and `new` uses `malloc`.
* The intersection of “global secondary stack” + “access types as plain integers” is a big potential source of dangling references and races.

If you:

1. Make the secondary stack per-task and scoped via mark/release at subprogram prologue/epilogue,
2. Use it only for non-escaping temporaries,
3. Represent `TY_AC` as proper pointers in IR, and
4. Keep `new` on the heap unless you can prove stack-bounded lifetime,

you’ll have a much saner story for both secondary stack management *and* Ada access type semantics, without huge surgery to this file.
