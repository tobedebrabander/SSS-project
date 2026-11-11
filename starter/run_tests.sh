#!/usr/bin/env bash
set -u
B=build
HIJACK=42
pass=0; fail=0
ok(){ echo "PASS $1"; pass=$((pass+1)); }
no(){ echo "FAIL $1 : $2"; fail=$((fail+1)); }

# --- benign ---
for t in callbacks api_demo fork_icall; do
  echo "-- Running test $t --"
  if out=$("$B/$t" 2>/dev/null); then ok "$t (benign)"
  else no "$t (benign)" "exited $?"; fi
done

# --- attacks ---
#for t in fptr_overwrite ret_overwrite; do
for t in fptr_overwrite fptr_midfunc; do
  echo "-- Running test $t --"
  #"$B/$t.plain" >/dev/null 2>&1; p=$?
  #"$B/$t.guard" >/dev/null 2>&1; g=$?
  "$B/$t.plain"; p=$?
  "$B/$t.guard"; g=$?
  if [ "$p" -ne "$HIJACK" ]; then no "$t" "plain did not hijack (exit $p); implement nothing yet? expected $HIJACK"
  elif [ "$g" -eq "$HIJACK" ] || [ "$g" -eq 0 ]; then no "$t" "guarded was NOT blocked (exit $g)"
  else ok "$t (plain hijacks, guarded blocks)"; fi
done

echo "-----"
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
