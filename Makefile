
CC = gcc
CFLAGS = -O3 -Wall -std=gnu2x
LIBS = -lm -lpthread

# This makefile builds the Unix-like hosts.  Windows is built by build.bat,
# which unpacks LLVM-C.zip beside the executable instead of relying on a
# system libLLVM.
WHOLE_PROGRAM = -fwhole-program

all: ada83 provision-llvm

ada83: ada83.c
	$(CC) $(CFLAGS) $(WHOLE_PROGRAM) -o ada83 ada83.c $(LIBS) -march=native

SUDO := $(shell [ $$(id -u) -eq 0 ] || echo sudo)
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

clean:
	rm -f ada83 *.o *.ll *.s *.exe a.out core
	rm -rf test_results acats_logs acats/report.ll

clean-test:
	rm -rf test_results acats_logs acats/report.ll

.PHONY: all provision-llvm clean clean-test
