# Ada83 

[![Linux](https://github.com/AdaDoom3/Ada83/actions/workflows/ci-linux.yml/badge.svg)](https://github.com/AdaDoom3/Ada83/actions/workflows/ci-linux.yml)
[![macOS](https://github.com/AdaDoom3/Ada83/actions/workflows/ci-macos.yml/badge.svg)](https://github.com/AdaDoom3/Ada83/actions/workflows/ci-macos.yml)
[![Windows](https://github.com/AdaDoom3/Ada83/actions/workflows/ci-windows.yml/badge.svg)](https://github.com/AdaDoom3/Ada83/actions/workflows/ci-windows.yml)

A single-file Ada 83 LLVM compiler.

![The Ada 83 extension for VS Code](readme-demo.gif)

| | |
|---|---|
| Compiler | `ada83.c`, 64k lines, no generated code, no third-party source |
| Runtime | `ada83-runtime.ada`, 3k lines of Ada |
| Language | all of MIL-STD-1815A: tasking, generics, fixed point, representation clauses |
| Conformance | 3561 / 3561, ACATS 1.11 |
| Targets | Linux, macOS, Windows |

## Quick start

From the [latest release](https://github.com/AdaDoom3/Ada83/releases/latest),
unpack the archive for your platform: `bin-linux.zip`, `bin-macos.zip` or
`bin-windows.zip`

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
| macOS    | `make.applescript` | Apple Clang; libLLVM via Homebrew |
| Windows  | `make.bat` | GCC or Clang; offers to fetch Zig if neither is installed |

Every script writes what it builds into `bin-<target>/` - `bin-linux/ada83`,
`bin-macos/ada83`, `bin-windows\ada83.exe` with the DLLs it loads beside it.
The DLLs are vendored in `bin-dll.zip`, which holds nothing else; the
release workflow zips each finished `bin-<target>/` into the archives it
publishes.

`ada83-runtime.ada` holds the standard library, and the compiler looks for it
beside its own executable.

| Platform | Command | Produces |
| -------- | ------- | -------- |
| Linux    | `make package` | `bin-linux/`, extension and artwork included |
| macOS    | `osascript make.applescript package` | `bin-macos/`, both slices together |
| Windows  | `make.bat package` | `bin-windows/`, DLLs included |

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

## Extensions

Two departures from bare MIL-STD-1815A, neither observable from within a
conforming program:

**Linker naming.** A library subprogram's symbol is prefixed `_ada_`, in the
GNAT manner, so `procedure Main` cannot collide with the C entry point and a
procedure named `Read` or `Sleep` cannot interpose on libc. Package members
keep their `package__name` form; names given through `pragma Import` and
`pragma Export` are never prefixed. The standard says nothing about object
code, so this is linker territory, not language territory.

**Command_Line.** The generated entry point captures `argc`/`argv`,
and a vendor runtime package exposes them, mirroring Ada 95's
`Ada.Command_Line`:

```ada
package Command_Line is
  function Argument_Count return Natural;
  function Argument (Number : Positive) return String;
  function Command_Name return String;
end;
```

`Argument (1)` is the first argument after the program name; a `Number` beyond
`Argument_Count` raises `Constraint_Error`. Both extensions are covered by the
test programs under `extensions/` in `tests.zip`, which `test.sh` runs as the
last stage of a full run.

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
| `ada83.compilerPath` | Location of `ada83` and where `${workspaceFolder}` gets substituted |
| `ada83.includePaths` | Directories for with-ed units |
| `ada83.language` | Language error messages are read in |
| `ada83.formatOnType` | Reindent each line as you type it |
| `ada83.formatOnSave` | Reformat the whole file as it is saved, by asking a model |
| `ada83.formatStrength` | How much a reformat may change: `indentation`, `layout` or `style` |
| `ada83.trace.server` | Write the protocol traffic to the output channel |

## Use

The compiler emits LLVM IR, so the IR can be taken directly:

```sh
./ada83 --ir hello.ada -o hello.ll      # Textual LLVM IR
./ada83 --emit-llvm hello.ada -o hello  # Native, keeping the optimised IR
./ada83 --ir a.ada b.ada c.ada          # Several units, one process each
./ada83 hello.ll world.ll -o hello      # Link .ll modules, no source needed
lli hello.ll                            # Interpret the IR
```

## Tests

The ACATS tests are in `tests.zip` and unzipped on first use.

```sh
bash test.sh            # Every class, then the extensions -- the default
bash test.sh run c      # One class
bash test.sh run c45    # One group
bash test.sh check      # Run, then diff against the baseline
bash test.sh extensions # Only the extension tests
bash test.sh help
```

A full run ends with the extension stage: Ada programs under `extensions/` in
the same `tests.zip`, covering what ACATS cannot see. Each one self-reports
PASSED or FAILED, and comment headers tell the harness what to do around the
run — `-- ARGS:` for command-line arguments, `-- LINK:` for a separately
compiled unit, `-- SYMBOL:` / `-- SYMBOL-NOT:` for what nm must find in the
executable. They are counted apart from the ACATS classes, as `X=` and `XF=`
in the run summary.

## Release Workflow

1. Update `ada83.c` with `ADA83_VERSION_MINOR` or `ADA83_VERSION_MAJOR` through a normal PR and merge to main.
2. Update git with `git tag v1.0 && git push origin v1.0`
3. Allow `release.yml` to verify the tag, build and packages all platforms and publishes.

The tag gate refuses to publish unless the tag matches `ADA83_VERSION_*` and no release exists under that tag. So a tag on an unmerged branch, or one that disagrees.
