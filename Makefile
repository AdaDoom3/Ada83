# Makefile for Ada83 Compiler and Runtime

CC = gcc
CFLAGS = -O2 -Wall
RUNTIME_CFLAGS = -O2 -mcmodel=large -Wall
LLC = llc
ADA83 = ./ada83

# Runtime components
RUNTIME_OBJ = ada_runtime.o print_helper.o
RUNTIME_LIBS = -lm

.PHONY: all clean test runtime compiler help

all: compiler runtime

compiler: ada83

runtime: $(RUNTIME_OBJ)

# Build the compiler
ada83: ada83.c
	$(CC) $(CFLAGS) -o ada83 ada83.c -lm

# Build runtime objects
ada_runtime.o: ada_runtime.c
	$(CC) $(RUNTIME_CFLAGS) -c ada_runtime.c -o ada_runtime.o

print_helper.o: print_helper.c
	$(CC) $(CFLAGS) -c print_helper.c -o print_helper.o

# Pattern rules for compiling Ada programs
%.ll: %.adb $(ADA83)
	$(ADA83) $< > $@

%.s: %.ll
	$(LLC) $< -o $@

%.exe: %.s $(RUNTIME_OBJ)
	$(CC) $< $(RUNTIME_OBJ) $(RUNTIME_LIBS) -o $@

# Test targets
TEST_PROGS = test_simple_output test_runtime_array test_array

test: compiler runtime $(TEST_PROGS)
	@echo "Running test suite..."
	@./test_simple_output && echo "PASS: test_simple_output"
	@./test_runtime_array && echo "PASS: test_runtime_array"
	@./test_array && echo "PASS: test_array"
	@echo "Test suite complete"

# Executable targets
test_simple_output: test_simple_output.exe
	cp $< $@

test_runtime_array: test_runtime_array.exe
	cp $< $@

test_array: test_array.exe
	cp $< $@

# Clean build artifacts (NOT source files!)
clean:
	rm -f ada83 *.o *.ll *.s *.exe
	rm -f test_simple_output test_runtime_array test_array
	rm -f a.out core
	@echo "Clean complete"

# Help target
help:
	@echo "Ada83 Compiler Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build compiler and runtime"
	@echo "  make test         - Run test suite"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make <prog>.exe   - Compile Ada program <prog>.adb to executable"
	@echo ""
	@echo "Examples:"
	@echo "  make test_simple_output.exe   # Compile test_simple_output.adb"

.SECONDARY:  # Keep intermediate files
