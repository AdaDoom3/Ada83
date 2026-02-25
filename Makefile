# Makefile for Ada83 Compiler

CC = gcc
CFLAGS = -O2 -Wall
LLC = llc
ADA83 = ./ada83

RUNTIME_LIBS = -lm

.PHONY: all clean clean-test compiler help

all: compiler

compiler: ada83

# Build the compiler
ada83: ada83.c
	$(CC) $(CFLAGS) -o ada83 ada83.c -lm -lpthread -march=native

# Pattern rules for compiling Ada programs
%.ll: %.adb $(ADA83)
	$(ADA83) $< > $@

%.s: %.ll
	$(LLC) $< -o $@

%.exe: %.s
	$(CC) $< $(RUNTIME_LIBS) -o $@

# Clean build artifacts (NOT source files!)
clean:
	rm -f ada83 *.o *.ll *.s *.exe
	rm -f a.out core
	rm -rf test_results acats_logs acats/report.ll
	@echo "Clean complete"

# Clean only test artifacts (keep compiler)
clean-test:
	rm -rf test_results acats_logs acats/report.ll
	@echo "Test artifacts cleaned"

# Help target
help:
	@echo "Ada83 Compiler Makefile"
	@echo ""
	@echo "Targets:"
	@echo "  make              - Build compiler"
	@echo "  make clean        - Clean build artifacts"
	@echo "  make <prog>.exe   - Compile Ada program <prog>.adb to executable"
	@echo ""
	@echo "Examples:"
	@echo "  make program.exe   # Compile program.adb to executable"

.SECONDARY:  # Keep intermediate files
