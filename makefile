CC     = gcc
CFLAGS = -O3 -Wall -std=gnu2x
LIBS   = -lm -lpthread

HOST_SYSTEM   := $(shell uname -s)
HOST_MACHINE  := $(shell uname -m)
HOST_IS_MACOS := $(filter Darwin,$(HOST_SYSTEM))
HOST_IS_CLANG := $(shell echo | $(CC) -dM -E - 2>/dev/null | grep -c __clang__)
HOST_IS_GCC   := $(filter 0,$(HOST_IS_CLANG))

WHOLE_PROGRAM := $(if $(HOST_IS_GCC),-fwhole-program)
TUNE          := $(if $(HOST_IS_GCC)$(filter x86_64 amd64,$(HOST_MACHINE)),\
                      -march=native)

RUNTIME = ada83-runtime.ada
MANUAL  = manual.md
VSIX    = ada83.vsix
BUNDLE  = ada83-extension.js
ICON    = ada83-icon

HOST_TARGET := $(if $(HOST_IS_MACOS),macos,linux)
TARGET      ?= $(HOST_TARGET)
CROSS       := $(filter-out $(HOST_TARGET),$(TARGET))

COMPILE_linux     = -O3 -Wall -g0 -std=gnu17
COMPILE_macos     = -O3 -Wall -g0 -std=gnu2x
COMPILE_windows   = -O2 -Wall -g0 -std=gnu2x
COMPILER_linux    = $(if $(CROSS),x86_64-linux-gnu-gcc,$(CC))
COMPILER_macos    = $(if $(CROSS),o64-clang,$(CC))
COMPILER_windows  = x86_64-w64-mingw32-gcc
LINK_linux        = -lm -lpthread
LINK_macos        = -lm -lpthread
LINK_windows      = -lm
BASELINE_linux    = -march=x86-64 -mtune=generic
SLICE_macos       = arm64 x86_64
SUFFIX_windows    = .exe
LIBRARIES_windows = *.dll
ARTWORK_linux     = $(ICON).png
ARTWORK_macos     = $(ICON).icns
ARTWORK_windows   = $(ICON).ico
WINDRES_windows   = x86_64-w64-mingw32-windres
LAUNCHER_linux    = ada83.desktop

$(if $(COMPILE_$(TARGET)),,\
  $(error TARGET is '$(TARGET)'; it must be linux, macos or windows))

PACKAGE            = bin-$(TARGET).zip
PACKAGE_COMPILER   = $(COMPILER_$(TARGET))
PACKAGE_SLICES     = $(SLICE_$(TARGET))
PACKAGE_UNIVERSAL  = $(word 2,$(PACKAGE_SLICES))
PACKAGE_EXECUTABLE = ada83$(SUFFIX_$(TARGET))
PACKAGE_CFLAGS     = $(COMPILE_$(TARGET)) $(BASELINE_$(TARGET)) \
                     $(if $(CROSS),,$(WHOLE_PROGRAM))
PACKAGE_ARTWORK    = $(ARTWORK_$(TARGET))
PACKAGE_RESOURCE   = $(if $(WINDRES_$(TARGET)),staging/$(ICON).o)
PACKAGE_CONTENTS   = $(PACKAGE_EXECUTABLE) $(RUNTIME) $(VSIX) \
                     $(PACKAGE_ARTWORK) $(LAUNCHER_$(TARGET)) \
                     $(LIBRARIES_$(TARGET))

Slice_Output  = staging/$(PACKAGE_EXECUTABLE)$(if $(PACKAGE_UNIVERSAL),-$1)
Compile_Slice = $(PACKAGE_COMPILER) $(PACKAGE_CFLAGS) \
                $(if $(filter-out .,$1),-arch $1) \
                -o $(call Slice_Output,$1) ada83.c $(PACKAGE_RESOURCE) \
                $(LINK_$(TARGET))
PACKAGE_PIECES = $(foreach Slice,$(PACKAGE_SLICES),$(call Slice_Output,$(Slice)))

LIPO := $(shell command -v lipo || command -v llvm-lipo || \
                command -v "$$(llvm-config --bindir 2>/dev/null)/llvm-lipo")
SUDO := $(shell [ $$(id -u) -eq 0 ] || echo sudo)

LLVM_PRESENT_Darwin  = ls /opt/homebrew/opt/llvm/lib/libLLVM.dylib \
                          /usr/local/opt/llvm/lib/libLLVM.dylib \
                          /Library/Developer/CommandLineTools/usr/lib/libLLVM.dylib \
                          2>/dev/null | grep -q .
LLVM_PRESENT_Linux   = ldconfig -p 2>/dev/null | grep -q libLLVM
LLVM_MANAGERS_Darwin = brew:install,llvm
LLVM_MANAGERS_Linux  = apt-get:install,-y,--no-install-recommends,llvm \
                       dnf:install,-y,llvm-libs \
                       pacman:-S,--noconfirm,llvm-libs \
                       zypper:install,-y,libLLVM \
                       apk:add,llvm-libs
