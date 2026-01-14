# Ada83 Compiler Runtime Library

This document describes the Ada83 runtime library and build system.

## Runtime Components

### ada_runtime.c
Comprehensive Ada runtime support library implementing:

1. **Secondary Stack** - Dynamic memory management for unconstrained return values
   - `__ada_ss_init()` - Initialize secondary stack
   - `__ada_ss_allocate(size)` - Allocate memory from secondary stack
   - `__ada_ss_mark()` - Mark current stack position
   - `__ada_ss_release(mark)` - Release stack to marked position

2. **Exception Handling** - Ada exception support
   - `__ada_push_handler(env)` - Push exception handler
   - `__ada_pop_handler()` - Pop exception handler
   - `__ada_raise(exception)` - Raise an exception
   - `__ada_setjmp(env)` - Set jump point for exceptions
   - Exception constants: CONSTRAINT_ERROR, PROGRAM_ERROR, STORAGE_ERROR

3. **Range Checking** - Runtime constraint validation
   - `__ada_check_range(value, low, high)` - Check value is in range

4. **Arithmetic** - Extended arithmetic operations
   - `__ada_powi(base, exponent)` - Integer power operation
   - `__ada_powf(base, exponent)` - Floating point power operation

5. **String/Image Operations** - Type conversion to strings
   - `__ada_image_int(value)` - Convert integer to string
   - `__ada_image_enum(value, low, high)` - Convert enum to string
   - `__ada_value_int(str)` - Parse integer from string

6. **Finalization** - Object cleanup support
   - `__ada_register_finalizer(finalizer, object)` - Register finalizer
   - `__ada_finalize(object)` - Finalize specific object
   - `__ada_finalize_all()` - Finalize all registered objects

7. **Task Support** (Stubs)
   - `__ada_task_trampoline()` - Task entry point
   - `__ada_delay(microseconds)` - Delay operation

### print_helper.c
Basic I/O support functions:
- `print_int(n)` - Print integer value
- `print_newline()` - Print newline

## Building

### Prerequisites
- GCC compiler
- LLVM tools (llc)
- Make

### Build Commands

```bash
# Build everything
make all

# Build compiler only
make compiler

# Build runtime only
make runtime

# Compile an Ada program
./ada83 program.adb > program.ll
llc program.ll -o program.s
gcc program.s ada_runtime.o print_helper.o -lm -o program

# Or use make with pattern:
make program.exe  # Compiles program.adb to program.exe
```

## Usage Example

```ada
-- test.adb
procedure TEST is
   procedure PRINT_INT(N : INTEGER);
   pragma IMPORT(C, PRINT_INT, "print_int");

   procedure PRINT_NEWLINE;
   pragma IMPORT(C, PRINT_NEWLINE, "print_newline");

   X : INTEGER;
begin
   X := 42;
   PRINT_INT(X);
   PRINT_NEWLINE;
end TEST;
```

Compile and run:
```bash
./ada83 test.adb > test.ll
llc test.ll -o test.s
gcc test.s ada_runtime.o print_helper.o -lm -o test
./test
```

## Runtime Features Implemented

### Arrays
- ✅ Fixed-size arrays
- ✅ Runtime-sized arrays (bounds determined at runtime)
- ✅ Unconstrained arrays with fat pointers
- ✅ Fixed Lower Bound (FLB) optimization
- ✅ Array indexing with bounds checking
- ✅ Array assignment via memcpy
- ✅ Array slicing
- ✅ Array concatenation

### Functions/Procedures
- ✅ Function calls with parameters
- ✅ Function returns
- ✅ Unconstrained array returns via secondary stack
- ✅ Nested procedures with static links
- ✅ Pragma IMPORT for C interop

### Control Flow
- ✅ If/then/else
- ✅ Case statements
- ✅ For loops
- ✅ While loops
- ✅ Loop exit/next
- ✅ Goto statements

### Types
- ✅ INTEGER (64-bit)
- ✅ BOOLEAN
- ✅ CHARACTER
- ✅ Enumerations
- ✅ Arrays
- ✅ Records
- ✅ Access types (pointers)
- ✅ Subtypes with constraints

### Exception Handling
- ✅ Exception declarations
- ✅ Raise statements
- ✅ Exception handlers (begin/exception/end)
- ✅ Predefined exceptions (CONSTRAINT_ERROR, etc.)

## Known Limitations

1. **Nested Functions** - Some complex nested function scenarios may fail
2. **Tasks** - Task support is stubbed out (not fully implemented)
3. **Generics** - Not yet implemented  
4. **Packages** - Limited support (basic with/use)
5. **Text_IO** - Not yet implemented (use pragma IMPORT for I/O)

## Troubleshooting

### "Cannot allocate memory" error
This can occur with complex programs using nested functions. Try:
- Simplifying the program structure
- Avoiding deeply nested procedures
- Using flat procedurestructure where possible

### Linking errors with runtime
Ensure you're compiling the runtime with:
```bash
gcc -c -mcmodel=large ada_runtime.c -o ada_runtime.o
```

The `-mcmodel=large` is required for the secondary stack's large static data.

## File Structure

```
Ada83/
├── ada83.c              # Compiler source
├── ada_runtime.c        # Runtime library
├── print_helper.c       # I/O helpers
├── Makefile             # Build system
├── test_*.adb           # Test programs
└── README_RUNTIME.md    # This file
```

## Contributing

When adding runtime functions:
1. Declare in ada_runtime.c
2. Ensure proper error handling (raise exceptions, don't crash)
3. Use secondary stack for dynamic allocations
4. Follow Ada semantics precisely
