# Hand tests

Regression tests for behavior the ACATS Class C suite does not cover
(written alongside REPRESENTATION.md). Each is a standalone procedure
that prints `DONE` as its last line on success and a `FAIL ...` line
on any failed check:

```
../ada83 packed_static.ada -o /tmp/t.ll && lli /tmp/t.ll
```

- `packed_static.ada` — static-bounds packed boolean arrays: aggregates,
  indexing, unchecked conversion to and from INTEGER, equality.
- `packed_dynamic.ada` — dynamic-bounds packed arrays (`1 .. LEN`),
  arrays of packed rows, nested dynamic indexing, scalar roundtrip.
- `packed_generic_roundtrip.ada` — the ACATS `LENGTH_CHECK` pattern as
  a generic over a private type, with a clause-placed record.
- `packed_nested_rows.ada` — writes through dynamic row and element
  indices land on the right bits and nowhere else.
- `packed_slices_and_actuals.ada` — misaligned-phase slice assignment,
  slice equality, and OUT / IN OUT packed element actuals.
