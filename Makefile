# Ada83 compiler. `make` is the whole story:
#   - builds ./ada83 (one C file; LLVM is never a build-time dependency)
#   - on Linux, best-effort provisions the runtime-loaded libLLVM that
#     `--native` uses, probing the distribution's package manager
#   - on Windows (MSYS2), decompresses the vendored libLLVM under
#     tools/win64/ and embeds it into ada83.exe, making the executable a
#     fully standalone single-file toolchain

CC = gcc
CFLAGS = -O3 -Wall
LIBS = -lm -lpthread

ifeq ($(OS),Windows_NT)
BLOB = tools/win64/llvmc_blob.o
else
BLOB =
endif

all: ada83 provision-llvm

ada83: ada83.c $(BLOB)
	$(CC) $(CFLAGS) -o ada83 ada83.c $(BLOB) $(LIBS) -march=native

# The vendored DLL is upstream MSYS2's libLLVM, unmodified, recompressed
# alone so the repository carries only the one file --native requires
# (provenance and checksum: tools/win64/README.md). zstd is always present
# under MSYS2 — pacman itself depends on it. objcopy turns the DLL into a
# linkable blob; §20's weak symbols pick it up and extract it to
# %LOCALAPPDATA% on first --native use.
tools/win64/LLVM-C.dll: tools/win64/LLVM-C.dll.zst
	zstd -d -f $< -o $@

tools/win64/llvmc_blob.o: tools/win64/LLVM-C.dll
	cd tools/win64 && objcopy -I binary -O pe-x86-64 LLVM-C.dll llvmc_blob.o

# Provision libLLVM for --native (runtime-dlopen'd; never a build
# dependency). Failure is non-fatal — --native prints an actionable hint.
SUDO := $(shell [ $$(id -u) -eq 0 ] || echo sudo)
provision-llvm:
ifeq ($(OS),Windows_NT)
	@:
else
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
	rm -f tools/win64/LLVM-C.dll tools/win64/llvmc_blob.o
	rm -rf test_results acats_logs acats/report.ll

clean-test:
	rm -rf test_results acats_logs acats/report.ll

.PHONY: all provision-llvm clean clean-test
