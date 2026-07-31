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

# Which platform `make package` packages for: the host, unless TARGET names
# another, in which case the archive is cross-built with Zig, which carries a C
# compiler and every target's libraries in the one program:
#
#     make package TARGET=windows PACKAGE=/tmp/bin-windows.zip
#
# Naming the archive keeps a trial build out of the tree. Nothing here fetches
# a toolchain: a cross build wants zig on PATH, a macOS one lipo as well, and
# says so and stops when they are missing. A Windows host packages with
# make.bat, so windows here is always a cross build.
HOST_TARGET := $(if $(filter Darwin,$(HOST_SYSTEM)),macos,linux)
TARGET      ?= $(HOST_TARGET)
PACKAGE      = bin-$(TARGET).zip

# These archives are meant to run on machines other than the one that built
# them, so none of them is tuned for the host: the macOS one is both slices
# lipo'd together, the Linux and Windows ones are baseline x86_64. -g0 is asked
# for by name because Zig's driver emits debug information where GCC does not,
# and an archive should not grow fivefold with the machine that packed it.
ifeq ($(TARGET),linux)
# Baseline x86_64, and gnu17 rather than gnu2x. Under gnu2x glibc redirects
# sscanf and strtoul to their __isoc23_ variants, which raises the oldest glibc
# the archive will load on from 2.34 to 2.38. The two differ only in accepting
# 0b literals when the base is zero, which no call site here asks for, so the
# compiler behaves identically either way. `make` itself still uses gnu2x. Zig
# is told both in the one word: the baseline and that floor ride in the triple.
PACKAGE_CFLAGS         = -O3 -Wall -g0 -std=gnu17
PACKAGE_BASELINE_FLAGS = -march=x86-64 -mtune=generic
PACKAGE_TRIPLE         = x86_64-linux-gnu.2.34
else ifeq ($(TARGET),macos)
# One compile per slice, joined into a universal executable by lipo -- Apple's,
# or LLVM's llvm-lipo, which the LLVM packages sometimes leave off PATH. Apple
# spells the 64-bit ARM architecture arm64 and LLVM spells it aarch64, which is
# why the triples are not the architectures with -macos on the end.
PACKAGE_CFLAGS            = -O3 -Wall -g0 -std=gnu2x
PACKAGE_ARCHITECTURE      = arm64 x86_64
PACKAGE_ARCHITECTURE_FLAG = -arch
PACKAGE_TRIPLE            = aarch64-macos x86_64-macos
LIPO := $(shell command -v lipo || command -v llvm-lipo || \
                command -v "$$(llvm-config --bindir 2>/dev/null)/llvm-lipo")
else ifeq ($(TARGET),windows)
# Optimised the way make.bat optimises it, so that the archive holds the same
# compiler however it was packed. LLVM-C.dll and its companions are what
# ada83.exe loads when it runs; they are neither sources nor built here, so a
# repack lifts them out of the published archive, as make.bat's unpacking step
# does.
PACKAGE_CFLAGS    = -O2 -Wall -g0 -std=gnu2x
PACKAGE_TRIPLE    = x86_64-windows-gnu
PACKAGE_LIBRARIES = *.dll
else
$(error TARGET is '$(TARGET)'; it must be linux, macos or windows)
endif

# A slice is built by the host compiler when the target is the host and by Zig
# otherwise, and the two name one differently: an architecture against a whole
# target triple. The host compiler's flags do not cross either, -fwhole-program
# being GCC's alone and the baseline riding in Zig's triple.
ifeq ($(TARGET),$(HOST_TARGET))
PACKAGE_COMPILER   = $(CC)
PACKAGE_SLICE      = $(PACKAGE_ARCHITECTURE)
PACKAGE_SLICE_FLAG = $(PACKAGE_ARCHITECTURE_FLAG)
PACKAGE_CFLAGS    += $(WHOLE_PROGRAM) $(PACKAGE_BASELINE_FLAGS)
else
PACKAGE_COMPILER   = zig cc
PACKAGE_SLICE      = $(PACKAGE_TRIPLE)
PACKAGE_SLICE_FLAG = -target
endif

RUNTIME = ada83-runtime.ada

# The extension is one file: the manifest, the grammar, the language
# configuration, the snippets and the two packaging files ride in
# ada83-extension.js as line comments, which is the only container a payload
# holding both */ and $\{ cannot break out of.  Splitting them back out gives
# the layout VS Code installs.
VSIX   = ada83.vsix
BUNDLE = ada83-extension.js

# The reference manual travels in the vsix beside the extension, which is
# where the search_ada83_manual tool looks for it: a question of Ada 83
# legality is then answered out of the standard rather than out of a memory
# of some later Ada.
MANUAL = manual.md

