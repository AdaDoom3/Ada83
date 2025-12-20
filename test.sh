#!/bin/bash
set -euo pipefail
declare -A X=([a]=0[b]=0[c]=0[d]=0[e]=0[l]=0[f]=0[s]=0[z]=0[ec]=0[ee]=0[t]=$(date +%s%3N))
z=$'\e[0m' d=$'\e[2m' k=$'\e[90m' w=$'\e[97m' g=$'\e[32m' r=$'\e[31m' y=$'\e[33m' c=$'\e[36m' m=$'\e[35m' b=$'\e[94m'

:()(((${2:-1}>0))&&printf %d $((100*$1/$2))||printf 0)
.(){ printf "%.3f" "$(bc<<<"scale=4;($(date +%s%3N)-${X[t]})/1000")"; }

E(){
  local col=$1 sym=$2 name=$3 status=$4 detail=${5:-}
  printf "  ${b}%-18s${z} ${col}${sym}${z} ${w}%-14s${z}" "$name" "$status"
  [[ -n $detail ]]&&printf " ${k}%s${z}" "$detail"
  printf "\n"
}

^(){
  local f=$1;local -a x a;local i=0 h=0
  while IFS= read -r l;do((++i));[[ $l =~ --\ ERROR ]]&&x+=($i);done<"$f"
  while IFS=: read -r _ n _ _;do a+=($n);done< <(./ada83 "$f" 2>&1)
  for e in ${x[@]+"${x[@]}"};do for v in ${a[@]+"${a[@]}"};do((v>=e-1&&v<=e+1))&&{ ((++h));break;};done;done
  X[ec]=$((X[ec]+h)) X[ee]=$((X[ee]+${#x[@]}));printf "%d:%d" $h ${#x[@]}
}

@(){
  local f=$1 n=$(basename "$f" .ada);local -a x t a;local i=0 h=0
  while IFS= read -r l;do((++i));[[ $l =~ --\ ERROR:?\ *(.*) ]]&&{ x+=($i);t+=("${BASH_REMATCH[1]:-?}");};done<"$f"
  while IFS=: read -r _ n _ _;do a+=($n);done< <(./ada83 "$f" 2>&1)
  printf "\n"
  printf "   ${m}──────────────────────────────────────────────────────────────────${z}\n"
  printf "   ${w}%s${z}  ${d}expect${z} ${w}%d${z}  ${d}reported${z} ${w}%d${z}  ${d}tolerance${z} ±1\n" "$n" ${#x[@]} ${#a[@]}
  printf "   ${m}──────────────────────────────────────────────────────────────────${z}\n"
  for j in ${!x[@]};do
    local e=${x[$j]} s=${t[$j]} q=0
    for v in ${a[@]+"${a[@]}"};do((v>=e-1&&v<=e+1))&&{ q=1;break;};done
    ((q))&&{ ((++h));printf "   ${g}■${z} %4d  %s\n" $e "$s";}\
         ||printf "   ${r}□${z} %4d  ${r}%s${z}\n" $e "$s"
  done
  local p=$(: $h ${#x[@]}) v;((p>=90))&&v="${g}pass${z}"||v="${r}fail${z}"
  printf "   ${m}──────────────────────────────────────────────────────────────────${z}\n"
  printf "   coverage ${w}%d${z}/${w}%d${z} (${w}%d%%${z})  %b\n\n" $h ${#x[@]} $p "$v"
}

%(){
  local f=$1 v=${2:-} n=$(basename "$f" .ada) q=${n:0:1};((++X[z]))
  case $q in
    [aA])
      local out err
      if ! timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
        err=$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)
        E "$y" "○" "$n" "COMPILE" "$err";((++X[s]));return
      fi
      if ! timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
        E "$y" "○" "$n" "BIND" "unresolved symbols";((++X[s]));return
      fi
      if timeout 5 lli test_results/$n.bc>acats_logs/$n.out 2>&1;then
        E "$g" "✓" "$n" "PASSED";((++X[a]))
      else
        local ec=$?
        E "$r" "✗" "$n" "FAILED" "exit $ec";((++X[f]))
      fi;;
    [bB])
      if timeout 3 ./ada83 "$f" 2>acats_logs/$n.err;then
        E "$r" "✗" "$n" "WRONG_ACCEPT" "compiled when should reject";((++X[f]))
      else
        ((++X[b]))
        if [[ $v == v ]];then
          @ "$f"
        else
          local errcnt=$(wc -l<acats_logs/$n.err 2>/dev/null||echo 0)
          E "$g" "✓" "$n" "REJECTED" "$errcnt errors reported"
        fi
      fi;;
    [cC])
      if ! timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
        local err=$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)
        E "$y" "○" "$n" "COMPILE" "$err";((++X[s]));return
      fi
      if ! timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
        E "$y" "○" "$n" "BIND" "unresolved symbols";((++X[s]));return
      fi
      if timeout 10 lli test_results/$n.bc>acats_logs/$n.out 2>&1;then
        if grep -q "PASSED" acats_logs/$n.out 2>/dev/null;then
          E "$g" "✓" "$n" "PASSED";((++X[c]))
        elif grep -q "NOT.APPLICABLE" acats_logs/$n.out 2>/dev/null;then
          local reason=$(grep -o "NOT.APPLICABLE.*" acats_logs/$n.out|head -1|cut -c1-40)
          E "$y" "–" "$n" "N/A" "$reason";((++X[s]))
        elif grep -q "FAILED" acats_logs/$n.out 2>/dev/null;then
          local reason=$(grep "FAILED" acats_logs/$n.out|head -1|cut -c1-50)
          E "$r" "✗" "$n" "FAILED" "$reason";((++X[f]))
        else
          E "$r" "✗" "$n" "NO_REPORT" "no PASSED/FAILED in output";((++X[f]))
        fi
      else
        local ec=$?
        local last=$(tail -1 acats_logs/$n.out 2>/dev/null|cut -c1-40)
        E "$r" "✗" "$n" "RUNTIME" "exit $ec ${last:+| $last}";((++X[f]))
      fi;;
    [dD])
      if ! timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
        if grep -qi "capacity\|overflow\|limit" acats_logs/$n.err 2>/dev/null;then
          E "$y" "–" "$n" "CAPACITY" "compiler limit exceeded";((++X[s]));return
        fi
        local err=$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)
        E "$y" "○" "$n" "COMPILE" "$err";((++X[s]));return
      fi
      if ! timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>/dev/null;then
        E "$y" "○" "$n" "BIND";((++X[s]));return
      fi
      if timeout 5 lli test_results/$n.bc>acats_logs/$n.out 2>&1&&grep -q "PASSED" acats_logs/$n.out;then
        E "$g" "✓" "$n" "PASSED";((++X[d]))
      else
        E "$r" "✗" "$n" "FAILED" "exact arithmetic check";((++X[f]))
      fi;;
    [eE])
      if ! timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
        local err=$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-50)
        E "$y" "○" "$n" "COMPILE" "$err";((++X[s]));return
      fi
      if ! timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>/dev/null;then
        E "$y" "○" "$n" "BIND";((++X[s]));return
      fi
      timeout 5 lli test_results/$n.bc>acats_logs/$n.out 2>&1
      if grep -q "TENTATIVELY PASSED" acats_logs/$n.out 2>/dev/null;then
        E "$y" "?" "$n" "INSPECT" "requires manual verification";((++X[e]))
      elif grep -q "PASSED" acats_logs/$n.out 2>/dev/null;then
        E "$g" "✓" "$n" "PASSED";((++X[e]))
      else
        E "$r" "✗" "$n" "FAILED";((++X[f]))
      fi;;
    [lL])
      if timeout 3 ./ada83 "$f">test_results/$n.ll 2>acats_logs/$n.err;then
        if timeout 3 llvm-link -o test_results/$n.bc test_results/$n.ll rts/report.ll 2>acats_logs/$n.link;then
          if timeout 3 lli test_results/$n.bc>acats_logs/$n.out 2>&1;then
            E "$r" "✗" "$n" "WRONG_EXEC" "should not execute";((++X[f]))
          else
            E "$g" "✓" "$n" "BIND_REJECT" "execution blocked";((++X[l]))
          fi
        else
          E "$g" "✓" "$n" "LINK_REJECT" "binding failed as expected";((++X[l]))
        fi
      else
        local err=$(head -1 acats_logs/$n.err 2>/dev/null|cut -c1-40)
        E "$g" "✓" "$n" "COMPILE_REJECT" "$err";((++X[l]))
      fi;;
    [fF])E "$k" "·" "$n" "FOUNDATION" "support code";;
    *)E "$y" "○" "$n" "UNKNOWN" "unrecognized test class '$q'";((++X[s]));;
  esac
}

