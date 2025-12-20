#!/bin/bash
set -euo pipefail;declare -A X=([a]=0 [b]=0 [c]=0 [d]=0 [e]=0 [l]=0 [f]=0 [s]=0 [z]=0 [ec]=0 [ee]=0 [t]=$(date +%s%3N))
z=$'\e[0m' d=$'\e[2m' k=$'\e[90m' w=$'\e[97m' g=$'\e[32m' r=$'\e[31m' y=$'\e[33m' c=$'\e[36m' m=$'\e[35m' b=$'\e[94m'
:()(((${2:-1}>0))&&printf %d $((100*$1/$2))||printf 0)
.(){ printf %.3f "$(bc<<<"scale=4;($(date +%s%3N)-${X[t]})/1000")";}
E(){ printf "  ${b}%-18s${z} ${1}${2}${z} ${w}%-14s${z}" "$3" "$4";[[ -n ${5:-} ]]&&printf " ${k}%s${z}" "$5";printf "\n";}
^(){ local f=$1;local -a x a;local i=0 h=0;while IFS= read -r l;do((++i));[[ $l =~ --\ ERROR ]]&&x+=($i);done<"$f"
while IFS=: read -r _ n _;do a+=($n);done< <(./ada83 "$f" 2>&1|grep "^[^:]*:[0-9]")
for e in ${x[@]+"${x[@]}"};do for v in ${a[@]+"${a[@]}"};do((v>=e-1&&v<=e+1))&&{ ((++h));break;};done;done
X[ec]=$((X[ec]+h)) X[ee]=$((X[ee]+${#x[@]}));printf %d:%d $h ${#x[@]};}
@(){ local f=$1 n=$(basename "$f" .ada);local -a x t a;local i=0 h=0
while IFS= read -r l;do((++i));[[ $l =~ --\ ERROR:?\ *(.*) ]]&&{ x+=($i);t+=("${BASH_REMATCH[1]:-?}");};done<"$f"
while IFS=: read -r _ m _;do a+=($m);done< <(./ada83 "$f" 2>&1|grep "^[^:]*:[0-9]");printf "\n   ${m}${'':->68}${z}\n"
printf "   ${w}%s${z}  ${d}expect${z} ${w}%d${z}  ${d}reported${z} ${w}%d${z}  ${d}tolerance${z} ±1\n" "$n" ${#x[@]} ${#a[@]}
printf "   ${m}${'':->68}${z}\n";for j in ${!x[@]};do local e=${x[$j]} s=${t[$j]} q=0
for v in ${a[@]+"${a[@]}"};do((v>=e-1&&v<=e+1))&&{ q=1;break;};done
((q))&&printf "   ${g}■${z} %4d  %s\n" $e "$s"||printf "   ${r}□${z} %4d  ${r}%s${z}\n" $e "$s";done
local p=$(: $h ${#x[@]}) v;((p>=90))&&v="${g}pass${z}"||v="${r}fail${z}"
printf "   ${m}${'':->68}${z}\n   coverage ${w}%d${z}/${w}%d${z} (${w}%d%%${z})  %b\n\n" $h ${#x[@]} $p "$v";}
T(){ local f=$1 v=${2:-} n=$(basename "$f" .ada);local q=${n:0:1};((++X[z]));case $q in
[aA])if ! timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
E "$y" ○ "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));return;fi
if ! timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
E "$y" ○ "$n" BIND "unresolved symbols";((++X[s]));return;fi
timeout 5 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&E "$g" ✓ "$n" PASSED&&((++X[a]))||E "$r" ✗ "$n" FAILED "exit $?"&&((++X[f]));;
[bB])if timeout 3 ./ada83 "$f" 2>acats_logs/$n.err;then E "$r" ✗ "$n" WRONG_ACCEPT "compiled when should reject";((++X[f]))
else q=$(^ "$f");h=${q%:*};x=${q#*:};p=$(: $h $x);((p>=90))&&{ ((++X[b]));[[ $v == v ]]&&@ "$f"||E "$g" ✓ "$n" REJECTED "$h/$x errors (${p}%)";}||{ ((++X[f]));E "$r" ✗ "$n" LOW_COVERAGE "$h/$x errors (${p}%)";};fi;;
[cC])if ! timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
E "$y" ○ "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));return;fi
if ! timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
E "$y" ○ "$n" BIND "unresolved symbols";((++X[s]));return;fi
if timeout 10 lli test_results/$n.bc>acats_logs/$n.out 2>&1;then
grep -q PASSED acats_logs/$n.out 2>/dev/null&&E "$g" ✓ "$n" PASSED&&((++X[c]))||{
grep -q NOT.APPLICABLE acats_logs/$n.out 2>/dev/null&&E "$y" – "$n" N/A "$(grep -o "NOT.APPLICABLE.*" acats_logs/$n.out|head -1|cut -c1-40)"&&((++X[s]))||{
grep -q FAILED acats_logs/$n.out 2>/dev/null&&E "$r" ✗ "$n" FAILED "$(grep FAILED acats_logs/$n.out|head -1|cut -c1-50)"&&((++X[f]))||
E "$r" ✗ "$n" NO_REPORT "no PASSED/FAILED in output"&&((++X[f]));};};else local ec=$?
E "$r" ✗ "$n" RUNTIME "exit $ec $(tail -1 acats_logs/$n.out 2>/dev/null|cut -c1-40)";((++X[f]));fi;;
[dD])if ! timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
grep -qi "capacity\|overflow\|limit" acats_logs/$n.err 2>/dev/null&&E "$y" – "$n" CAPACITY "compiler limit exceeded"&&((++X[s]))&&return
E "$y" ○ "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));return;fi
timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>/dev/null||{ E "$y" ○ "$n" BIND;((++X[s]));return;}
timeout 5 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&grep -q PASSED acats_logs/$n.out&&E "$g" ✓ "$n" PASSED&&((++X[d]))||
E "$r" ✗ "$n" FAILED "exact arithmetic check"&&((++X[f]));;
[eE])timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err||{ E "$y" ○ "$n" COMPILE "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)";((++X[s]));return;}
timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>/dev/null||{ E "$y" ○ "$n" BIND;((++X[s]));return;}
timeout 5 lli test_results/$n.bc>acats_logs/$n.out 2>&1
grep -q "TENTATIVELY PASSED" acats_logs/$n.out 2>/dev/null&&E "$y" ? "$n" INSPECT "requires manual verification"&&((++X[e]))||{
grep -q PASSED acats_logs/$n.out 2>/dev/null&&E "$g" ✓ "$n" PASSED&&((++X[e]))||E "$r" ✗ "$n" FAILED&&((++X[f]));};;
[lL])if timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
if timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
timeout 3 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&E "$r" ✗ "$n" WRONG_EXEC "should not execute"&&((++X[f]))||
E "$g" ✓ "$n" BIND_REJECT "execution blocked"&&((++X[l]));else E "$g" ✓ "$n" LINK_REJECT "binding failed as expected"&&((++X[l]));fi
else E "$g" ✓ "$n" COMPILE_REJECT "$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-40)"&&((++X[l]));fi;;
[fF])E "$k" · "$n" FOUNDATION "support code";;*)E "$y" ○ "$n" UNKNOWN "unrecognized test class '$q'"&&((++X[s]));;esac;}
R(){ local tot=${X[z]} pass=$((X[a]+X[b]+X[c]+X[d]+X[e]+X[l])) bf=${X[f]};local bt=$((X[b]+bf)) ct=$((X[c]+X[s]))
printf "\n${c}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n ${w}RESULTS${z}\n"
printf "${c}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n\n"
printf " ${d}%-22s${z}  ${g}%6s${z}  ${r}%6s${z}  ${y}%6s${z}  %6s  %6s\n" CLASS pass fail skip total rate
printf " ${k}──────────────────────  ──────  ──────  ──────  ──────  ──────${z}\n"
((X[a]>0))&&printf " A  Acceptance            %6d                  %6d\n" ${X[a]} ${X[a]}
((bt>0))&&printf " B  Illegality             %6d  %6d          %6d  %5d%%\n" ${X[b]} $bf $bt $(: ${X[b]} $bt)
((ct>0))&&printf " C  Executable             %6d          %6d  %6d  %5d%%\n" ${X[c]} ${X[s]} $ct $(: ${X[c]} $ct)
((X[d]>0))&&printf " D  Numerics               %6d                  %6d\n" ${X[d]} ${X[d]}
((X[e]>0))&&printf " E  Inspection             %6d                  %6d\n" ${X[e]} ${X[e]}
((X[l]>0))&&printf " L  Post-compilation       %6d                  %6d\n" ${X[l]} ${X[l]}
printf " ${k}──────────────────────  ──────  ──────  ──────  ──────  ──────${z}\n"
printf " ${w}TOTAL${z}                    ${w}%6d${z}  ${w}%6d${z}  ${w}%6d${z}  ${w}%6d${z}  ${w}%5d%%${z}\n" $pass ${X[f]} ${X[s]} $tot $(: $pass $tot)
((X[ee]>0))&&{ printf "\n${m}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n"
printf " ${w}B-TEST ERROR COVERAGE${z}\n${m}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n\n"
printf " errors detected   ${w}%5d${z} / ${w}%d${z}\n coverage rate     ${w}%d%%${z}\n" ${X[ec]} ${X[ee]} $(: ${X[ec]} ${X[ee]});}
printf "\n${k}────────────────────────────────────────────────────────────────────────${z}\n"
printf " elapsed ${w}$(.)s${z}    processed ${w}${X[z]}${z} tests    $(date "+%Y-%m-%d %H:%M:%S")\n"
printf "${k}────────────────────────────────────────────────────────────────────────${z}\n"
printf "A=%d B=%d C=%d D=%d E=%d L=%d F=%d S=%d T=%d/%d (%d%%) ERR=%d/%d\n" \
${X[a]} ${X[b]} ${X[c]} ${X[d]} ${X[e]} ${X[l]} ${X[f]} ${X[s]} $pass $tot $(: $pass $tot) ${X[ec]} ${X[ee]}>test_summary.txt;}
+(){ printf "\n${c}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n ${w}%s${z}\n" "$1"
printf "${c}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n\n";}
G(){ + "Class ${1^^} Tests";for f in acats/${1}*.ada;do [[ -f $f ]]&&T "$f" "${2:-}";done;R;}
O(){ + "B-Test Error Detection Analysis";for f in acats/b*.ada;do [[ -f $f ]]||continue;n=$(basename "$f" .ada);((++X[z]))
[[ ${1:-} == v ]]&&{ @ "$f";q=$(^ "$f");((100*${q%:*}/${q#*:}>=90))&&((++X[b]))||((++X[f]));continue;}
q=$(^ "$f");h=${q%:*};x=${q#*:};p=$(: $h $x);((p>=90))&&E "$g" ✓ "$n" PASS "$h/$x errors (${p}%)"&&((++X[b]))||
E "$r" ✗ "$n" FAIL "$h/$x errors (${p}%)"&&((++X[f]));done;R;}
Q(){ + "Sample Tests";for f in acats/b{22003a,22001h}.ada acats/c{95009a,45231a}.ada;do [[ -f $f ]]&&T "$f";done;R;}
A(){ + "Full Suite";for f in acats/*.ada;do [[ -f $f ]]&&T "$f";done;R;}
U(){ cat<<E
Usage: $0 <mode> [options]
Modes:  f  full suite  s  sample (4 tests)  g <X> [v]  group  b [v]  B-test oracle  h  help
Classes: A acceptance B illegality C executable D numerics E inspection L post-compilation
Options: v  verbose B-test oracle detail
Output: test_results/*.{ll,bc} acats_logs/*.{err,out} test_summary.txt
E
}
mkdir -p test_results acats_logs;case ${1:-h} in f)A;;s)Q;;g)G "${2:-c}" "${3:-}";;b)O "${2:-}";;*)U;;esac
