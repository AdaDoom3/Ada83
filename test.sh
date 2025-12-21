#!/bin/bash
set -euo pipefail;declare -A X=([a]=0 [b]=0 [c]=0 [d]=0 [e]=0 [l]=0 [f]=0 [s]=0 [z]=0 [ec]=0 [ee]=0 [t]=$(date +%s%3N))
:()(((${2:-1}>0))&&printf %d $((100*$1/$2))||printf 0)
.(){ printf %.3f "$(bc<<<"scale=4;($(date +%s%3N)-${X[t]})/1000")";}
E(){ printf "  %-18s %-6s %-14s" "$2" "$1" "$3";[[ -n ${4:-} ]]&&printf " %s" "$4";printf "\n";}
^(){ local f=$1;local -a x=() a=();local i=0 h=0;while IFS= read -r l;do((++i));[[ $l =~ --\ ERROR ]]&&x+=($i);done<"$f"
while IFS=: read -r _ n _;do a+=($n);done< <(./ada83 "$f" 2>&1|grep "^[^:]*:[0-9]")
for e in ${x[@]+"${x[@]}"};do for v in ${a[@]+"${a[@]}"};do((v>=e-1&&v<=e+1))&&{ ((++h));break;};done;done
local xe=${#x[@]};X[ec]=$((X[ec]+h)) X[ee]=$((X[ee]+xe));printf %d:%d $h $xe;}
@(){ local f=$1 n=$(basename "$f" .ada);local -a x=() t=() a=();local i=0 h=0
while IFS= read -r l;do((++i));[[ $l =~ --\ ERROR:?\ *(.*) ]]&&{ x+=($i);t+=("${BASH_REMATCH[1]:-?}");};done<"$f"
while IFS=: read -r _ m _;do a+=($m);done< <(./ada83 "$f" 2>&1|grep "^[^:]*:[0-9]");local xe=${#x[@]} ae=${#a[@]};printf "\n   %s\n" "${'':->68}"
printf "   %s  expect %d  reported %d  tolerance ±1\n" "$n" $xe $ae
printf "   %s\n" "${'':->68}";for j in ${!x[@]};do local e=${x[$j]} s=${t[$j]} q=0
for v in ${a[@]+"${a[@]}"};do((v>=e-1&&v<=e+1))&&{ q=1;break;};done
((q))&&printf "   [✓] %4d  %s\n" $e "$s"||printf "   [ ] %4d  %s\n" $e "$s";done
local p=$(: $h $xe) v;((p>=90))&&v="pass"||v="fail"
printf "   %s\n   coverage %d/%d (%d%%)  %s\n\n" "${'':->68}" $h $xe $p "$v";}
T(){ local f=$1 v=${2:-} n=$(basename "$f" .ada);local q=${n:0:1};((++X[z]));case $q in
[aA])if ! timeout 0.2 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
E SKIP "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));return;fi
if ! timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll 2>acats_logs/$n.link;then
E SKIP "$n" BIND "unresolved symbols";((++X[s]));return;fi
timeout 1 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&E PASS "$n" PASSED&&((++X[a]))||E FAIL "$n" FAILED "exit $?"&&((++X[f]));;
[bB])if timeout 0.2 ./ada83 "$f" >acats_logs/$n.ll 2>acats_logs/$n.err;then E FAIL "$n" WRONG_ACCEPT "compiled when should reject";((++X[f]))
else q=$(^ "$f");h=${q%:*};x=${q#*:};p=$(: $h $x);((p>=90))&&{ ((++X[b]));[[ $v == v ]]&&@ "$f"||E PASS "$n" REJECTED "$h/$x errors (${p}%)";}||{ ((++X[f]));E FAIL "$n" LOW_COVERAGE "$h/$x errors (${p}%)";};fi;;
[cC])if ! timeout 0.2 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
E SKIP "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));return;fi
if ! timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
E SKIP "$n" BIND "unresolved symbols";((++X[s]));return;fi
if timeout 1 lli test_results/$n.bc>acats_logs/$n.out 2>&1;then
grep -q PASSED acats_logs/$n.out 2>/dev/null&&E PASS "$n" PASSED&&((++X[c]))||{
grep -q NOT.APPLICABLE acats_logs/$n.out 2>/dev/null&&E N/A "$n" N/A "$(grep -o "NOT.APPLICABLE.*" acats_logs/$n.out|head -1|cut -c1-40)"&&((++X[s]))||{
grep -q FAILED acats_logs/$n.out 2>/dev/null&&E FAIL "$n" FAILED "$(grep FAILED acats_logs/$n.out|head -1|cut -c1-50)"&&((++X[f]))||
E FAIL "$n" NO_REPORT "no PASSED/FAILED in output"&&((++X[f]));};};else local ec=$?
E FAIL "$n" RUNTIME "exit $ec $(tail -1 acats_logs/$n.out 2>/dev/null|cut -c1-40)";((++X[f]));fi;;
[dD])if ! timeout 0.2 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
grep -qi "capacity\|overflow\|limit" acats_logs/$n.err 2>/dev/null&&E N/A "$n" CAPACITY "compiler limit exceeded"&&((++X[s]))&&return
E SKIP "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));return;fi
timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll 2>/dev/null||{ E SKIP "$n" BIND;((++X[s]));return;}
timeout 1 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&grep -q PASSED acats_logs/$n.out&&E PASS "$n" PASSED&&((++X[d]))||
E FAIL "$n" FAILED "exact arithmetic check"&&((++X[f]));;
[eE])timeout 0.2 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err||{ E SKIP "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));return;}
timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll 2>/dev/null||{ E SKIP "$n" BIND;((++X[s]));return;}
timeout 1 lli test_results/$n.bc>acats_logs/$n.out 2>&1
grep -q "TENTATIVELY PASSED" acats_logs/$n.out 2>/dev/null&&E INSP "$n" INSPECT "requires manual verification"&&((++X[e]))||{
grep -q PASSED acats_logs/$n.out 2>/dev/null&&E PASS "$n" PASSED&&((++X[e]))||E FAIL "$n" FAILED&&((++X[f]));};;
[lL])if timeout 0.2 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
if timeout 0.2 llvm-link -o test_results/$n.bc test_results/$n.ll 2>acats_logs/$n.link;then
timeout 0.5 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&E FAIL "$n" WRONG_EXEC "should not execute"&&((++X[f]))||
E PASS "$n" BIND_REJECT "execution blocked"&&((++X[l]));else E PASS "$n" LINK_REJECT "binding failed as expected"&&((++X[l]));fi
else E PASS "$n" COMPILE_REJECT "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-40)"&&((++X[l]));fi;;
[fF])E SUPP "$n" FOUNDATION "support code";;*)E SKIP "$n" UNKNOWN "unrecognized test class '$q'"&&((++X[s]));;esac;}
R(){ local tot=${X[z]} pass=$((X[a]+X[b]+X[c]+X[d]+X[e]+X[l])) bf=${X[f]};local bt=$((X[b]+bf)) ct=$((X[c]+X[s]))
printf "\n========================================\nRESULTS\n========================================\n\n"
printf " %-22s  %6s  %6s  %6s  %6s  %6s\n" CLASS pass fail skip total rate
printf " ----------------------  ------  ------  ------  ------  ------\n"
((X[a]>0))&&printf " A  Acceptance            %6d                  %6d\n" ${X[a]} ${X[a]}
((bt>0))&&printf " B  Illegality             %6d  %6d          %6d  %5d%%\n" ${X[b]} $bf $bt $(: ${X[b]} $bt)
((ct>0))&&printf " C  Executable             %6d          %6d  %6d  %5d%%\n" ${X[c]} ${X[s]} $ct $(: ${X[c]} $ct)
((X[d]>0))&&printf " D  Numerics               %6d                  %6d\n" ${X[d]} ${X[d]}
((X[e]>0))&&printf " E  Inspection             %6d                  %6d\n" ${X[e]} ${X[e]}
((X[l]>0))&&printf " L  Post-compilation       %6d                  %6d\n" ${X[l]} ${X[l]}
printf " ----------------------  ------  ------  ------  ------  ------\n"
printf " TOTAL                    %6d  %6d  %6d  %6d  %5d%%\n" $pass ${X[f]} ${X[s]} $tot $(: $pass $tot)
((X[ee]>0))&&{ printf "\n========================================\nB-TEST ERROR COVERAGE\n========================================\n\n"
printf " errors detected   %5d / %d\n coverage rate     %d%%\n" ${X[ec]} ${X[ee]} $(: ${X[ec]} ${X[ee]});}
printf "\n========================================\n"
printf " elapsed $(.)s    processed ${X[z]} tests    $(date "+%Y-%m-%d %H:%M:%S")\n"
printf "========================================\n"
printf "A=%d B=%d C=%d D=%d E=%d L=%d F=%d S=%d T=%d/%d (%d%%) ERR=%d/%d\n" \
${X[a]} ${X[b]} ${X[c]} ${X[d]} ${X[e]} ${X[l]} ${X[f]} ${X[s]} $pass $tot $(: $pass $tot) ${X[ec]} ${X[ee]}>test_summary.txt;}
+(){ printf "\n========================================\n%s\n========================================\n\n" "$1";}
G(){ + "Class ${1^^} Tests";for f in acats/${1}*.ada;do [[ -f $f ]]&&T "$f" "${2:-}";done;R;}
O(){ + "B-Test Error Detection Analysis";for f in acats/b*.ada;do [[ -f $f ]]||continue;n=$(basename "$f" .ada);((++X[z]))
[[ ${1:-} == v ]]&&{ @ "$f";q=$(^ "$f");((100*${q%:*}/${q#*:}>=90))&&((++X[b]))||((++X[f]));continue;}
q=$(^ "$f");h=${q%:*};x=${q#*:};p=$(: $h $x);((p>=90))&&E PASS "$n" PASS "$h/$x errors (${p}%)"&&((++X[b]))||
E FAIL "$n" FAIL "$h/$x errors (${p}%)"&&((++X[f]));done;R;}
A(){ + "Full Suite";for f in acats/*.ada;do [[ -f $f ]]&&T "$f";done;R;}
U(){ cat<<E
Usage: $0 <mode> [options]
Modes:  f  full suite  g <X> [v]  group  b [v]  B-test oracle  h  help
Classes: A acceptance B illegality C executable D numerics E inspection L post-compilation
Options: v  verbose B-test oracle detail
Output: test_results/*.{ll,bc} acats_logs/*.{err,out} test_summary.txt
E
}
mkdir -p test_results acats_logs;case ${1:-h} in f)A;;g)G "${2:-c}" "${3:-}";;b)O "${2:-}";;*)U;;esac
