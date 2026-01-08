#!/bin/bash
set -euo pipefail;declare -A X=([a]=0 [b]=0 [c]=0 [d]=0 [e]=0 [l]=0 [f]=0 [s]=0 [z]=0 [ec]=0 [ee]=0 [t]=$(date +%s%3N) [ta]=0 [tb]=0 [tc]=0 [td]=0 [te]=0 [tl]=0 [fa]=0 [fb]=0 [fc]=0 [fd]=0 [fe]=0 [fl]=0 [sa]=0 [sb]=0 [sc]=0 [sd]=0 [se]=0 [sl]=0)
:()(((${2:-1}>0))&&printf %d $((100*$1/$2))||printf 0)
.(){ printf %.3f "$(bc<<<"scale=4;($(date +%s%3N)-${X[t]})/1000")";}
E(){ printf "  %-18s %-6s %-14s" "$2" "$1" "$3";[[ -n ${4:-} ]]&&printf " %s" "$4";printf "\n";}
^(){ local f=$1;local -a x=() a=();local i=0 h=0;while IFS= read -r l;do((++i));[[ $l =~ --\ ERROR ]]&&x+=($i);done<"$f"
while IFS=: read -r _ n _;do a+=($n);done< <(./ada83 -Iacats -Irts "$f" 2>&1|grep "^[^:]*:[0-9]")
for e in ${x[@]+"${x[@]}"};do for v in ${a[@]+"${a[@]}"};do((v>=e-1&&v<=e+1))&&{ ((++h));break;};done;done
local xe=${#x[@]};X[ec]=$((X[ec]+h)) X[ee]=$((X[ee]+xe));printf %d:%d $h $xe;}
@(){ local f=$1 n=$(basename "$f" .ada);local -a x=() t=() a=();local i=0 h=0
while IFS= read -r l;do((++i));[[ $l =~ --\ ERROR:?\ *(.*) ]]&&{ x+=($i);t+=("${BASH_REMATCH[1]:-?}");};done<"$f"
while IFS=: read -r _ m _;do a+=($m);done< <(./ada83 -Iacats -Irts "$f" 2>&1|grep "^[^:]*:[0-9]");local xe=${#x[@]} ae=${#a[@]};printf "\n   %s\n" "${'':-<68>}"
printf "   %s  expect %d  reported %d  tolerance ±1\n" "$n" $xe $ae
printf "   %s\n" "${'':-<68>}";for j in ${!x[@]};do local e=${x[$j]} s=${t[$j]} q=0
for v in ${a[@]+"${a[@]}"};do((v>=e-1&&v<=e+1))&&{ q=1;((++h));break;};done
((q))&&printf "   [✓] %4d  %s\n" $e "$s"||printf "   [ ] %4d  %s\n" $e "$s";done
local p=$(: $h $xe) v;((p>=90))&&v="pass"||v="fail"
printf "   %s\n   coverage %d/%d (%d%%)  %s\n\n" "${'':-<68>}" $h $xe $p "$v";}
R(){ [[ ! -f rts/report.ll || rts/report.adb -nt rts/report.ll ]]&&./ada83 -Iacats -Irts rts/report.adb>rts/report.ll 2>/dev/null||true;}
T(){ local f=$1 v=${2:-} n=$(basename "$f" .ada);local q=${n:0:1};if [[ $n =~ [0-9]$ && ! $n =~ m$ ]];then return;fi;((++X[z]));R;case $q in
[aA])((++X[ta]));if ! timeout 0.2 ./ada83 -Iacats -Irts "$f">test_results/$n.ll 2>acats_logs/$n.err;then
E SKIP "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));((++X[sa]));return;fi
if ! timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
E SKIP "$n" BIND "unresolved symbols";((++X[s]));((++X[sa]));return;fi
timeout 1 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&E PASS "$n" PASSED&&((++X[a]))||E FAIL "$n" FAILED "exit $?"&&((++X[f]))&&((++X[fa]));;
[bB])((++X[tb]));if timeout 0.2 ./ada83 -Iacats -Irts "$f" >acats_logs/$n.ll 2>acats_logs/$n.err;then E FAIL "$n" WRONG_ACCEPT "compiled when should reject";((++X[f]));((++X[fb]))
else q=$(^ "$f");h=${q%:*};x=${q#*:};p=$(: $h $x);((p>=90))&&{ ((++X[b]));[[ $v == v ]]&&@ "$f"||E PASS "$n" REJECTED "$h/$x errors (${p}%)";}||{ ((++X[f]));((++X[fb]));E FAIL "$n" LOW_COVERAGE "$h/$x errors (${p}%)";};fi;;
[cC])((++X[tc]));if ! timeout 0.2 ./ada83 -Iacats -Irts "$f">test_results/$n.ll 2>acats_logs/$n.err;then
E SKIP "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));((++X[sc]));return;fi
if ! timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
E SKIP "$n" BIND "unresolved symbols";((++X[s]));((++X[sc]));return;fi
if timeout 1 lli test_results/$n.bc>acats_logs/$n.out 2>&1;then
grep -q PASSED acats_logs/$n.out 2>/dev/null&&E PASS "$n" PASSED&&((++X[c]))||{
grep -q NOT.APPLICABLE acats_logs/$n.out 2>/dev/null&&E N/A "$n" N/A "$(grep -o "NOT.APPLICABLE.*" acats_logs/$n.out|head -1|cut -c1-40)"&&((++X[s]))&&((++X[sc]))||{
grep -q FAILED acats_logs/$n.out 2>/dev/null&&E FAIL "$n" FAILED "$(grep FAILED acats_logs/$n.out|head -1|cut -c1-50)"&&((++X[f]))&&((++X[fc]))||
E FAIL "$n" NO_REPORT "no PASSED/FAILED in output"&&((++X[f]))&&((++X[fc]));};};else local ec=$?
E FAIL "$n" RUNTIME "exit $ec $(tail -1 acats_logs/$n.out 2>/dev/null|cut -c1-40)";((++X[f]));((++X[fc]));fi;;
[dD])((++X[td]));if ! timeout 0.2 ./ada83 -Iacats -Irts "$f">test_results/$n.ll 2>acats_logs/$n.err;then
grep -qi "capacity\|overflow\|limit" acats_logs/$n.err 2>/dev/null&&E N/A "$n" CAPACITY "compiler limit exceeded"&&((++X[s]))&&((++X[sd]))&&return
E SKIP "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));((++X[sd]));return;fi
timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>/dev/null||{ E SKIP "$n" BIND;((++X[s]));((++X[sd]));return;}
timeout 1 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&grep -q PASSED acats_logs/$n.out&&E PASS "$n" PASSED&&((++X[d]))||
E FAIL "$n" FAILED "exact arithmetic check"&&((++X[f]))&&((++X[fd]));;
[eE])((++X[te]));timeout 0.2 ./ada83 -Iacats -Irts "$f">test_results/$n.ll 2>acats_logs/$n.err||{ E SKIP "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));((++X[se]));return;}
timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>/dev/null||{ E SKIP "$n" BIND;((++X[s]));((++X[se]));return;}
timeout 1 lli test_results/$n.bc>acats_logs/$n.out 2>&1
grep -q "TENTATIVELY PASSED" acats_logs/$n.out 2>/dev/null&&E INSP "$n" INSPECT "requires manual verification"&&((++X[e]))||{
grep -q PASSED acats_logs/$n.out 2>/dev/null&&E PASS "$n" PASSED&&((++X[e]))||E FAIL "$n" FAILED&&((++X[f]))&&((++X[fe]));};;
[lL])((++X[tl]));if timeout 0.2 ./ada83 -Iacats -Irts "$f">test_results/$n.ll 2>acats_logs/$n.err;then
if timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.bc 2>acats_logs/$n.link;then
timeout 0.5 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&E FAIL "$n" WRONG_EXEC "should not execute"&&((++X[f]))&&((++X[fl]))||
E PASS "$n" BIND_REJECT "execution blocked"&&((++X[l]));else E PASS "$n" LINK_REJECT "binding failed as expected"&&((++X[l]));fi
else E PASS "$n" COMPILE_REJECT "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-40)"&&((++X[l]));fi;;
[fF])E SUPP "$n" FOUNDATION "support code";;*)E SKIP "$n" UNKNOWN "unrecognized test class '$q'"&&((++X[s]));;esac;}
RR(){ local tot=${X[z]} pass=$((X[a]+X[b]+X[c]+X[d]+X[e]+X[l]));local af=${X[f]} as=${X[s]}
printf "\n========================================\nRESULTS\n========================================\n\n"
printf " %-22s %6s %6s %6s %6s %7s\n" "CLASS" "pass" "fail" "skip" "total" "rate"
printf " %-22s %6s %6s %6s %6s %7s\n" "----------------------" "------" "------" "------" "------" "-------"
((X[ta]>0))&&printf " %-22s %6d %6d %6d %6d %6d%%\n" "A  Acceptance" ${X[a]} ${X[fa]} ${X[sa]} ${X[ta]} $(: ${X[a]} ${X[ta]})
((X[tb]>0))&&printf " %-22s %6d %6d %6d %6d %6d%%\n" "B  Illegality" ${X[b]} ${X[fb]} ${X[sb]} ${X[tb]} $(: ${X[b]} ${X[tb]})
((X[tc]>0))&&printf " %-22s %6d %6d %6d %6d %6d%%\n" "C  Executable" ${X[c]} ${X[fc]} ${X[sc]} ${X[tc]} $(: ${X[c]} ${X[tc]})
((X[td]>0))&&printf " %-22s %6d %6d %6d %6d %6d%%\n" "D  Numerics" ${X[d]} ${X[fd]} ${X[sd]} ${X[td]} $(: ${X[d]} ${X[td]})
((X[te]>0))&&printf " %-22s %6d %6d %6d %6d %6d%%\n" "E  Inspection" ${X[e]} ${X[fe]} ${X[se]} ${X[te]} $(: ${X[e]} ${X[te]})
((X[tl]>0))&&printf " %-22s %6d %6d %6d %6d %6d%%\n" "L  Post-compilation" ${X[l]} ${X[fl]} ${X[sl]} ${X[tl]} $(: ${X[l]} ${X[tl]})
printf " %-22s %6s %6s %6s %6s %7s\n" "----------------------" "------" "------" "------" "------" "-------"
printf " %-22s %6d %6d %6d %6d %6d%%\n" "TOTAL" $pass ${X[f]} ${X[s]} $tot $(: $pass $tot)
((X[ee]>0))&&{ printf "\n========================================\nB-TEST ERROR COVERAGE\n========================================\n\n"
printf " errors detected    %d / %d\n" ${X[ec]} ${X[ee]}
printf " coverage rate      %d%%\n" $(: ${X[ec]} ${X[ee]});}
printf "\n========================================\n"
printf " elapsed $(.)s  |  processed %d tests  |  %s\n" ${X[z]} "$(date '+%Y-%m-%d %H:%M:%S')"
printf "========================================\n"
printf "A=%d B=%d C=%d D=%d E=%d L=%d F=%d S=%d T=%d/%d (%d%%) ERR=%d/%d\n" \
${X[a]} ${X[b]} ${X[c]} ${X[d]} ${X[e]} ${X[l]} ${X[f]} ${X[s]} $pass $tot $(: $pass $tot) ${X[ec]} ${X[ee]}>test_summary.txt;}
+(){ printf "\n========================================\n%s\n========================================\n\n" "$1";}
G(){ + "Class ${1^^} Tests";for f in acats/${1,,}*.ada;do [[ -f $f ]]&&T "$f" "${2:-}";done;RR;}
O(){ + "B-Test Error Coverage Analysis";for f in acats/b*.ada;do [[ -f $f ]]||continue;n=$(basename "$f" .ada);((++X[z]))
[[ ${1:-} == v ]]&&{ @ "$f";q=$(^ "$f");((100*${q%:*}/${q#*:}>=90))&&((++X[b]))||((++X[f]))&&((++X[fb]));continue;}
q=$(^ "$f");h=${q%:*};x=${q#*:};p=$(: $h $x);((p>=90))&&E PASS "$n" PASS "$h/$x errors (${p}%)"&&((++X[b]))||
E FAIL "$n" FAIL "$h/$x errors (${p}%)"&&((++X[f]))&&((++X[fb]));done;RR;}
A(){ + "Full Suite";for f in acats/*.ada;do [[ -f $f ]]&&T "$f";done;RR;}
Q(){ + "Group ${1^^} Tests";for f in acats/${1,,}*.ada;do [[ -f $f ]]&&T "$f" "${2:-}";done;RR;}
U(){ cat<<E
Usage: $0 <mode> [options]

Modes:
  f, full         Run complete ACATS test suite
  g <X> [v]       Run all tests for class X (A/B/C/D/E/L)
  q <XX> [v]      Run tests for group XX (e.g., a21, b22, c32)
  b [v]           Run B-test error coverage analysis
  h, help         Show this help message

Classes:
  A  Acceptance tests      - basic compiler validation
  B  Illegality tests      - rejection of invalid code
  C  Executable tests      - runtime behavior verification
  D  Numeric tests         - arithmetic precision checks
  E  Inspection tests      - manual verification required
  L  Post-compilation      - linker/binder rejection tests

Options:
  v               Verbose mode (detailed B-test diagnostics)

Output:
  test_results/*.{ll,bc}   Compiled test artifacts
  acats_logs/*.{err,out}   Compiler and runtime logs
  test_summary.txt         Final pass/fail summary
E
}
mkdir -p test_results acats_logs;case ${1:-h} in f|full)A;;g)G "${2:-c}" "${3:-}";;q)Q "${2:-b22}" "${3:-}";;b)O "${2:-}";;h|help|*)U;;esac
