CC     = gcc
CFLAGS = -O3 -Wall -std=gnu2x
LIBS   = -lm -lpthread

HOST_SYSTEM   := $(shell uname -s)
HOST_MACHINE  := $(shell uname -m)
HOST_IS_MACOS := $(filter Darwin,$(HOST_SYSTEM))
HOST_IS_GCC   := $(shell echo | $(CC) -dM -E - 2>/dev/null \
                         | grep -q __clang__ || echo yes)

WHOLE_PROGRAM := $(if $(HOST_IS_GCC),-fwhole-program)
TUNE          := $(if $(HOST_IS_GCC)$(filter x86_64 amd64,$(HOST_MACHINE)),\
                      -march=native)

RUNTIME = ada83-runtime.ada
MANUAL  = manual.md
VSIX    = ada83.vsix
BUNDLE  = ada83-extension.html
ICON    = ada83-icon

HOST_TARGET := $(if $(HOST_IS_MACOS),macos,linux)
TARGET      ?= $(HOST_TARGET)
CROSS       := $(filter-out $(HOST_TARGET),$(TARGET))

COMPILER_linux            = $(if $(CROSS),x86_64-linux-gnu-gcc,$(CC))
COMPILER_macos            = $(if $(CROSS),o64-clang,$(CC))
COMPILER_windows          = x86_64-w64-mingw32-gcc
COMPILER_FLAGS_linux      = -O3 -Wall -g0 -std=gnu17 -march=x86-64 -mtune=generic
COMPILER_FLAGS_macos      = -O3 -Wall -g0 -std=gnu2x
COMPILER_FLAGS_windows    = -O2 -Wall -g0 -std=gnu2x
ARCHITECTURES_macos       = arm64 x86_64
LINK_LIBRARIES_linux      = -lm -lpthread
LINK_LIBRARIES_macos      = -lm -lpthread
LINK_LIBRARIES_windows    = -lm
SUFFIX_windows            = .exe
RESOURCE_FORK_macos       = $(ICON).rsrc
ARTWORK_linux             = $(ICON).png
ARTWORK_macos             = $(ICON).icns
ARTWORK_windows           = $(ICON).ico
LAUNCHER_linux            = ada83.desktop
RESOURCE_COMPILER_windows = x86_64-w64-mingw32-windres
SHARED_LIBRARIES_windows  = *.dll

$(if $(COMPILER_$(TARGET)),,\
  $(error TARGET is '$(TARGET)'; it must be linux, macos or windows))

$(foreach Role,COMPILER COMPILER_FLAGS LINK_LIBRARIES SUFFIX ARTWORK LAUNCHER \
               RESOURCE_COMPILER RESOURCE_FORK SHARED_LIBRARIES ARCHITECTURES,\
  $(eval $(Role) := $($(Role)_$(TARGET))))

COMPILER_FLAGS += $(if $(CROSS),,$(WHOLE_PROGRAM))

PACKAGE          = bin-$(TARGET).zip
LIBRARY_SOURCE   = bin-$(TARGET).zip
EXECUTABLE       = ada83$(SUFFIX)
RESOURCE_OBJECT  = $(if $(RESOURCE_COMPILER),staging/$(ICON).o)
PACKAGE_CONTENTS = $(EXECUTABLE) $(RUNTIME) $(VSIX) $(ARTWORK) $(LAUNCHER) \
                   $(SHARED_LIBRARIES) \
                   $(if $(RESOURCE_FORK),__MACOSX/._$(EXECUTABLE))

LIPO := $(shell command -v lipo || command -v llvm-lipo || \
                command -v "$$(llvm-config --bindir 2>/dev/null)/llvm-lipo")
SUDO := $(shell [ $$(id -u) -eq 0 ] || echo sudo)

Slice_Path       = staging/$(EXECUTABLE)$(if $1,-$1)
Compile_Slice    = $(COMPILER) $(COMPILER_FLAGS) $(if $1,-arch $1) \
                   -o $(call Slice_Path,$1) ada83.c $(RESOURCE_OBJECT) \
                   $(LINK_LIBRARIES)
SLICES           = $(foreach Architecture,$(ARCHITECTURES),\
                     $(call Slice_Path,$(Architecture)))
BUILD_EXECUTABLE = $(if $(SLICES),\
                     $(foreach Architecture,$(ARCHITECTURES),\
                       $(call Compile_Slice,$(Architecture)) &&) \
                     $(LIPO) -create -output $(call Slice_Path,) $(SLICES),\
                     $(call Compile_Slice,))

LLVM_PRESENT_Darwin = ls /opt/homebrew/opt/llvm/lib/libLLVM.dylib \
                         /usr/local/opt/llvm/lib/libLLVM.dylib \
                         /Library/Developer/CommandLineTools/usr/lib/libLLVM.dylib \
                         2>/dev/null | grep -q .
