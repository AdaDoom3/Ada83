# Generic Package Test Plan

## Test Cases (Progressive Complexity)

### T1: Simple generic package spec only
```ada
GENERIC
PACKAGE PKG IS
  X : INTEGER := 1;
END PKG;
```
Expected: Compile, no code gen (template only)

### T2: Generic package with body, no instantiation
```ada
GENERIC
PACKAGE PKG IS
  X : INTEGER := 1;
END PKG;

PACKAGE BODY PKG IS
BEGIN
  X := 2;
END PKG;
```
Expected: Compile, no code gen (body is template)
Current: Incorrectly generates elaboration code

### T3: Generic package instantiation
```ada
GENERIC
PACKAGE PKG IS
  X : INTEGER := 1;
END PKG;

PACKAGE BODY PKG IS
BEGIN
  X := 2;
END PKG;

PACKAGE INST IS NEW PKG;
```
Expected: Generate code for INST elaboration
Current: Unknown

### T4: Multiple generic packages (c23006d scenario)
```ada
GENERIC
PACKAGE PKG1 IS
  A : INTEGER := 1;
END PKG1;

GENERIC
PACKAGE PKG2 IS
  D : INTEGER := 2;
END PKG2;

PACKAGE BODY PKG2 IS
BEGIN
  D := 5;
END PKG2;
```
Expected: Compile both templates
Current: Fails with "dup 'A'"

### T5: Generic with formal parameters
```ada
GENERIC
  TYPE T IS PRIVATE;
PACKAGE PKG IS
  X : T;
END PKG;
```

## Root Cause Analysis

**Problem**: Generic bodies are being processed as regular packages, creating:
1. Symbols in global scope (causes conflicts)
2. Elaboration code (incorrect - templates don't elaborate)
3. Template AST mutation (sy fields set on shared nodes)

**Ada Semantics**:
- Generic specs/bodies are TEMPLATES (like C++ templates)
- No code generation until instantiation
- No elaboration until instantiation
- Body visibility rules still apply (body sees spec declarations)

## Design: Comprehensive Fix

### Phase 1: Template Mode Flag
Add SM->in_template flag to track template processing

### Phase 2: Modify gcl()
- Set gs->df for generic packages (already done)
- Do NOT set sy fields on template nodes

### Phase 3: Modify N_PKB
- If gt != NULL (generic body):
  - Skip symbol creation
  - Skip code generation
  - Skip elaboration code
  - Only validate syntax/types if needed
- Non-generic bodies: normal processing

### Phase 4: Modify N_GINST
- Clone template AST (already done)
- Process cloned body with full semantics
- Generate instantiation code

## Implementation Strategy

1. Create test harness
2. Implement template mode
3. Fix N_PKB to skip generic body processing
4. Verify instantiation still works
5. Test progressive complexity
