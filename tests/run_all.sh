#!/bin/sh
# Smoke test for cs. Focuses on required commands and edge cases.
set -eu

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
CS="$ROOT/cs"

if [ ! -x "$CS" ]; then
  echo "cs binary not found. Run 'make' at repo root first." >&2
  exit 1
fi

WORKDIR="$(mktemp -d "${TMPDIR:-/tmp}/cs-test-XXXXXX")"
echo "Using workdir: $WORKDIR"
cd "$WORKDIR"

pass() { printf "✅ %s\n" "$1"; }
fail() { printf "❌ %s\n" "$1"; exit 1; }

run_expect_fail() {
  DESC="$1"
  shift
  if "$@"; then
    fail "$DESC (expected failure, but succeeded)"
  else
    pass "$DESC (failed as expected)"
  fi
}

# 1) init
"$CS" init >/dev/null && pass "init created .cs"

# 2) add + commit happy path
echo "hello" > file.txt
"$CS" add file.txt >/dev/null
"$CS" commit -m "first commit" >/dev/null && pass "commit with staged file"

# 3) log
"$CS" log >/dev/null && pass "log shows history"

# 4) trace existing file
"$CS" trace file.txt >/dev/null && pass "trace existing file"

# 5) integrity
"$CS" integrity >/dev/null && pass "integrity ok"

# 6) timewarp (use current timestamp)
TS=$(date +%s)
"$CS" timewarp "$TS" >/dev/null && pass "timewarp to now"

# 7) revert HEAD
"$CS" revert HEAD >/dev/null && pass "revert HEAD"

# Edge cases
run_expect_fail "commit with empty index" "$CS" commit -m "should fail"
run_expect_fail "add missing file" "$CS" add no_such_file.txt
run_expect_fail "trace missing file" "$CS" trace no_such_file.txt
run_expect_fail "revert invalid id" "$CS" revert deadbeef
run_expect_fail "timewarp too early" "$CS" timewarp 0

pass "All smoke tests completed"


