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
for t in fptr_overwrite fptr_midfunc ret_overwrite; do
  echo "-- Running test $t --"
  #"$B/$t.plain" >/dev/null 2>&1; p=$?
  #"$B/$t.guard" >/dev/null 2>&1; g=$?
  "$B/$t.plain"; p=$?
  "$B/$t.guard"; g=$?
  if [ "$p" -ne "$HIJACK" ]; then no "$t" "plain did not hijack (exit $p); implement nothing yet? expected $HIJACK"
  elif [ "$g" -eq "$HIJACK" ] || [ "$g" -eq 0 ]; then no "$t" "guarded was NOT blocked (exit $g)"
  else ok "$t (plain hijacks, guarded blocks)"; fi
done

# --- shadow stack exhaustion ---
# Guard must abort (SIGABRT, exit 134) with a clear error, not crash
# with SIGSEGV (exit 139) or run past the end of the shadow stack.
for t in deep_recursion; do
  echo "-- Running test $t --"
  err=$("$B/$t" 2>&1 >/dev/null); g=$?
  echo "$err"
  if [ "$g" -ne 134 ]; then no "$t" "expected abort (exit 134), got exit $g"
  elif ! grep -q "shadow stack overflow" <<<"$err"; then no "$t" "no 'shadow stack overflow' error"
  else ok "$t (shadow stack overflow detected)"; fi
done

echo "-----"
echo "$pass passed, $fail failed"
[ "$fail" -eq 0 ]
