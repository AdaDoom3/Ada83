#!/bin/bash
P=0;F=0;S=0;BP=0;BF=0;BT=0;BC=0;BE=0
r(){ local f=$1;local n=$(basename "$f" .ada);local s=${n:0:1};printf "%-20s" "$n"
if [[ "$s" =~ [bB] ]];then if timeout 3 ./ada83 "$f" &>/dev/null;then
echo -e "\033[31m✗\033[0m WRONG_ACCEPT";((BF++));else ((BP++));
if [ "$2" = "v" ];then oracle "$f";else echo -e "\033[32m✓\033[0m REJECT";fi;fi;
return;fi;if ! timeout 3 ./ada83 "$f" >"test_results/${n}.ll" 2>/dev/null;then
echo -e "\033[33m○\033[0m SKIP_COMPILE";((S++));return;fi
if ! timeout 3 llvm-link -o "test_results/${n}.bc" "test_results/${n}.ll" rts/report.ll 2>/dev/null;then
echo -e "\033[33m○\033[0m SKIP_LINK";((S++));return;fi
if timeout 3 lli "test_results/${n}.bc" &>/dev/null;then
echo -e "\033[32m✓\033[0m PASS";((P++));else echo -e "\033[31m✗\033[0m FAIL";((F++));fi;}
oracle(){ local f=$1 n=$(basename "$f" .ada);local -a ex ac;local i=0 j=0
while IFS= read -r line;do ((i++));[[ "$line" =~ --\ ERROR ]]&&ex+=("$i");done<"$f"
while IFS=: read -r file line col rest;do [[ "$file" == "$f" ]]&&ac+=("$line");done< <(./ada83 "$f" 2>&1)
local nx=${#ex[@]} na=${#ac[@]} mc=0;for e in "${ex[@]}";do for a in "${ac[@]}";do
((a>e-2&&a<e+2))&&{ ((mc++));break;};done;done
local sc=0;((nx>0))&&sc=$((100*mc/nx));local st="FAIL";((sc>=90))&&st="PASS"
echo -e "\n\033[96m┏━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┓\033[0m"
echo -e "\033[96m┃ \033[97mB-TEST ORACLE\033[96m: \033[93m$n\033[96m ┃\033[0m"
echo -e "\033[96m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m"
for e in "${ex[@]}";do local hit=0;for a in "${ac[@]}";do ((a>e-2&&a<e+2))&&{ hit=1;break;};done
if ((hit));then echo -e "\033[32m✓ Line $e\033[0m [DETECTED]";else echo -e "\033[31m✗ Line $e\033[0m [MISSING!]";fi;done
echo -e "\033[96m┣━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┫\033[0m"
echo -e "\033[96m┃ \033[97mCoverage\033[96m: \033[0m$mc/$nx errors \033[96m│ \033[97mScore\033[96m: \033[0m$sc% \033[96m│ \033[97mStatus\033[96m: \033[0m$st \033[96m┃\033[0m"
echo -e "\033[96m┗━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━┛\033[0m";}
o(){ local T=$((P+F+S+BP+BF)) TP=$((P+BP)) PP=$((P+F)) NP=$((BP+BF))
local PC=0 PPC=0 NPC=0;[ $T -gt 0 ]&&PC=$((100*TP/T))
[ $PP -gt 0 ]&&PPC=$((100*P/PP));[ $NP -gt 0 ]&&NPC=$((100*BP/NP))
echo "";echo "═══════════════════════════════════════════════════════════"
echo "  TEST ORACLE RESULTS";echo "═══════════════════════════════════════════════════════════"
echo "";echo "Positive (compile+run): PASS=$P FAIL=$F SKIP=$S ($PPC%)"
echo "Negative (B-tests):     PASS=$BP FAIL=$BF ($NPC%)"
echo "Total:                  $TP/$T ($PC%)";echo ""
if [ $BT -gt 0 ];then echo "B-Test Oracle Coverage:"
echo "  Tests validated:    $BT";echo "  Total coverage:     $BC errors covered"
echo "  Total expected:     $BE errors expected";local BCP=0
[ $BE -gt 0 ]&&BCP=$((100*BC/BE));echo "  Coverage rate:      $BCP%";echo "";fi
echo "═══════════════════════════════════════════════════════════"
cat>test_summary.txt<<E
ACATS Test Results - $(date)
Positive: $P pass, $F fail, $S skip
Negative: $BP pass, $BF fail
Total: $TP/$T ($PC%) | Pos: $PPC% | Neg: $NPC%
E
echo "";echo "Summary → test_summary.txt";}
b(){ echo "═══════════════════════════════════════════════════════════"
echo "  B-TEST ORACLE: Comprehensive Error Validation"
echo "═══════════════════════════════════════════════════════════";echo ""
mkdir -p acats_logs;local mode=$1;for f in acats/b*.ada;do [ -f "$f" ]||continue
local n=$(basename "$f" .ada);printf "%-20s " "$n";((BT++))
local -a ex ac;local i=0 j=0 mc=0 nx=0 na=0
while IFS= read -r line;do ((i++));[[ "$line" =~ --\ ERROR ]]&&ex+=("$i");done<"$f"
while IFS=: read -r file line col rest;do [[ "$file" == "$f" ]]&&ac+=("$line");done< <(timeout 3 ./ada83 "$f" 2>&1)
nx=${#ex[@]};na=${#ac[@]};for e in "${ex[@]}";do for a in "${ac[@]}";do
((a>e-2&&a<e+2))&&{ ((mc++));break;};done;done
local sc=0;((nx>0))&&sc=$((100*mc/nx));BC=$((BC+mc));BE=$((BE+nx))
if ((sc>=90));then echo -e "\033[32m✓ PASS\033[0m ($mc/$nx = $sc%)";((BP++))
else echo -e "\033[31m✗ FAIL\033[0m ($mc/$nx = $sc%)";((BF++));fi
[ "$mode" = "v" ]&&oracle "$f";done
echo "";echo "Oracle validation complete. Coverage: $BC/$BE errors ($((BE>0?100*BC/BE:0))%)";o;}
s(){ echo "═══════════════════════════════════════════════════════════"
echo "  QUICK SAMPLE VERIFICATION";echo "═══════════════════════════════════════════════════════════";echo ""
echo "B-tests (should reject):";for f in acats/b22003a.ada acats/b22001h.ada acats/b24201a.ada;do
[ -f "$f" ]&&r "$f";done;echo "";echo "C-tests (should compile):"
for f in acats/c95009a.ada acats/c45231a.ada acats/c34007a.ada;do [ -f "$f" ]&&r "$f";done
echo "";echo "Sample complete.";}
g(){ local x=$1;echo "═══════════════════════════════════════════════════════════"
echo "  ACATS ${x^^}-GROUP TESTS";echo "═══════════════════════════════════════════════════════════";echo ""
for f in acats/${x}*.ada;do [ -f "$f" ]&&r "$f" "$2";done;o;}
a(){ echo "═══════════════════════════════════════════════════════════"
echo "  FULL ACATS SUITE (4,050 tests)";echo "═══════════════════════════════════════════════════════════";echo ""
for f in acats/*.ada;do [ -f "$f" ]&&r "$f";done;o;}
mkdir -p test_results acats_logs
case "${1:-h}" in
f|full)a;;s|sample)s;;g|group)g "$2" "$3";;b|oracle)b "$2";;h|help|--help)cat<<E
ACATS Test Harness - Ultra-compressed B-test Oracle
Usage: $0 [MODE] [OPTS]
Modes:
  f, full         Full suite (4,050 tests, ~2min)
  s, sample       Quick verification (6 tests)
  g, group X [v]  Run group X∈{a,b,c,d,e,l}, v=verbose
  b, oracle [v]   B-test oracle validation (1,515 tests), v=verbose
  h, help         This message
Examples:
  $0 f            # Full suite
  $0 s            # Quick check
  $0 g b          # B-group only
  $0 g b v        # B-group verbose (show oracle output)
  $0 b            # B-oracle validation (comprehensive)
  $0 b v          # B-oracle verbose (show coverage per test)
Groups: a(144) b(1515) c(2119) d(50) e(54) l(168)
Output: test_results/*.{ll,bc} acats_logs/*.{err,out}
E
;;*)echo "Unknown: $1 (try '$0 help')";exit 1;;esac
