# Makefile for Ada83 Compiler and Runtime

CC = gcc
CFLAGS = -O2 -Wall
RUNTIME_CFLAGS = -O2 -mcmodel=large -Wall
LLC = llc
ADA83 = ./ada83

# Runtime components (automatically generated from embedded runtime)
RUNTIME_OBJ = ada_runtime.o
RUNTIME_LIBS = -lm

.PHONY: all clean runtime compiler help

all: compiler runtime

compiler: ada83

runtime: $(RUNTIME_OBJ)

# Build the compiler
ada83: ada83.c
	$(CC) $(CFLAGS) -o ada83 ada83.c -lm

# Generate and build runtime from embedded source
ada_runtime.c: ada83
	$(ADA83) --emit-runtime

ada_runtime.o: ada_runtime.c
	$(CC) $(RUNTIME_CFLAGS) -c ada_runtime.c -o ada_runtime.o

# Pattern rules for compiling Ada programs
%.ll: %.adb $(ADA83)
	$(ADA83) $< > $@

%.s: %.ll
	$(LLC) $< -o $@

%.exe: %.s $(RUNTIME_OBJ)
	$(CC) $< $(RUNTIME_OBJ) $(RUNTIME_LIBS) -o $@

# Clean build artifacts (NOT source files!)
clean:
	rm -f ada83 ada_runtime.c *.o *.ll *.s *.exe
	rm -f a.out core
	@echo "Clean complete"

# Help target
help:
	@echo "Ada83 Compiler Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build compiler and runtime"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make <prog>.exe   - Compile Ada program <prog>.adb to executable"
	@echo ""
	@echo "Examples:"
	@echo "  make program.exe   # Compile program.adb to executable"
	@echo "  ./ada83 --emit-runtime [file.c]  # Emit runtime source"

.SECONDARY:  # Keep intermediate files
