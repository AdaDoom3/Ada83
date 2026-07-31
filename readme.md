# Ada83 

A single-file Ada 83 LLVM compiler.

![The Ada 83 extension for VS Code](readme-screenshot.gif)

| | |
|---|---|
| Compiler | `ada83.c`, 64k lines, no generated code, no third-party source |
| Runtime | `ada83-runtime.ada`, 3k lines of Ada |
| Language | all of MIL-STD-1815A: tasking, generics, fixed point, representation clauses |
| Conformance | 3561 / 3561, ACATS 1.11 |
| Output | native, through a libLLVM loaded at run time; `--ir` for the text |
| Editor | `ada83 --lsp`, with a VS Code extension in one dependency-free file |
| Binaries | Linux, macOS, Windows |
| Reference | `manual.md`, the standard itself |

## Quick start

Unpack the archive for your platform — [`bin-linux.zip`](bin-linux.zip),
[`bin-macos.zip`](bin-macos.zip) or [`bin-windows.zip`](bin-windows.zip)

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

For the editor, install the extension that came in the same archive:

```sh
code --install-extension ada83.vsix
```

It needs `ada83` on your PATH, or `ada83.compilerPath` set to where you
unpacked it.

## Building

| Platform | Command | Notes |
| -------- | ------- | ----- |
| Linux    | `make`  | GCC or Clang; installs libLLVM via the system package manager if absent |
| macOS    | `make`, or run `make.applescript` | Apple Clang; libLLVM via Homebrew |
| Windows  | `make.bat` | GCC or Clang; offers to fetch Zig if neither is installed |

Prebuilt compilers ship with the repository:

| Archive | Contents |
| ------- | -------- |
| [`bin-linux.zip`](bin-linux.zip) | `ada83`, baseline x86_64, plus `ada83-runtime.ada` and `ada83.vsix`; needs glibc 2.34 or newer, and libLLVM from the system package manager |
| [`bin-macos.zip`](bin-macos.zip) | `ada83`, universal x86_64 + arm64, plus `ada83-runtime.ada` and `ada83.vsix`; macOS 11 or newer, libLLVM via Homebrew |
| [`bin-windows.zip`](bin-windows.zip) | `ada83.exe`, `ada83-runtime.ada`, `ada83.vsix`, and `LLVM-C.dll` with its companion DLLs; unzip them together. Linking native executables wants clang with lld, which resolves the weak externals thread-local storage becomes on COFF |

`ada83-runtime.ada` holds the standard library, and the compiler looks for it
beside its own executable.

| Platform | Command | Produces |
| -------- | ------- | -------- |
| Linux    | `make package` | `bin-linux.zip` |
| macOS    | `make package`, or `osascript make.applescript package` | `bin-macos.zip`, both slices lipo'd together |
| Windows  | `make.bat package` | `bin-windows.zip`, DLLs included |

Each script builds the platform's icon from the one `ada83-icon.png` as it
packages.

| Command | Produces | Wants |
| ------- | -------- | ----- |
| `make package TARGET=windows` | `bin-windows.zip` | `x86_64-w64-mingw32-gcc` |
| `make package TARGET=macos` | `bin-macos.zip` | `o64-clang` and `lipo`, or a Mac |
| `make package TARGET=linux` | `bin-linux.zip` | `x86_64-linux-gnu-gcc` |

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

`ada83 --lsp` serves the Language Server Protocol on stdin and stdout.

Error messages can be read in another language. `ada83.language` picks one,
and anything but English hands the message to the editor's model.

| `ada83.language` | |
| ---------------- | --- |
| `en` | English, as the compiler writes it — no model, no request |
| `es` | Spanish |
| `fr` | French |
| `de` | German |
| `zh-CN` | Chinese (Simplified) |
| `ja` | Japanese |
| `hi` | Hindi |
| `lolcat` | `O NOES 'Put_Lin' IS NOT CALLABUL. SRSLY.` |

| Setting | |
| ------- | --- |
| `ada83.compilerPath` | where `ada83` is; `${workspaceFolder}` is substituted |
| `ada83.includePaths` | directories searched for with-ed units |
| `ada83.language` | the language error messages are read in |
| `ada83.formatOnType` | reindent each line as you type it, worked out locally |
| `ada83.formatOnSave` | reformat the whole file as it is saved, by asking a model |
| `ada83.formatStrength` | how much a reformat may change: `indentation`, `layout` or `style` |
| `ada83.trace.server` | write the protocol traffic to the output channel |

## Use

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
