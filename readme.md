# Ada83

An Ada 83 (ANSI/MIL-STD-1815A) compiler in a single C file, with an LLVM
backend.

![The Ada 83 extension for VS Code](readme-screenshot.gif)

## At a glance

| | |
|---|---|
| **One translation unit** | `ada83.c`, 64k lines. No generated parser, no build system beyond a makefile, no third-party source. |
| **Whole language** | Tasking, generics, fixed point, representation clauses, exceptions — all of MIL-STD-1815A, not a subset. |
| **3561 / 3561 ACATS** | The 1.11 conformance suite passes in full, every class. |
| **Native code** | LLVM IR straight out of the front end, through a libLLVM loaded at run time. `--ir` if you want the text. |
| **Editor support in the binary** | `ada83 --lsp` is a language server. The extension it drives is one JavaScript file with no npm dependencies. |
| **Nothing to install** | Prebuilt archives for Linux, macOS and Windows, each standalone. |

The standard library is Ada, in one file (`ada83-runtime.ada`), and the
reference manual travels with the compiler (`manual.md`) so a question of
legality is answered out of the standard rather than out of memory.

## Building

| Platform | Command | Notes |
| -------- | ------- | ----- |
| Linux    | `make`  | GCC or Clang; installs libLLVM via the system package manager if absent |
| macOS    | `make`, or run `make.applescript` | Apple Clang; libLLVM via Homebrew |
| Windows  | `make.bat` | GCC, Clang or Zig, whichever is found first; downloads Zig if none is present |

Prebuilt compilers ship with the repository:

| Archive | Contents |
| ------- | -------- |
| [`bin-linux.zip`](bin-linux.zip) | `ada83`, baseline x86_64, and `ada83-runtime.ada`; needs glibc 2.34 or newer, and libLLVM from the system package manager |
| [`bin-macos.zip`](bin-macos.zip) | `ada83`, universal x86_64 + arm64, and `ada83-runtime.ada`; libLLVM via Homebrew |
| [`bin-windows.zip`](bin-windows.zip) | `ada83.exe`, `ada83-runtime.ada`, and `LLVM-C.dll` with its companion DLLs; unzip them together |

`ada83-runtime.ada` holds the standard library, and the compiler looks for it
beside its own executable, so keep the two together when unpacking. To rebuild
an archive from source:

| Platform | Command | Produces |
| -------- | ------- | -------- |
| Linux    | `make package` | `bin-linux.zip` |
| macOS    | `make package`, or `osascript make.applescript package` | `bin-macos.zip`, both slices lipo'd together |
| Windows  | `make.bat package` | `bin-windows.zip`, DLLs included |

The compiler also cross-builds with [Zig](https://ziglang.org) from any host,
for example:

```sh
zig cc -O2 -std=gnu2x -target x86_64-windows-gnu ada83.c -o ada83.exe -lm
zig cc -O2 -std=gnu2x -target aarch64-macos     ada83.c -o ada83 -lm -lpthread
```

## Conformance

Under ACATS 1.11 all 3561 tests pass

| Suite | Category | Passed | Completion |
|:-----:|----------|-------:|-----------:|
| **A** | Acceptance | `140 / 140` | **100%** ✅ |
| **B** | Illegality | `1350 / 1350` | **100%** ✅ |
| **C** | Executable | `1973 / 1973` | **100%** ✅ |
| **D** | Numerics | `17 / 17` | **100%** ✅ |
| **E** | Inspection | `34 / 34` | **100%** ✅ |
| **L** | Post-compilation | `47 / 47` | **100%** ✅ |
| | **Total** | **`3561 / 3561`** | **100%** ✅ |

## Benchmarks

Run time of the generated code at `-O2`, against GNAT 13.3.0 (GCC
`13.3.0-6ubuntu2~24.04.1`), on Linux x86_64 with 4 cpus (Intel Xeon @ 2.10 GHz),
with libLLVM `20.1.2` behind ada83's back end.

| Program | Stresses | ada83 (s) | gnat (s) | Ratio | Result |
|---------|----------|----------:|---------:|------:|-------:|
| **lu** | LU decomposition, float division | `0.058 ± 0.001` | `0.205 ± 0.004` | `0.28` | **3.5× faster** |
| **strings** | slices and character work | `0.057 ± 0.001` | `0.055 ± 0.001` | — | *indistinguishable* |
| **numerics** | fixed point and 12-digit float * | `0.122 ± 0.002` | `0.090 ± 0.001` | `1.36` | 1.4× slower |
| **memory** | allocation and deallocation | `0.382 ± 0.009` | `0.257 ± 0.009` | `1.49` | 1.5× slower |
| **checks** | range and index checks in a hot loop | `0.258 ± 0.004` | `0.164 ± 0.004` | `1.57` | 1.6× slower |
| **sieve** | integer arrays, index checks | `0.087 ± 0.003` | `0.051 ± 0.001` | `1.71` | 1.7× slower |
| **recurse** | call and return | `0.057 ± 0.001` | `0.023 ± 0.000` | `2.48` | 2.5× slower |
| **matmul** | dense float, nested loops | `0.049 ± 0.001` | `0.017 ± 0.001` | `2.88` | 2.9× slower |

## VSCode Extension

`ada83 --lsp` serves the Language Server Protocol on stdin and stdout, so the
outline, the hovers, go-to-definition, completion and the quick fixes in the
recording above are the compiler's own answers rather than a second and lesser
model of Ada living in the editor.

```sh
code --install-extension ada83.vsix
```

`ada83.vsix` is in each of the archives above, and `make vsix` builds it from
`ada83-extension.js`.

## Use

```sh
./ada83 hello.ada -o hello
```

```ada
with Text_IO; use Text_IO;
procedure Hello is
begin
   Put_Line ("Hello, Ada 83!");
end;
```

```
$ ./ada83 hello.ada -o hello
Compiled 'hello.ada' -> 'hello.native.ll'
Generated ALI file 'hello.native.ali'
$ ./hello
Hello, Ada 83!
```

The compiler emits LLVM IR, so the IR can be taken directly:

```sh
./ada83 --ir hello.ada -o hello.ll      # textual LLVM IR
./ada83 --emit-llvm hello.ada -o hello  # native, keeping the optimised IR
./ada83 --ir a.ada b.ada c.ada          # several units, one process each
lli hello.ll                            # interpret the IR
```

## Tests

The ACATS tests are in `tests.zip` and unzipped on first use.

```sh
bash test.sh run all      # every class
bash test.sh run c        # one class
bash test.sh run c45      # one group
bash test.sh check        # run, then diff against the baseline
bash test.sh help
```

Each run writes to its own directory under `test_results/`, with
logs under `acats_logs/`, so concurrent runs don't overwrite eachother.
