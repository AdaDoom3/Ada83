#!/bin/bash
set -e;g=${1:-b22};./test.sh q $g 2>&1|awk '/^  [a-z]/{p+=$2~/PASS|PASS/;f+=$2~/FAIL/;t++}END{printf"%s %d/%d %.0f%%\n","'$g'",p,t,100*p/t}'