LLVM_PRESENT_Linux  = ldconfig -p 2>/dev/null | grep -q libLLVM
LLVM_INSTALL_Darwin = Install brew install llvm
LLVM_INSTALL_Linux  = Install apt-get install -y --no-install-recommends llvm; \
                      Install dnf install -y llvm-libs; \
                      Install pacman -S --noconfirm llvm-libs; \
                      Install zypper install -y libLLVM; \
                      Install apk add llvm-libs
LLVM_PRESENT = $(or $(LLVM_PRESENT_$(HOST_SYSTEM)),$(LLVM_PRESENT_Linux))
LLVM_INSTALL = $(or $(LLVM_INSTALL_$(HOST_SYSTEM)),$(LLVM_INSTALL_Linux))
LLVM_SUDO    = $(if $(HOST_IS_MACOS),,$(SUDO))

all: ada83 provision-llvm

ada83: ada83.c
	$(CC) $(CFLAGS) $(WHOLE_PROGRAM) $(TUNE) -o $@ $< $(LIBS)

provision-llvm:
	@$(LLVM_PRESENT) && exit 0; \
	 Install () { \
	   command -v $$1 >/dev/null 2>&1 || return 0; \
	   echo "libLLVM not found - installing with $$1"; \
	   $(LLVM_SUDO) "$$@" && exit 0; \
	 }; \
	 $(LLVM_INSTALL); \
	 echo "libLLVM not found; install your system's llvm package"

package: $(PACKAGE)

$(PACKAGE): ada83.c $(RUNTIME) $(ARTWORK) staging/$(VSIX)
	@command -v $(COMPILER) >/dev/null || { \
	  echo "packaging for $(TARGET) needs $(COMPILER)"; exit 1; }
	@test -z "$(SHARED_LIBRARIES)" || test -f $(LIBRARY_SOURCE) || { \
	  echo "$(LIBRARY_SOURCE) holds the only copy of the libraries $(TARGET)"; \
	  echo "loads at run time, and is missing"; exit 1; }
	@test -z "$(SLICES)" || test -n "$(LIPO)" || { \
	  echo "joining the $(TARGET) slices needs lipo"; exit 1; }
	@test -z "$(RESOURCE_OBJECT)" || { set -x; \
	  echo '1 ICON "$(abspath $(ARTWORK))"' \
	    | $(RESOURCE_COMPILER) -O coff -o $(RESOURCE_OBJECT); }
	$(BUILD_EXECUTABLE)
	cp $(RUNTIME) $(ARTWORK) staging/
	test -z "$(RESOURCE_FORK)" || { mkdir -p staging/__MACOSX \
	  && cp $(RESOURCE_FORK) staging/__MACOSX/._$(EXECUTABLE); }
	test -z "$(LAUNCHER)" || printf '%s\n' '[Desktop Entry]' 'Type=Application' \
	  'Name=Ada 83' 'Comment=Ada 83 compiler' 'Exec=ada83 %F' 'Icon=$(ICON)' \
	  'Terminal=true' 'Categories=Development;Building;' > staging/$(LAUNCHER)
	test -z "$(SHARED_LIBRARIES)" || \
	  unzip -qoj $(LIBRARY_SOURCE) '$(SHARED_LIBRARIES)' -d staging
	rm -f $@.new
	cd staging && zip -q $(abspath $@).new $(PACKAGE_CONTENTS)
	mv $@.new $@
	@echo "Packaged $@:"; unzip -l $@ | tail -n +4

vsix: staging/$(VSIX)
	@echo "Built $<:"; unzip -l $< | tail -n +4

staging/$(VSIX): $(BUNDLE) $(ICON).png $(wildcard $(MANUAL))
	@command -v zip >/dev/null || { echo "zip is needed to package"; exit 1; }
	rm -rf staging/vsix && mkdir -p staging/vsix/extension/syntaxes
	cp $(ICON).png staging/vsix/extension/
	@test -f $(MANUAL) && cp $(MANUAL) staging/vsix/extension/ \
	  || echo "$(MANUAL) is missing; packaging without the manual search tool"
	awk '/^<\/script>$$/ { out = ""; next } \
	     /^<script/ { match ($$0, /id="[^"]*"/); \
	                  name = substr ($$0, RSTART + 4, RLENGTH - 5); \
	                  out = (name ~ /^(extension.vsixmanifest|\[)/) \
	                        ? "staging/vsix/" name \
	                        : "staging/vsix/extension/" name; next } \
	     out != "" { print > out }' $(BUNDLE)
	rm -f $@
	cd staging/vsix && zip -qr $(abspath $@) .
	rm -rf staging/vsix

clean: clean-test
	rm -f ada83 ada83.exe $(VSIX) bin-*.zip.new
	rm -rf staging

clean-test:
	rm -rf test_results acats_logs acats/report.ll

.PHONY: all package vsix provision-llvm clean clean-test
