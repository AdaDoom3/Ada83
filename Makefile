
CC = gcc
CFLAGS = -O3 -Wall -std=gnu2x
LIBS = -lm -lpthread

# This makefile builds the Unix-like hosts.  Windows is built by build.bat,
# which unpacks LLVM-C.zip beside the executable instead of relying on a
# system libLLVM.
HOST_SYSTEM  := $(shell uname -s)
HOST_MACHINE := $(shell uname -m)
CC_IS_CLANG  := $(shell echo | $(CC) -dM -E - 2>/dev/null | grep -c __clang__)

# -fwhole-program is a GCC optimisation.  Clang takes the flag only to
# warn that it does nothing, so do not offer it one.
ifeq ($(CC_IS_CLANG),0)
WHOLE_PROGRAM = -fwhole-program
TUNE          = -march=native
else
WHOLE_PROGRAM =
# Clang implements -march=native on x86-64 alone: on AArch64 it rejects
# the argument outright, and the baseline there already carries the NEON
# that §18 compiles against.
ifneq ($(filter x86_64 amd64,$(HOST_MACHINE)),)
TUNE = -march=native
else
TUNE =
endif
endif

all: ada83 provision-llvm

ada83: ada83.c
	$(CC) $(CFLAGS) $(WHOLE_PROGRAM) -o ada83 ada83.c $(LIBS) $(TUNE)

SUDO := $(shell [ $$(id -u) -eq 0 ] || echo sudo)

ifeq ($(HOST_SYSTEM),Darwin)
# macOS has no ldconfig, so look where the loader will: the two Homebrew
# prefixes and the command-line tools.  Homebrew is never run under sudo.
provision-llvm:
	-@if [ -e /opt/homebrew/opt/llvm/lib/libLLVM.dylib ] \
	   || [ -e /usr/local/opt/llvm/lib/libLLVM.dylib ] \
	   || [ -e /Library/Developer/CommandLineTools/usr/lib/libLLVM.dylib ]; then \
	  :; \
	elif command -v brew >/dev/null 2>&1; then \
	  echo "libLLVM not found - installing (brew)"; \
	  brew install llvm; \
	else \
	  echo "libLLVM not found and Homebrew is not installed;"; \
	  echo "install it from https://brew.sh then run 'brew install llvm'"; \
	fi
else
provision-llvm:
	-@if ldconfig -p 2>/dev/null | grep -q libLLVM; then \
	  :; \
	elif command -v apt-get >/dev/null 2>&1; then \
	  echo "libLLVM not found - installing (apt)"; \
	  $(SUDO) apt-get install -y --no-install-recommends llvm; \
	elif command -v dnf >/dev/null 2>&1; then \
	  echo "libLLVM not found - installing (dnf)"; \
	  $(SUDO) dnf install -y llvm-libs; \
	elif command -v pacman >/dev/null 2>&1; then \
	  echo "libLLVM not found - installing (pacman)"; \
	  $(SUDO) pacman -S --noconfirm llvm-libs; \
	elif command -v zypper >/dev/null 2>&1; then \
	  echo "libLLVM not found - installing (zypper)"; \
	  $(SUDO) zypper install -y libLLVM; \
	elif command -v apk >/dev/null 2>&1; then \
	  echo "libLLVM not found - installing (apk)"; \
	  $(SUDO) apk add llvm-libs; \
	else \
	  echo "libLLVM not found and no known package manager;"; \
	  echo "install your distribution's llvm package for --native support"; \
	fi
endif

clean:
	rm -f ada83 *.o *.ll *.s *.exe a.out core
	rm -rf test_results acats_logs acats/report.ll

clean-test:
	rm -rf test_results acats_logs acats/report.ll

.PHONY: all provision-llvm clean clean-test
