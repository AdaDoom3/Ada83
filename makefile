CC     = gcc
CFLAGS = -O3 -Wall -std=gnu2x
LIBS   = -lpthread

# Stage lines in the manner of test-bench.sh: dimmed on a terminal, plain
# text when piped, so CI logs and redirected output stay free of ANSI codes.
STAGE = if [ -t 1 ]; then printf '  \033[2m%s\033[0m\n' "$(1)"; \
        else printf '  %s\n' "$(1)"; fi

HOST_SYSTEM   := $(shell uname -s)
HOST_MACHINE  := $(shell uname -m)
HOST_IS_MACOS := $(filter Darwin,$(HOST_SYSTEM))
HOST_IS_GCC   := $(shell echo | $(CC) -dM -E - 2>/dev/null \
                         | grep -q __clang__ || echo yes)

WHOLE_PROGRAM := $(if $(HOST_IS_GCC),-fwhole-program)
TUNE          := $(if $(HOST_IS_GCC)$(filter x86_64 amd64,$(HOST_MACHINE)),\
                      -march=native)

CONTAINED_GCC = docker run --rm --volume $(CURDIR):/work --workdir /work \
                gcc:13 gcc
Found = $(if $(shell command -v $1 2>/dev/null),$1)
LINUX_CROSS_COMPILER := $(or \
  $(call Found,x86_64-linux-gnu-gcc), \
  $(call Found,x86_64-unknown-linux-gnu-gcc), \
  $(if $(call Found,docker),$(CONTAINED_GCC)), \
  $(if $(call Found,podman),$(subst docker,podman,$(CONTAINED_GCC))), \
  x86_64-linux-gnu-gcc)

RUNTIME     = ada83-runtime.ada
MANUAL      = ada83-manual.md
VSIX        = ada83.vsix
BUNDLE      = ada83-extension.html
ICON        = ada83-icon
ICON_SOURCE = $(ICON).png

ICON_WRITER = \
  Byte () { printf "\\$$(printf '%03o' $$(($$1 & 255)))"; }; \
  Big () { Byte $$(($$1 >> 24)); Byte $$(($$1 >> 16)); \
           Byte $$(($$1 >> 8)); Byte $$1; }; \
  Little () { Byte $$1; Byte $$(($$1 >> 8)); \
              Byte $$(($$1 >> 16)); Byte $$(($$1 >> 24)); }; \
  Short () { Byte $$(($$1 >> 8)); Byte $$1; }; \
  Zeros () { Written=0; while [ $$Written -lt $$1 ]; do Byte 0; \
             Written=$$((Written + 1)); done; }; \
  Number () { od -An -tu1 -j$$1 -N4 $(ICON_SOURCE) \
              | { read a b c d; echo $$((a<<24 | b<<16 | c<<8 | d)); }; }; \
  Length=`wc -c < $(ICON_SOURCE)`; Width=`Number 16`; Height=`Number 20`; \
  case $$Width in \
    16) Chunk=icp4;; 32) Chunk=icp5;; 64) Chunk=icp6;; 128) Chunk=ic07;; \
    256) Chunk=ic08;; 512) Chunk=ic09;; 1024) Chunk=ic10;; *) Chunk=ic07;; \
  esac

ICON_ICO = { Short 0; Byte 1; Byte 0; Byte 1; Byte 0; \
             Byte $$((Width % 256)); Byte $$((Height % 256)); \
             Byte 0; Byte 0; Byte 1; Byte 0; Byte 32; Byte 0; \
             Little $$Length; Little 22; cat $(ICON_SOURCE); }

ICON_ICNS = { printf icns; Big $$((Length + 16)); printf %s $$Chunk; \
              Big $$((Length + 8)); cat $(ICON_SOURCE); }

ICON_RSRC = Icns=`wc -c < $(BIN_DIR)/$(ICON).icns`; Data=$$((Icns + 4)); \
            { Big 333319; Big 131072; Zeros 16; Short 2; \
              Big 9; Big 50; Big 32; \
              Big 2; Big 82; Big $$((256 + Data + 46)); \
              Zeros 8; Short 1024; Zeros 22; \
              Big 256; Big $$((256 + Data)); Big $$Data; Big 46; \
              Zeros 240; Big $$Icns; cat $(BIN_DIR)/$(ICON).icns; \
              Zeros 24; Short 28; Short 46; Short 0; \
              printf icns; Short 0; Short 10; \
              Short 49081; Short 65535; Big 0; }

