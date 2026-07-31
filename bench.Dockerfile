# bench.Dockerfile — a fixed environment for `bash bench.sh codegen`.
#
# The point of this image is that two people on two machines get *comparable
# ratios*.  Absolute seconds still belong to the host: a laptop and a cloud VM
# will not agree on how long `sieve` takes, and no container can make them.
# What the image fixes is everything that would otherwise make the ada83/gnat
# ratio drift — the GNAT version, the LLVM the ada83 backend loads, the flags
# both compilers are handed, and the flags ada83 itself was built with.
#
# Build:
#   docker build -f bench.Dockerfile -t ada83-bench .
#
# Run — bench.sh pins to cores taken from whatever cpuset it is given, so any
# shape works, but give it at least two: the programs with tasks in them are
# measured on a pair.  SYS_NICE lets it raise its own priority; without the
# capability it measures at normal priority and says so rather than pretending:
#   docker run --rm --cpuset-cpus=0-3 --cap-add=SYS_NICE ada83-bench
#
# Anything after the image name is passed to bench.sh, so a shorter pass is
#   docker run --rm --cpuset-cpus=0-3 --cap-add=SYS_NICE \
#       -e SUITES=1 -e CODEGEN_REPEATS=11 ada83-bench codegen
#
# NOTE: this file has never been built.  It was written and checked by reading
# on a host with no Docker daemon.  Treat the first build as unverified.

# ---------------------------------------------------------------------------
# Base, by digest rather than by tag: `ubuntu:24.04` is a moving target that
# is rebuilt whenever its packages are patched, and two people who build this
# file a month apart would not be starting from the same bytes.  This digest
# is what `ubuntu:24.04` resolved to on 2026-07-31, read from the registry:
#
#   curl -s "https://auth.docker.io/token?service=registry.docker.io\
#   &scope=repository:library/ubuntu:pull" | jq -r .token   # -> $TOKEN
#   curl -sI -H "Authorization: Bearer $TOKEN" \
#     -H "Accept: application/vnd.oci.image.index.v1+json" \
#     https://registry-1.docker.io/v2/library/ubuntu/manifests/24.04 \
#     | grep docker-content-digest
#
# It is the multi-arch index digest, so it still selects the right image on
# arm64 — though the figures in the readme are x86_64 and only x86_64 ratios
# are comparable with them.
# ---------------------------------------------------------------------------
FROM ubuntu@sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90

SHELL ["/bin/bash", "-o", "pipefail", "-c"]
ARG DEBIAN_FRONTEND=noninteractive

# ---------------------------------------------------------------------------
# Toolchain, pinned to the exact archive versions the published table was
# measured with.  If apt says a version is no longer available, the build stops
# rather than silently substituting a different compiler — that is deliberate.
# Two ways out, in order of preference:
#   1. point apt at a snapshot of the archive from the day these were current,
#      which is how you rebuild this image years later:
#        --build-arg APT_SNAPSHOT=YYYYMMDDTHHMMSSZ
#      (a timestamp snapshot.ubuntu.com actually carries — no default is set
#      here because an unchecked one would only fail at build time)
#   2. override the versions:
#        --build-arg GNAT_VERSION=... --build-arg GCC_VERSION=...
#      and note in any published figure that the toolchain moved.
# ---------------------------------------------------------------------------
ARG GCC_VERSION=13.3.0-6ubuntu2~24.04.1
ARG GNAT_VERSION=13.3.0-6ubuntu2~24.04.1
ARG LLVM_APT_VERSION=1:20.1.2-0ubuntu1~24.04.2
ARG APT_SNAPSHOT=

