# Makefile for Ada83 Compiler

CC = gcc
CFLAGS = -O3 -Wall
LLC = llc
ADA83 = ./ada83

# Emitted programs need libm and, for tasking, pthreads. A Windows host
# additionally needs Synchronization.lib — the specialized rendezvous wait
# parks with WaitOnAddress/WakeByAddressAll. (MSVC-style linkers pull it
# automatically from the IR's embedded /DEFAULTLIB directive; the explicit
# flag covers MinGW's GNU ld, which ignores that directive.)
RUNTIME_LIBS = -lm -lpthread
ifeq ($(OS),Windows_NT)
RUNTIME_LIBS += -lsynchronization
endif

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