Slice_Path       = $(if $1,staging/$(EXECUTABLE)-$1,$(BIN_DIR)/$(EXECUTABLE))
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

HOST_TARGET := $(if $(HOST_IS_MACOS),macos,linux)
TARGET      ?= $(HOST_TARGET)
CROSS       := $(filter-out $(HOST_TARGET),$(TARGET))

COMPILER_linux            = $(if $(CROSS),$(LINUX_CROSS_COMPILER),$(CC))
COMPILER_macos            = $(if $(CROSS),o64-clang,$(CC))
COMPILER_windows          = x86_64-w64-mingw32-gcc
COMPILER_FLAGS_linux      = -O3 -Wall -g0 -std=gnu17 -march=x86-64 -mtune=generic
COMPILER_FLAGS_macos      = -O3 -Wall -g0 -std=gnu2x
COMPILER_FLAGS_windows    = -O2 -Wall -g0 -std=gnu2x
ARCHITECTURES_macos       = arm64 x86_64
LINK_LIBRARIES_linux      = -lpthread
LINK_LIBRARIES_macos      = -lpthread
LINK_LIBRARIES_windows    =
SUFFIX_windows            = .exe
ARTWORK_linux             = $(ICON).png
ARTWORK_macos             = $(ICON).icns
ARTWORK_windows           = $(ICON).ico
ICON_BUILD_linux          = cp $(ICON_SOURCE) $(BIN_DIR)/$(ICON).png
ICON_BUILD_macos          = $(ICON_ICNS) > $(BIN_DIR)/$(ICON).icns; \
                            mkdir -p $(BIN_DIR)/__MACOSX; \
                            $(ICON_RSRC) > $(BIN_DIR)/__MACOSX/._$(EXECUTABLE)
ICON_BUILD_windows        = $(ICON_ICO) > $(BIN_DIR)/$(ICON).ico
LAUNCHER_linux            = ada83.desktop
RESOURCE_COMPILER_windows = x86_64-w64-mingw32-windres
SHARED_LIBRARIES_windows  = *.dll

$(if $(COMPILER_$(TARGET)),,\
  $(error TARGET is '$(TARGET)'; it must be linux, macos or windows))

$(foreach Role,COMPILER COMPILER_FLAGS LINK_LIBRARIES SUFFIX ARTWORK LAUNCHER \
               RESOURCE_COMPILER SHARED_LIBRARIES ARCHITECTURES,\
  $(eval $(Role) := $($(Role)_$(TARGET))))

COMPILER_FLAGS += $(if $(CROSS),,$(WHOLE_PROGRAM))

BIN_DIR          = bin-$(TARGET)
BIN_DIRS         = bin-linux bin-macos bin-windows
LIBRARY_SOURCE   = bin-dll.zip
EXECUTABLE       = ada83$(SUFFIX)
HOST_BINARY      = bin-$(HOST_TARGET)/ada83
RESOURCE_OBJECT  = $(if $(RESOURCE_COMPILER),staging/$(ICON).o)

LIPO := $(shell command -v lipo || command -v llvm-lipo || \
                command -v "$$(llvm-config --bindir 2>/dev/null)/llvm-lipo")
SUDO := $(shell [ $$(id -u) -eq 0 ] || echo sudo)

all: ada83 provision-llvm

ada83: $(HOST_BINARY)

$(HOST_BINARY): ada83.c
	@mkdir -p $(@D)
	@$(call STAGE,compiling ada83.c with $(CC))
	$(CC) $(CFLAGS) $(WHOLE_PROGRAM) $(TUNE) -o $@ $< $(LIBS)
	@echo "Built $@."

