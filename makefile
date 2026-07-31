CC = gcc
CFLAGS = -O3 -Wall -std=gnu2x
LIBS = -lm -lpthread

HOST_SYSTEM  := $(shell uname -s)
HOST_MACHINE := $(shell uname -m)
CC_IS_CLANG  := $(shell echo | $(CC) -dM -E - 2>/dev/null | grep -c __clang__)

ifeq ($(CC_IS_CLANG),0)
WHOLE_PROGRAM = -fwhole-program
TUNE          = -march=native
else
WHOLE_PROGRAM =
ifneq ($(filter x86_64 amd64,$(HOST_MACHINE)),)
TUNE = -march=native
else
TUNE =
endif
endif

# What `make package` produces. These archives are meant to run on machines
# other than the one that built them, so neither is tuned for the host: the
# macOS one is both slices lipo'd together, the Linux one is baseline x86_64.
ifeq ($(HOST_SYSTEM),Darwin)
PACKAGE       = bin-macos.zip
PACKAGE_SLICE = arm64 x86_64
else
PACKAGE       = bin-linux.zip
PACKAGE_SLICE =
# Baseline x86_64, and gnu17 rather than gnu2x. Under gnu2x glibc redirects
# sscanf and strtoul to their __isoc23_ variants, which raises the oldest glibc
# the archive will load on from 2.34 to 2.38. The two differ only in accepting
# 0b literals when the base is zero, which no call site here asks for, so the
# compiler behaves identically either way. `make` itself still uses gnu2x.
PACKAGE_TUNE  = -march=x86-64 -mtune=generic -std=gnu17
endif

RUNTIME = ada83-runtime.ada

all: ada83 provision-llvm

ada83: ada83.c
	$(CC) $(CFLAGS) $(WHOLE_PROGRAM) -o ada83 ada83.c $(LIBS) $(TUNE)

SUDO := $(shell [ $$(id -u) -eq 0 ] || echo sudo)

ifeq ($(HOST_SYSTEM),Darwin)
# macOS has no ldconfig, so look where the loader will
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

# The distributable: the compiler plus $(RUNTIME), which ada83 looks for beside
# its own executable. Without the runtime in the archive an unpacked compiler
# cannot build anything that withs the standard library.
#
# The compiler is built into staging/ rather than reusing the ada83 target,
# which tunes for the host; an archive compiled with -march=native would fault
# on an older machine than the one that packaged it.
package: $(PACKAGE)

$(PACKAGE): ada83.c $(RUNTIME)
	@command -v zip >/dev/null || { echo "zip is needed to package"; exit 1; }
	rm -rf staging && mkdir staging
ifeq ($(HOST_SYSTEM),Darwin)
	@for slice in $(PACKAGE_SLICE); do \
	  echo "Compiling for $$slice..."; \
	  $(CC) $(CFLAGS) -arch $$slice -o staging/ada83-$$slice ada83.c $(LIBS) \
	    || exit 1; \
	done
	lipo -create -output staging/ada83 \
	  $(addprefix staging/ada83-,$(PACKAGE_SLICE))
	rm -f $(addprefix staging/ada83-,$(PACKAGE_SLICE))
else
	$(CC) -O3 -Wall $(WHOLE_PROGRAM) -o staging/ada83 ada83.c $(LIBS) \
	  $(PACKAGE_TUNE)
endif
	cp $(RUNTIME) staging/
	rm -f $@
	cd staging && zip -q ../$@ ada83 $(RUNTIME)
	rm -rf staging
	@echo "Packaged $@:"; unzip -l $@ | tail -n +4

clean:
	rm -f ada83 *.o *.ll *.s *.exe a.out core
	rm -rf staging test_results acats_logs acats/report.ll

clean-test:
	rm -rf test_results acats_logs acats/report.ll

.PHONY: all package provision-llvm clean clean-test
