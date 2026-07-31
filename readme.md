# Ada83

An Ada 83 (ANSI/MIL-STD-1815A) compiler in a single C file, with an LLVM
backend.

## Conformance

For ACATS 1.11 all 3561 of its tests pass.

```
 A  Acceptance        ..............................   140/140   100%
 B  Illegality        .............................. 1350/1350   100%
 C  Executable        .............................. 1973/1973   100%
 D  Numerics          ..............................     17/17   100%
 E  Inspection        ..............................     34/34   100%
 L  Post-compilation  ..............................     47/47   100%
                      ──────────────────────────────
    Total             .............................. 3561/3561   100%
```

## Use

The default output is a native executable, produced through LLVM:

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

The compiler emits LLVM IR, loads libLLVM, runs the pass pipeline, emits an
object file, and calls the system linker. The IR can be taken directly:

```sh
./ada83 --ir hello.ada -o hello.ll      # textual LLVM IR
./ada83 --emit-llvm hello.ada -o hello  # native, keeping the optimised IR
./ada83 --ir a.ada b.ada c.ada          # several units, one process each
lli hello.ll                            # interpret the IR
```

## Tests

The suite ships as `tests.zip` and is unpacked by the harness on first use.

```sh
bash test.sh run all      # every class
bash test.sh run c        # one class
bash test.sh run c45      # one group
bash test.sh check        # run, then diff against the baseline
bash test.sh help
```

Each run writes to its own timestamped directory under `test_results/`, with
logs under `acats_logs/`, so concurrent runs do not overwrite one another.

## Benchmarks

`bench.sh` exists to aim work on `ada83.c` and then judge it.

```sh
bash bench.sh stages            # front end against back end
bash bench.sh parser            # front end against input size
bash bench.sh corpus            # throughput, and the slowest inputs named
bash bench.sh compare /tmp/old  # this build against another, with deltas
bash bench.sh profile           # the functions compiling spends its time in
bash bench.sh codegen           # run time of the generated code
bash bench.sh memory            # peak resident set, compiling and running
bash bench.sh help
```

To judge a change, keep the old binary and name it:

```sh
cp ada83 /tmp/before && make && bash bench.sh compare /tmp/before
```
