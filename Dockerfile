# ============================================================================
# DOCKER CONTAINER FOR ADA83 ARM MICROKERNEL DEVELOPMENT
# ============================================================================
# Provides complete ARM cross-compilation environment with QEMU
# Usage:
#   docker build -t ada83-microkernel .
#   docker run -it --rm -v $(pwd):/workspace ada83-microkernel
# ============================================================================

FROM ubuntu:24.04

LABEL maintainer="Ada83 Microkernel Project"
LABEL description="ARM cross-compilation environment for Ada83 microkernel"
LABEL version="1.0"

# Prevent interactive prompts during installation
ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=UTC

# Install ARM toolchain and development tools
RUN apt-get update && apt-get install -y \
    gcc-arm-none-eabi \
    binutils-arm-none-eabi \
    qemu-system-arm \
    gdb-multiarch \
    build-essential \
    make \
    git \
    vim \
    nano \
    bc \
    wget \
    curl \
    && rm -rf /var/lib/apt/lists/*

# Set working directory
WORKDIR /workspace

# Add convenience aliases
RUN echo 'alias ll="ls -lah"' >> /root/.bashrc && \
    echo 'alias build="make -f Makefile.kernel all"' >> /root/.bashrc && \
    echo 'alias run="make -f Makefile.kernel run"' >> /root/.bashrc && \
    echo 'alias clean="make -f Makefile.kernel clean"' >> /root/.bashrc && \
    echo 'alias analyze="./analyze_kernel.sh"' >> /root/.bashrc && \
    echo 'alias simulate="./kernel_simulator"' >> /root/.bashrc

# Display welcome message on container start
RUN echo '#!/bin/bash' > /usr/local/bin/welcome.sh && \
    echo 'echo "╔════════════════════════════════════════════════════════════╗"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║      Ada83 ARM Microkernel - Development Container        ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "╠════════════════════════════════════════════════════════════╣"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║ ARM Toolchain:  $(arm-none-eabi-gcc --version | head -1 | cut -c1-38) ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║ QEMU ARM:       $(qemu-system-arm --version | head -1 | cut -c1-38) ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "╠════════════════════════════════════════════════════════════╣"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║ Quick commands:                                            ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║   build      - Build microkernel                           ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║   run        - Build and run in QEMU                       ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║   analyze    - Run static analysis                         ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║   simulate   - Run host simulator                          ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "║   clean      - Clean build artifacts                       ║"' >> /usr/local/bin/welcome.sh && \
    echo 'echo "╚════════════════════════════════════════════════════════════╝"' >> /usr/local/bin/welcome.sh && \
    chmod +x /usr/local/bin/welcome.sh && \
    echo '/usr/local/bin/welcome.sh' >> /root/.bashrc

# Set default command
CMD ["/bin/bash"]

# Health check (verify ARM toolchain is available)
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=1 \
    CMD arm-none-eabi-gcc --version || exit 1