provision-llvm:
	@$(LLVM_PRESENT) && exit 0; \
	 Install () { \
	   command -v $$1 >/dev/null 2>&1 || return 0; \
	   echo "libLLVM not found - installing with $$1"; \
	   $(LLVM_SUDO) "$$@" && exit 0; \
	 }; \
	 $(LLVM_INSTALL); \
	 echo "libLLVM not found; install your system's llvm package"

# `package` fills bin-<target>/ with everything a release carries: the
# compiler, the runtime, the extension, the platform artwork, and — on
# Windows — the vendored DLLs unpacked from bin-dll.zip, which holds
# nothing else. The release workflow zips the folder itself.
package: ada83.c $(RUNTIME) $(ICON_SOURCE) $(BIN_DIR)/$(VSIX)
	@command -v $(firstword $(COMPILER)) >/dev/null || { \
	  echo "packaging for $(TARGET) needs $(firstword $(COMPILER))"; exit 1; }
	@test -z "$(SHARED_LIBRARIES)" || test -f $(LIBRARY_SOURCE) || { \
	  echo "$(LIBRARY_SOURCE) holds the only copy of the libraries $(TARGET)"; \
	  echo "loads at run time, and is missing"; exit 1; }
	@test -z "$(SLICES)" || test -n "$(LIPO)" || { \
	  echo "joining the $(TARGET) slices needs lipo"; exit 1; }
	@mkdir -p staging $(BIN_DIR)
	@$(call STAGE,building the $(TARGET) icon)
	$(ICON_WRITER); $(ICON_BUILD_$(TARGET))
	@test -z "$(RESOURCE_OBJECT)" || { set -x; \
	  echo '1 ICON "$(abspath $(BIN_DIR)/$(ICON).ico)"' \
	    | $(RESOURCE_COMPILER) -O coff -o $(RESOURCE_OBJECT); }
	@$(call STAGE,compiling ada83.c for $(TARGET))
	$(BUILD_EXECUTABLE)
	cp $(RUNTIME) $(BIN_DIR)/
	test -z "$(LAUNCHER)" || printf '%s\n' '[Desktop Entry]' 'Type=Application' \
	  'Name=Ada 83' 'Comment=Ada 83 compiler' 'Exec=ada83 %F' 'Icon=$(ICON)' \
	  'Terminal=true' 'Categories=Development;Building;' > $(BIN_DIR)/$(LAUNCHER)
	test -z "$(SHARED_LIBRARIES)" || \
	  unzip -qoj $(LIBRARY_SOURCE) '$(SHARED_LIBRARIES)' -d $(BIN_DIR)
	@echo "Packaged $(BIN_DIR)/:"; ls -1 $(BIN_DIR)

vsix: $(BIN_DIR)/$(VSIX)
	@echo "Built $<:"; unzip -l $< | tail -n +4

$(BIN_DIR)/$(VSIX): $(BUNDLE) $(ICON).png $(wildcard $(MANUAL))
	@command -v zip >/dev/null || { echo "zip is needed to package"; exit 1; }
	@mkdir -p $(BIN_DIR)
	rm -rf staging/vsix && mkdir -p staging/vsix/extension/syntaxes
	cp $(ICON).png staging/vsix/extension/
	@test -f $(MANUAL) && cp $(MANUAL) staging/vsix/extension/ \
	  || echo "$(MANUAL) is missing; packaging without the manual search tool"
	@$(call STAGE,splitting $(BUNDLE))
	awk '/^<\/script>$$/ { out = ""; next } \
	     /^<script/ { match ($$0, /id="[^"]*"/); \
	                  name = substr ($$0, RSTART + 4, RLENGTH - 5); \
	                  out = (name ~ /^(extension.vsixmanifest|\[)/) \
	                        ? "staging/vsix/" name \
	                        : "staging/vsix/extension/" name; next } \
	     out != "" { print > out }' $(BUNDLE)
	rm -f $@
	@$(call STAGE,packing $(VSIX))
	cd staging/vsix && zip -qr $(abspath $@) .
	rm -rf staging/vsix

clean: clean-test
	rm -f ada83 ada83.exe $(VSIX)
	rm -rf staging $(BIN_DIRS)

clean-test:
	rm -rf test_results acats_logs acats/report.ll

.PHONY: all ada83 package vsix provision-llvm clean clean-test
