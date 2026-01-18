# Quick Start Guide - Ada83 ARM Microkernel

## 🚀 Fastest Path to Running the Microkernel

### One-Command Build and Test

```bash
# Install dependencies (Ubuntu/Debian)
sudo apt-get update && sudo apt-get install -y gcc-arm-none-eabi qemu-system-arm

# Build and run
./test_kernel.sh
```

That's it! You should see:
```
Ada83 Minix ARM Microkernel v1.0
Initializing IPC subsystem...
Scheduler active. Entering main loop.
```

Press `Ctrl-A` then `X` to exit QEMU.

---

## 📦 Installation by Platform

### Ubuntu/Debian
```bash
sudo apt-get install gcc-arm-none-eabi qemu-system-arm gdb-multiarch
```

### Fedora/RHEL
```bash
sudo dnf install arm-none-eabi-gcc-cs qemu-system-arm
```

### Arch Linux
```bash
sudo pacman -S arm-none-eabi-gcc qemu-arch-extra
```

### macOS
```bash
brew install arm-none-eabi-gcc qemu
```

### Windows (WSL2)
```bash
# Inside WSL2 Ubuntu:
sudo apt-get install gcc-arm-none-eabi qemu-system-arm
```

---

## 🛠️ Build Options

### Standard Build
```bash
make -f Makefile.kernel all
```

### Clean Build
```bash
make -f Makefile.kernel clean all
```

### Build and Run
```bash
make -f Makefile.kernel run
```

### Debug Build
```bash
make -f Makefile.kernel debug
# In another terminal:
arm-none-eabi-gdb kernel.elf
(gdb) target remote :1234
(gdb) continue
```

---

## 📊 What Gets Built

```
kernel.elf          - ELF executable (20-30 KB)
kernel.bin          - Raw binary for flashing
kernel.map          - Memory layout map
kernel.dis          - Full disassembly listing
kernel.sym          - Symbol table
```

---

## 🧪 Testing

### Quick Test (10 second timeout)
```bash
timeout 10s qemu-system-arm -M virt -cpu cortex-a15 -m 128M \
    -nographic -serial mon:stdio -kernel kernel.elf
```

### Interactive Test
```bash
./test_kernel.sh run
# Press Ctrl-A then X to exit
```

### Automated CI Test
```bash
# Check build succeeds
make -f Makefile.kernel all

# Verify binary exists
test -f kernel.elf && echo "SUCCESS" || echo "FAILED"

# Check size is reasonable
SIZE=$(stat -f%z kernel.elf 2>/dev/null || stat -c%s kernel.elf)
[ "$SIZE" -lt 100000 ] && echo "Size OK" || echo "Too large"
```

---

## 🐛 Debugging

### View Disassembly
```bash
arm-none-eabi-objdump -D kernel.elf | less
```

### Check Memory Layout
```bash
arm-none-eabi-readelf -S kernel.elf
```

### Examine Symbols
```bash
arm-none-eabi-nm -n kernel.elf
```

### GDB Debugging Session
```bash
# Terminal 1
make -f Makefile.kernel debug

# Terminal 2
arm-none-eabi-gdb kernel.elf
(gdb) target remote :1234
(gdb) break _ada_microkernel_operating_system_executive
(gdb) continue
(gdb) layout asm
(gdb) info registers
(gdb) x/16i $pc
```

---

## ⚡ Quick Commands Reference

```bash
# Build
./test_kernel.sh build

# Run
./test_kernel.sh run

# Debug
./test_kernel.sh debug

# Clean
./test_kernel.sh clean

# Check dependencies
./test_kernel.sh deps

# View disassembly
./test_kernel.sh disasm
```

---

## 🎯 Expected Output

### Successful Boot
```
========================================================================
Starting microkernel in QEMU...
Press Ctrl-A X to exit QEMU
========================================================================
Ada83 Minix ARM Microkernel v1.0
Initializing IPC subsystem...
Scheduler active. Entering main loop.
```

### Build Output
```
========================================================================
Ada83 ARM Microkernel Build System
========================================================================
[AS] Assembling ARM bootstrap...
[GEN] Generating minimal C runtime stubs...
[CC] Compiling C runtime stubs...
[ADA->C] Translating Ada to C (simplified for bare-metal)...
[CC] Compiling Ada kernel (C translation)...
[LD] Linking microkernel ELF...
[INFO] Generating disassembly...
[INFO] Generating symbol map...
[OBJCOPY] Creating raw binary...
===================================================================
Microkernel build complete!
ELF binary: kernel.elf
Raw binary: kernel.bin
To test: make run
===================================================================
```

---

## 🔧 Troubleshooting

### "arm-none-eabi-gcc: not found"
→ Install ARM cross-compiler (see Installation section above)

### "qemu-system-arm: not found"
→ Install QEMU ARM emulator

### Build hangs
→ Check that all source files exist:
  - `kernel.adb`
  - `boot.s`
  - `kernel.ld`

### QEMU shows nothing
→ Make sure you're using `-nographic -serial mon:stdio`

### "Permission denied" on test_kernel.sh
```bash
chmod +x test_kernel.sh
```

---

## 📁 File Overview

```
kernel.adb          - Main Ada microkernel implementation (~350 lines)
boot.s              - ARM assembly bootstrap (~400 lines)
kernel.ld           - Linker script for memory layout
Makefile.kernel     - Build system
test_kernel.sh      - Test runner script
MICROKERNEL.md      - Complete documentation
QUICKSTART.md       - This file
```

---

## 🚀 Next Steps

After getting it running:

1. **Read the code**: Start with `kernel.adb` - it's highly documented
2. **Explore assembly**: Check out `boot.s` for low-level ARM programming
3. **Debug it**: Use GDB to step through boot process
4. **Modify it**: Add your own system calls or IPC messages
5. **Extend it**: Add multicore support, MMU, or real-time scheduling

---

## 💡 Tips

- **Fast iteration**: Use `make -f Makefile.kernel run` for build+test
- **Size matters**: Kernel should be < 50KB. Check with `ls -lh kernel.elf`
- **Assembly first**: If build fails, try `make -f Makefile.kernel asm-only`
- **Debug early**: Use `-s -S` QEMU flags and GDB from the start
- **UART is your friend**: All output goes through UART - add debug prints liberally

---

## 📚 Further Reading

- `MICROKERNEL.md` - Complete architecture documentation
- `kernel.adb` - Ada microkernel source (heavily commented)
- `boot.s` - ARM assembly (literate programming style)

---

**Happy Hacking! 🎉**

*Remember: A microkernel is measured not by what it does, but by what it doesn't do.*
