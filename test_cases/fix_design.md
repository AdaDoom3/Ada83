# Comprehensive Generic Package Fix

## Baseline Test Results
- T1 (spec only): ✓ PASS
- T2 (body no inst): ✗ FAIL (incorrectly generates elaboration)
- T3 (instantiation): ✗ FAIL (dup 'X' error)
- T4 (multiple generic): ✗ FAIL (incorrectly generates elaboration)
- T5 (with formal): ✗ FAIL (dup 'X' error)

## Root Cause
Generic package bodies are processed as regular packages, causing:
1. Symbol creation in global scope
2. Incorrect elaboration code generation
3. Conflicts during instantiation (dup errors)

## Solution Design

### Key Principle
**Generic bodies are templates - skip semantic processing entirely**

In Ada semantics:
- Generic declarations are templates (no code gen)
- Generic bodies are also templates (no code gen)
- Only instantiations generate code
- Generic bodies DO need syntax checking but NOT symbol creation

### Implementation

#### Step 1: Detect Generic Package Bodies
In N_PKB case:
```c
GT*gt=0;
if(ps&&ps->k==11){gt=ps->gt?ps->gt:gfnd(SM,n->pb.nm);}
```
If `gt != NULL`, this is a generic package body.

#### Step 2: Skip Processing for Generic Bodies
```c
case N_PKB:{
    Sy*ps=syf(SM,n->pb.nm);
    GT*gt=0;
    if(ps&&ps->k==11){
        gt=ps->gt?ps->gt:gfnd(SM,n->pb.nm);
    }

    // If generic body, skip all processing
    if(gt){
        // Just validate it's a valid body structure
        // No symbol creation, no code gen
        return;
    }

    // Normal package body processing for non-generics
    scp(SM);
    // ... rest of existing code
}
```

#### Step 3: Process Bodies During Instantiation
The gcl() function already clones the body. Modify it to process the clone:
```c
else if(n->k==N_GINST){
    GT*g=gfnd(SM,n->gi.gn);
    if(g){
        No*inst=ncs(g->un,0,0);  // Clone template

        // If it's a package, also clone the body if exists
        if(inst->k==N_PKS){
            // Look for corresponding body
            // Clone and process it
        }

        rdl(SM,inst);  // Process instantiation
        return inst;
    }
}
```

#### Step 4: Store Body Reference
Modify gcl() to store body node in GT structure:
```c
typedef struct{
    S nm;
    NV fp;   // formal params
    NV dc;   // declarations
    No*un;   // unit (spec)
    No*bd;   // body node (NEW)
}GT;
```

When processing N_GEN that contains a body:
```c
if(n->k==N_GEN){
    // ... existing spec processing

    // If next node is a body, store it
    // (This requires parser changes, may be complex)
}
```

## Simpler Alternative: Two-Part Fix

### Part A: Skip Generic Body Processing
Just modify N_PKB to skip when gt != NULL

### Part B: Body-aware Instantiation
When instantiating, look up the body separately and process it

This avoids needing to change GT structure or parser.

## Implementation Plan

1. Modify N_PKB to skip generic bodies completely
2. Test T2, T4 (should pass - no elab code)
3. Modify gcl/N_GINST to find and process body during instantiation
4. Test T3, T5 (should pass - instantiation works)
5. Run full ACATS suite

## Code Changes Required

### ada83.c line 173 (N_PKB case)
```c
case N_PKB:{
    Sy*ps=syf(SM,n->pb.nm);
    GT*gt=0;
    if(ps&&ps->k==11){gt=ps->gt?ps->gt:gfnd(SM,n->pb.nm);}

    // NEW: Skip processing for generic bodies
    if(gt){
        // Generic body is a template - defer processing to instantiation
        break;
    }

    scp(SM);
    // ... existing non-generic body processing
}
```

This single change should fix T2 and T4!

For T3 and T5, need to ensure N_GINST processes the body.
