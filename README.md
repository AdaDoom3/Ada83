# Ada83 Compiler

**Self-contained Ada 1983 compiler targeting LLVM IR**

A complete, single-file implementation of the Ada 1983 standard with embedded runtime library.

## Features

- **Complete Ada 83 support**: Full language implementation including generics, tasking, and representation clauses
- **LLVM IR target**: Direct code generation to LLVM intermediate representation
- **Embedded runtime**: All runtime support (secondary stack, exceptions, TEXT_IO) embedded in compiler binary
- **Self-contained**: Single `ada83.c` source file + generated runtime
- **DIANA-inspired architecture**: Clean separation of structural and semantic analysis

## Architecture

### DIANA Principles Applied

This compiler follows design principles from the DIANA (Descriptive Intermediate Attributed Notation for Ada) reference manual:

1. **Static Semantic Information Only**
   - AST contains only statically-determined information
   - No dynamic analysis, optimization artifacts, or runtime values
   - Exception: Static expressions ARE evaluated (Ada 83 requirement)

2. **Structural vs Semantic Attributes**
   - **Structural**: Preserve source program structure (node kind, location, syntactic components)
   - **Semantic**: Results of analysis (resolved types, symbol cross-references)
   - Clean separation enables independent traversals and tool building

3. **Easy to Recompute = Omit**
   - Information requiring ≤3-4 node visits: recomputed on demand
   - Single-pass computable: not stored
   - Examples omitted: parent pointers, enclosing scope, nesting depth

4. **Representation Independence**
   - Platform-neutral type system
   - Abstract source positions
   - Target-independent semantic model

### Compilation Pipeline

```
Ada Source (.adb)
    ↓
  Lexer (character → tokens)
    ↓
  Parser (tokens → AST)
    ↓
  Semantic Analysis (symbol resolution, type checking)
    ↓
  Code Generator (AST → LLVM IR)
    ↓
  LLVM (IR → assembly)
    ↓
  Linker (+ runtime → executable)
```

### Key Data Structures

- **Syntax_Node**: AST nodes with structural (syntax) and semantic (types, symbols) attributes
- **Type_Info**: Complete type representation following Ada's type model
- **Symbol**: Symbol table entries for defining occurrences (declarations)
- **Symbol_Manager**: Scope-aware symbol table with hash-based lookup

## Building

```bash
make                 # Build compiler and runtime
./ada83 --help       # Show usage
```

## Usage

### Compile an Ada program:
```bash
./ada83 program.adb > program.ll      # Generate LLVM IR
llc program.ll -o program.s            # Assemble
gcc program.s ada_runtime.o -lm -o program.exe  # Link
./program.exe                          # Run
```

### Or use Makefile:
```bash
make program.exe   # Does all steps automatically
```

### Emit runtime source:
```bash
./ada83 --emit-runtime [file.c]   # Extract runtime C code
```

## Runtime Library

The compiler includes a complete Ada runtime supporting:

- **Secondary Stack**: 64KB initial, auto-grows to 16MB for unconstrained returns
- **Exception Handling**: setjmp/longjmp based with handler stack
- **Range Checking**: CONSTRAINT_ERROR on violations
- **TEXT_IO Package**: PUT, PUT_LINE, NEW_LINE for integers/floats/strings
- **Arithmetic**: Integer power, floating point operations
- **Finalization**: Controlled types and automatic cleanup

## Implementation Notes

### Fat Pointers
Unconstrained arrays use fat pointers: `{data_ptr, bounds_ptr}`

### Fixed Lower Bound (FLB) Optimization
Arrays with constant lower bound store only upper bound (8 bytes saved per array)

### Secondary Stack
Function returns of unconstrained types use secondary stack allocation

### Type Model
- Base types vs subtypes properly distinguished
- Derived types maintain parent type links
- Universal types (universal_integer, universal_real) handled
- Constrained vs unconstrained arrays tracked via bounds

## Testing

The `acats/` directory contains the Ada Conformity Assessment Test Suite tests for validation.

## Reference Materials

- `reference/Ada83_LRM.md` - Complete Ada 83 Language Reference Manual
- `reference/DIANA.md` - DIANA intermediate representation guide
- `reference/gnat/` - GNAT compiler sources for reference

## Design Philosophy

This compiler prioritizes:
1. **Correctness**: Strict Ada 83 semantics
2. **Clarity**: Clean architecture following DIANA principles
3. **Completeness**: Full language support, not a subset
4. **Simplicity**: Single-file implementation, no external dependencies
5. **Efficiency**: Direct LLVM IR generation, optimizations via LLVM

## License

See LICENSE file for details.
