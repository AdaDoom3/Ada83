# Vendored libLLVM for Windows x86-64

`LLVM-C.dll.zst` is `mingw64/bin/libLLVM-22.dll` taken UNMODIFIED from the
upstream MSYS2 package and recompressed alone (zstd) so the repository
carries only the one file `--native` requires:

- Package:  `mingw-w64-x86_64-llvm-libs-22.1.8-2-any.pkg.tar.zst`
- Source:   https://repo.msys2.org/mingw/mingw64/
- Package sha256: `9d873ff295885e714fc1d1ee2096e52a653d0cbebc50c62c8c89f42e944e0833`
- License:  Apache-2.0 WITH LLVM-exception (see the LLVM project)

`make` (under MSYS2) decompresses it to `LLVM-C.dll` — a DLL does not care
about its own file name — and embeds it into `ada83.exe` as a binary blob;
§20 of `ada83.c` extracts it to `%LOCALAPPDATA%\ada83` on first `--native`
use and loads it from there. Both derived files are gitignored.

To upgrade: fetch the newer `llvm-libs` package, extract
`mingw64/bin/libLLVM-<N>.dll`, `zstd` it to `LLVM-C.dll.zst`, update the
checksum above and `LLVM_DLL_IN_PKG`-adjacent names in the Makefile, and
extend the version probe lists in §20 of `ada83.c` if `<N>` is new.