RUN if [ -n "$APT_SNAPSHOT" ]; then \
      sed -i "s|http://archive.ubuntu.com/ubuntu|http://snapshot.ubuntu.com/ubuntu/$APT_SNAPSHOT|g; \
              s|http://security.ubuntu.com/ubuntu|http://snapshot.ubuntu.com/ubuntu/$APT_SNAPSHOT|g" \
          /etc/apt/sources.list.d/ubuntu.sources; \
    fi && \
    apt-get update && \
    apt-get install -y --no-install-recommends \
        "gcc-13=$GCC_VERSION" \
        "cpp-13=$GCC_VERSION" \
        "libgcc-13-dev=$GCC_VERSION" \
        "gnat-13=$GNAT_VERSION" \
        "libgnat-13=$GNAT_VERSION" \
        "libllvm20=$LLVM_APT_VERSION" \
        libc6-dev \
        make \
        bash \
        mawk \
        util-linux \
        coreutils \
        ca-certificates && \
    rm -rf /var/lib/apt/lists/*

# One gcc and one gnatmake on PATH, by their version-suffixed binaries, so no
# alternatives system can quietly change which compiler runs. `cc` is included
# because ada83 links what it compiles by trying cc, then clang, then gcc, and
# installing gcc-13 rather than the gcc metapackage leaves no cc behind.
RUN ln -sf /usr/bin/gcc-13      /usr/local/bin/cc && \
    ln -sf /usr/bin/gcc-13      /usr/local/bin/gcc && \
    ln -sf /usr/bin/gnatmake-13 /usr/local/bin/gnatmake && \
    ln -sf /usr/bin/gnatbind-13 /usr/local/bin/gnatbind && \
    ln -sf /usr/bin/gnatlink-13 /usr/local/bin/gnatlink && \
    ln -sf /usr/bin/gnat-13     /usr/local/bin/gnat

# ada83's backend *is* libLLVM, and it dlopens it by trying a list of sonames
# newest first — so on a machine with LLVM 17, 18 and 20 side by side it takes
# 20, and on a machine with only 18 it takes 18, and the code it generates is
# not the same code. That is exactly the kind of silent difference this image
# exists to remove: one LLVM installed, and named outright rather than found.
# LLVM 20 because that is what produced the figures in the readme.
#
# The library lives under a multiarch directory whose name depends on the
# architecture, so it is linked to one fixed path here rather than spelled out.
RUN ln -sf "/usr/lib/$(gcc -dumpmachine)/libLLVM.so.20.1" /usr/local/lib/libLLVM.so.20 && \
    test -e /usr/local/lib/libLLVM.so.20
ENV ADA83_LLVM_LIB=/usr/local/lib/libLLVM.so.20

WORKDIR /bench
COPY ada83.c ada83-runtime.ada bench.sh makefile ./

# ---------------------------------------------------------------------------
# The compiler itself, at fixed flags.
#
# `make` would use -march=native, which is the one flag that must not be here:
# it bakes in whatever the *build* machine happens to support, so an image
# built on one host and run on another is either a different binary or an
# illegal-instruction fault.  Baseline x86-64 with generic tuning instead.
#
# This governs how fast ada83 compiles, not how fast the code it emits runs;
# the benchmark programs are built by both compilers at -O2 with no -march at
# all, so the *generated* code is baseline on every host.
# ---------------------------------------------------------------------------
ARG ADA83_CFLAGS="-O3 -Wall -std=gnu2x -fwhole-program -march=x86-64 -mtune=generic"
RUN gcc $ADA83_CFLAGS -o /bench/ada83 /bench/ada83.c -lm -lpthread && \
    /bench/ada83 --version

# bench.sh otherwise asks the makefile what it would use, and the makefile
# would answer -march=native — true of a `make` build, false of this one.
ENV ADA83_BUILD_FLAGS="gcc ${ADA83_CFLAGS}"

# ---------------------------------------------------------------------------
# What the image is, recorded at build time and printed by every run, so a
# pasted result carries its own provenance.
# ---------------------------------------------------------------------------
RUN { echo "image built     $(date -u +%Y-%m-%dT%H:%M:%SZ)"; \
      echo "base            ubuntu@sha256:4fbb8e6a8395de5a7550b33509421a2bafbc0aab6c06ba2cef9ebffbc7092d90"; \
      echo "                $(. /etc/os-release; echo "$PRETTY_NAME")"; \
      echo "gcc             $(gcc --version | head -1)"; \
      echo "gnatmake        $(gnatmake --version | head -1)"; \
      echo "libLLVM         $(readlink -f "$ADA83_LLVM_LIB")  ($(dpkg-query -W -f='${Version}' libllvm20))"; \
      echo "ada83 built     gcc $ADA83_CFLAGS"; \
      echo "benchmarks      both compilers at -O2, no -march, so baseline on any host"; \
    } > /etc/bench-environment && cat /etc/bench-environment

COPY bench-entry.sh /usr/local/bin/bench-entry.sh
RUN chmod +x /usr/local/bin/bench-entry.sh

ENV NO_ANIMATE=1 NO_COLOUR=1
ENTRYPOINT ["/usr/local/bin/bench-entry.sh"]
CMD ["codegen"]