# Windows spells the executable with an extension and takes its threads from
# the C runtime. The rest of an archive is the same file whichever machine
# packed it, the extension included: awk and zip split that one on the host.
PACKAGE_EXECUTABLE = ada83$(if $(filter windows,$(TARGET)),.exe)
PACKAGE_LIBS       = -lm $(if $(filter windows,$(TARGET)),,-lpthread)
PACKAGE_CONTENTS   = $(PACKAGE_EXECUTABLE) $(RUNTIME) $(VSIX) $(PACKAGE_LIBRARIES)

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
ifneq ($(TARGET),$(HOST_TARGET))
	@command -v zig >/dev/null || { \
	  echo "packaging for $(TARGET) on $(HOST_TARGET) is a cross build, which"; \
	  echo "needs zig on PATH; install it from https://ziglang.org"; exit 1; }
endif
	rm -rf staging && mkdir staging
ifeq ($(TARGET),macos)
	@test -n "$(LIPO)" || { \
	  echo "lipo is needed to join the macOS slices;"; \
	  echo "install LLVM, which provides llvm-lipo"; exit 1; }
	@for slice in $(PACKAGE_SLICE); do \
	  echo "Compiling for $$slice..."; \
	  $(PACKAGE_COMPILER) $(PACKAGE_CFLAGS) $(PACKAGE_SLICE_FLAG) $$slice \
	    -o staging/ada83-$$slice ada83.c $(PACKAGE_LIBS) || exit 1; \
	done
	$(LIPO) -create -output staging/$(PACKAGE_EXECUTABLE) \
	  $(addprefix staging/ada83-,$(PACKAGE_SLICE))
	rm -f $(addprefix staging/ada83-,$(PACKAGE_SLICE))
else
	$(PACKAGE_COMPILER) $(PACKAGE_CFLAGS) $(PACKAGE_SLICE_FLAG) $(PACKAGE_SLICE) \
	  -o staging/$(PACKAGE_EXECUTABLE) ada83.c $(PACKAGE_LIBS)
endif
	cp $(RUNTIME) staging/
	$(MAKE) --no-print-directory staging/$(VSIX)
ifeq ($(TARGET),windows)
	unzip -qoj bin-windows.zip '$(PACKAGE_LIBRARIES)' -d staging
endif
# The archive is written beside the one it replaces and moved over it only once
# it is whole, since a Windows repack lifted its DLLs out of that older one.
	rm -f $@.new
	cd staging && zip -q $(abspath $@).new $(PACKAGE_CONTENTS)
	mv $@.new $@
	rm -rf staging
	@echo "Packaged $@:"; unzip -l $@ | tail -n +4

# The extension, in the shape VS Code installs: `code --install-extension
# ada83.vsix`, or unzip its extension/ directory into ~/.vscode/extensions.
vsix:
	@command -v zip >/dev/null || { echo "zip is needed to package"; exit 1; }
	rm -rf staging && mkdir -p staging
	$(MAKE) --no-print-directory staging/$(VSIX)
	@echo "Built staging/$(VSIX):"; unzip -l staging/$(VSIX) | tail -n +4

# awk splits the bundle: //== names the next file, //= is commentary, and
# every other line comment is that file's content with the // taken off.
staging/$(VSIX): $(BUNDLE)
	rm -rf staging/vsix
	mkdir -p staging/vsix/extension/syntaxes
	cp $(BUNDLE) staging/vsix/extension/
	@test -f $(MANUAL) && cp $(MANUAL) staging/vsix/extension/ \
	  || echo "$(MANUAL) is missing; packaging without the manual search tool"
	awk 'BEGIN { out = "" } \
	     /^\/\/== end$$/       { out = ""; next } \
	     /^\/\/== /            { name = substr ($$0, 6); \
	                              out = (name ~ /^(extension.vsixmanifest|\[)/) \
	                                    ? "staging/vsix/" name \
	                                    : "staging/vsix/extension/" name; \
	                              printf "" > out; next } \
	     /^\/\/= /             { next } \
	     out != "" && /^\/\// { print substr ($$0, 3) >> out }' $(BUNDLE)
	cd staging/vsix && zip -qr ../$(VSIX) .
	rm -rf staging/vsix

clean:
	rm -f ada83 ada83.vsix *.o *.ll *.s *.exe a.out core
	rm -rf staging test_results acats_logs acats/report.ll

clean-test:
	rm -rf test_results acats_logs acats/report.ll

.PHONY: all package vsix provision-llvm clean clean-test
