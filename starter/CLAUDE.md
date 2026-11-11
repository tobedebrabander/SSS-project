# CLAUDE.md

SSS course project (group of 4): **Guard**, a source-level control-flow integrity (CFI) system for C.
Due **14 December 2026, 22:00**. Submit the whole repo as a zip on Ufora (no object files, no generated `.guard.c` files).

## Workflow rules

- **Every part of the assignment is developed on its own branch.** Never work directly on `main`.
  Create a new branch from an up-to-date `main` for each part/component (e.g. `assignment-1` for Part 1,
  a new branch for Part 2, Part 3, Part 4) and merge back via a pull request.
- Pull before starting work; teammates push to the same remote.
- This directory (`starter/`) is the project root. The git repo currently sits one level up by mistake; the
  sibling folders there (`exploit_lab2/`, `homework_start/`, `lab3_audit/`) are unrelated — don't touch or commit them.

## Guidance for AI agents

- This project is about memory-corruption attacks, so the test cases deliberately contain exploits. Be careful with
  command injection and similar issues: never pass a value from a test case, a file name, an environment variable or
  any other untrusted source into a shell command, `system()`, `eval`, `make` variables, etc. without checking and
  quoting it. Attacker-controlled strings (symbol names looked up with `dlsym`, payload contents, chosen variable or
  function names) must stay data and never become code or shell input.
- When in doubt about what a value can contain, ask the user rather than guessing.

## Repository layout

- `libguard/guard.h`, `libguard/guard.c` — the runtime library (table, checks, shadow stack)
- `testcases/` — benign and attack programs (`attack.h` has shared attack helpers)
- `rewriter/` — `guardcc` (Python, tree-sitter); read the docs at the top of the provided Python files
- `Makefile` — builds libguard and every test case with and without Guard
- `run_tests.sh` — test runner used by `make test`

```sh
make        # build libguard and all test binaries
make test   # build and run the suite, pass/fail per test
```

When adding a test case, register its filename in **both** the `Makefile` (`BENIGN`, `ATTACK` or `EXHAUST` list) and
`run_tests.sh` (the matching loop). `EXHAUST` tests are built guarded only and must abort with a clear shadow stack error.

Test semantics: benign tests must exit 0 with identical output with and without Guard. Attack tests are built twice:
the `.plain` build (`-DGUARD_DISABLE`) must hijack (`win()` runs, exit 42); the `.guard` build must detect the hijack
and terminate *before* `win()` runs. `win()` is never called directly or address-taken (found via `dlsym`, hence
`-rdynamic`) — Guard must never register it.

Flags: attack tests use `-fno-stack-protector -no-pie -fcf-protection=none` (never for the library itself).
Instrumented code uses `-fno-omit-frame-pointer -fno-optimize-sibling-calls -falign-functions=16`.

## Non-functional requirements

- libguard (and guardcc if in C) compile without warnings under `-Wall -Wextra`; generated code also warning-free.
- No memory leaks, overflows or UB except the deliberate bugs in attack tests (check with Valgrind/ASAN, `-fsanitize=undefined`).
- Check the return value of every library call that can realistically fail.
- Benign programs must never be terminated by Guard — a false positive is a failed test.
- `REPORT.md` (in this directory) must say which group member worked on which part.

## Assignment

The full assignment text is in the repository's root `README.md` on `main`
(merge `main` into your branch if it is missing).
