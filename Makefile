
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

tools/win64/LLVM-C.dll: tools/win64/LLVM-C.dll.zst
	zstd -d -f $< -o $@

tools/win64/llvmc_blob.o: tools/win64/LLVM-C.dll
	cd tools/win64 && objcopy -I binary -O pe-x86-64 LLVM-C.dll llvmc_blob.o

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
