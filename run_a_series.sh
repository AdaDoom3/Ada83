#!/bin/bash
mkdir -p test_results

for test in acats/a*.ada; do
  name=$(basename $test .ada)
  timeout 5 bash -c "./ada83 $test && llvm-as /tmp/$name.ll && lli /tmp/$name.bc" > test_results/$name.log 2>&1
  if [ $? -eq 0 ]; then
    echo "PASS: $name"
  else
    echo "FAIL: $name"
  fi
done