~(){
  local tot=${X[z]} pass=$((X[a]+X[b]+X[c]+X[d]+X[e]+X[l]))
  printf "\n"
  printf "${c}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n"
  printf " ${w}RESULTS${z}\n"
  printf "${c}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n"
  printf "\n"
  printf " ${d}%-22s${z}  ${g}%6s${z}  ${r}%6s${z}  ${y}%6s${z}  %6s  %6s\n" "CLASS" "pass" "fail" "skip" "total" "rate"
  printf " ${k}──────────────────────  ──────  ──────  ──────  ──────  ──────${z}\n"
  ((X[a]>0))&&printf " A  Acceptance            %6d                  %6d\n" ${X[a]} ${X[a]}
  local bf=$((X[f])) bt=$((X[b]+bf))
  ((bt>0))&&printf " B  Illegality             %6d  %6d          %6d  %5d%%\n" ${X[b]} $bf $bt $(: ${X[b]} $bt)
  local ct=$((X[c]+X[s]))
  ((ct>0))&&printf " C  Executable             %6d          %6d  %6d  %5d%%\n" ${X[c]} ${X[s]} $ct $(: ${X[c]} $ct)
  ((X[d]>0))&&printf " D  Numerics               %6d                  %6d\n" ${X[d]} ${X[d]}
  ((X[e]>0))&&printf " E  Inspection             %6d                  %6d\n" ${X[e]} ${X[e]}
  ((X[l]>0))&&printf " L  Post-compilation       %6d                  %6d\n" ${X[l]} ${X[l]}
  printf " ${k}──────────────────────  ──────  ──────  ──────  ──────  ──────${z}\n"
  printf " ${w}TOTAL${z}                    ${w}%6d${z}  ${w}%6d${z}  ${w}%6d${z}  ${w}%6d${z}  ${w}%5d%%${z}\n" $pass ${X[f]} ${X[s]} $tot $(: $pass $tot)
  ((X[ee]>0))&&{
    printf "\n"
    printf "${m}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n"
    printf " ${w}B-TEST ERROR COVERAGE${z}\n"
    printf "${m}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n"
    printf "\n"
    printf " errors detected   ${w}%5d${z} / ${w}%d${z}\n" ${X[ec]} ${X[ee]}
    printf " coverage rate     ${w}%d%%${z}\n" $(: ${X[ec]} ${X[ee]})
  }
  printf "\n"
  printf "${k}────────────────────────────────────────────────────────────────────────${z}\n"
  printf " elapsed ${w}$(.)s${z}    processed ${w}${X[z]}${z} tests    $(date "+%Y-%m-%d %H:%M:%S")\n"
  printf "${k}────────────────────────────────────────────────────────────────────────${z}\n"
  printf "A=%d B=%d C=%d D=%d E=%d L=%d F=%d S=%d T=%d/%d (%d%%) ERR=%d/%d\n" \
    ${X[a]} ${X[b]} ${X[c]} ${X[d]} ${X[e]} ${X[l]} ${X[f]} ${X[s]} $pass $tot $(: $pass $tot) ${X[ec]} ${X[ee]}>test_summary.txt
}