LLVM_PRESENT  = $(or $(LLVM_PRESENT_$(HOST_SYSTEM)),$(LLVM_PRESENT_Linux))
LLVM_MANAGERS = $(or $(LLVM_MANAGERS_$(HOST_SYSTEM)),$(LLVM_MANAGERS_Linux))
LLVM_SUDO     = $(if $(HOST_IS_MACOS),,$(SUDO))

all: ada83 provision-llvm

ada83: ada83.c
	$(CC) $(CFLAGS) $(WHOLE_PROGRAM) -o ada83 ada83.c $(LIBS) $(TUNE)

provision-llvm:
	-@$(LLVM_PRESENT) && exit 0; \
	  for Manager in $(LLVM_MANAGERS); do \
	    command -v $${Manager%%:*} >/dev/null 2>&1 || continue; \
	    echo "libLLVM not found - installing with $${Manager%%:*}"; \
	    $(LLVM_SUDO) $${Manager%%:*} $$(echo $${Manager#*:} | tr , ' ') \
	      && exit 0; \
	  done; \
	  echo "libLLVM not found; install your system's llvm package"

package: $(PACKAGE)

$(PACKAGE): ada83.c $(RUNTIME) $(BUNDLE)
	@command -v zip >/dev/null || { echo "zip is needed to package"; exit 1; }
	@command -v $(PACKAGE_COMPILER) >/dev/null || { \
	  echo "packaging for $(TARGET) needs $(PACKAGE_COMPILER)"; exit 1; }
	@test -z "$(PACKAGE_UNIVERSAL)" || test -n "$(LIPO)" || { \
	  echo "joining the $(TARGET) slices needs lipo"; exit 1; }
	@test -z "$(LIBRARIES_$(TARGET))" || test -f bin-$(TARGET).zip || { \
	  echo "bin-$(TARGET).zip holds the only copy of the libraries $(TARGET)"; \
	  echo "loads at run time, and is missing"; exit 1; }
	rm -rf staging && mkdir staging
	$(if $(PACKAGE_RESOURCE),\
	  echo '1 ICON "$(abspath $(ICON).ico)"' > staging/$(ICON).rc \
	    && $(WINDRES_$(TARGET)) staging/$(ICON).rc -O coff -o $(PACKAGE_RESOURCE))
	$(foreach Slice,$(or $(PACKAGE_SLICES),.),$(call Compile_Slice,$(Slice)) &&) \
	  true
	rm -f staging/$(ICON).rc
	$(if $(PACKAGE_UNIVERSAL),\
	  $(LIPO) -create -output staging/$(PACKAGE_EXECUTABLE) $(PACKAGE_PIECES) \
	    && rm -f $(PACKAGE_PIECES))
	cp $(RUNTIME) $(PACKAGE_ARTWORK) staging/
	$(if $(LAUNCHER_$(TARGET)),printf '%s\n' '[Desktop Entry]' 'Type=Application' \
	  'Name=Ada 83' 'Comment=Ada 83 compiler' 'Exec=ada83 %F' \
	  'Icon=ada83-icon' 'Terminal=true' 'Categories=Development;Building;' \
	  > staging/$(LAUNCHER_$(TARGET)))
	rm -f staging/$(ICON).o
	$(MAKE) --no-print-directory staging/$(VSIX)
	$(if $(LIBRARIES_$(TARGET)),\
	  unzip -qoj bin-$(TARGET).zip '$(LIBRARIES_$(TARGET))' -d staging)
	rm -f $@.new
	cd staging && zip -q $(abspath $@).new $(PACKAGE_CONTENTS)
	mv $@.new $@
	rm -rf staging
	@echo "Packaged $@:"; unzip -l $@ | tail -n +4

vsix:
	@command -v zip >/dev/null || { echo "zip is needed to package"; exit 1; }
	rm -rf staging && mkdir -p staging
	$(MAKE) --no-print-directory staging/$(VSIX)
	@echo "Built staging/$(VSIX):"; unzip -l staging/$(VSIX) | tail -n +4

staging/$(VSIX): $(BUNDLE)
	rm -rf staging/vsix
	mkdir -p staging/vsix/extension/syntaxes
	cp $(BUNDLE) staging/vsix/extension/
	cp $(ICON).png staging/vsix/extension/
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
	rm -f ada83 $(VSIX) *.o *.ll *.s *.exe a.out core bin-*.zip.new
	rm -rf staging test_results acats_logs acats/report.ll

clean-test:
	rm -rf test_results acats_logs acats/report.ll

.PHONY: all package vsix provision-llvm clean clean-test