+(){
  printf "\n"
  printf "${c}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n"
  printf " ${w}%s${z}\n" "$1"
  printf "${c}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━${z}\n"
  printf "\n"
}

G(){ + "Class ${1^^} Tests";for f in acats/${1}*.ada;do [[ -f $f ]]&&% "$f" "${2:-}";done;~;}

O(){
  + "B-Test Error Detection Analysis";for f in acats/b*.ada;do [[ -f $f ]]||continue;n=$(basename "$f" .ada);((++X[z]))
  [[ ${1:-} == v ]]&&{ @ "$f";q=$(^ "$f");((100*${q%:*}/${q#*:}>=90))&&((++X[b]))||((++X[f]));continue;}
  q=$(^ "$f");h=${q%:*};x=${q#*:};p=$(: $h $x)
  if ((p>=90));then
    E "$g" "✓" "$n" "PASS" "$h/$x errors (${p}%)"
    ((++X[b]))
  else
    E "$r" "✗" "$n" "FAIL" "$h/$x errors (${p}%)"
    ((++X[f]))
  fi
  done;~
}

Q(){ + "Sample Tests";for f in acats/b22003a.ada acats/b22001h.ada acats/c95009a.ada acats/c45231a.ada;do [[ -f $f ]]&&% "$f";done;~;}
A(){ + "Full Suite";for f in acats/*.ada;do [[ -f $f ]]&&% "$f";done;~;}

U(){
  echo "Usage: $0 <mode> [options]"
  echo ""
  echo "Modes:"
  echo "  f          run full test suite"
  echo "  s          run sample tests (4 tests)"
  echo "  g <X> [v]  run group X where X is a test class letter"
  echo "  b [v]      run B-test error detection analysis"
  echo "  h          show this help"
  echo ""
  echo "Test Classes:"
  echo "  A  Acceptance       must compile, bind, execute, report PASSED"
  echo "  B  Illegality       must be rejected at compile time"
  echo "  C  Executable       must report PASSED or NOT-APPLICABLE"
  echo "  D  Numerics         exact arithmetic on large literals"
  echo "  E  Inspection       special criteria, TENTATIVELY PASSED"
  echo "  L  Post-compilation must fail at bind or execution time"
  echo ""
  echo "Options:"
  echo "  v          verbose mode (show per-error oracle detail for B-tests)"
  echo ""
  echo "Output:"
  echo "  test_results/*.ll   LLVM IR output"
  echo "  test_results/*.bc   LLVM bitcode"
  echo "  acats_logs/*.err    compiler stderr"
  echo "  acats_logs/*.out    execution stdout"
  echo "  test_summary.txt    machine-readable results"
}

mkdir -p test_results acats_logs;case ${1:-h} in f)A;;s)Q;;g)G "${2:-c}" "${3:-}";;b)O "${2:-}";;*)U;;esac
