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

When adding a test case, register its filename in **both** the `Makefile` (`BENIGN` or `ATTACK` list) and
`run_tests.sh` (the matching loop).

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

---

## Assignment

### Background

- **Forward edges:** an overwritten function pointer (struct, global, callback table) makes the next indirect call jump to attacker code.
- **Backward edges:** a stack overflow overwrites a saved return address (stack smashing, ROP).

Inspired by **Microsoft Control Flow Guard**: coarse-grained forward-edge CFI. Every address-taken function goes into a
bitmap of valid call targets; before every indirect call a check aborts if the target is not in the bitmap. Any valid
target is allowed at any call site. Backward edges use a **software shadow stack** (like Intel CET, but in software).
Everything is source-level: libguard + a source-to-source rewriter; output is plain C compiled by unmodified gcc.

### Part 1: Runtime library and manual instrumentation

Instrument test cases by hand with macros; Part 2 must generate exactly the same instrumentation automatically.

**Component 1 — Valid call target table (6 pt)**
- `guard_register(void *fn)` adds a function; `guard_seal()` makes the table immutable. Registering after sealing terminates the process.
- Bitmap over the text segment, one bit per 16-byte slot; bounds from linker symbols `__executable_start` and `_etext`
  (code builds with `-falign-functions=16`).
- Allocate with `mmap`, make read-only with `mprotect` in `guard_seal()`.
- O(1) lookup. Addresses outside text, or inside a function but not at its start, are invalid (check 16-byte alignment after a bitmap hit).
- Test `api_demo`: at least one registration, a seal, one passing and one failing `guard_is_valid`.

**Component 2 — Indirect call checks (6 pt)**
- `GUARD_ICALL(fp)` checks `fp` and evaluates to `fp`, so `fp(x)` → `GUARD_ICALL(fp)(x)`. Must evaluate `fp` exactly once
  even with side effects (`table[i++]`) — use statement expressions.
- On violation, `guard_violation()` prints call site (`__FILE__`, `__LINE__`) and offending address to stderr, then `abort()`.
- Instrument all indirect calls in the test cases by hand.
- `fptr_overwrite` (provided): buffer overflow in a struct redirects a function pointer to `win()`; must be blocked.
- Test `fptr_midfunc`: pointer into the middle of a registered function; must be blocked.
- Test `fork_icall`: guard functions, `fork`, check indirect calls still work (catches mmap flag bugs).

**Component 3 — Shadow stack (6 pt)**
- `GUARD_ENTER()` as first statement of a function: push the return address (`__builtin_return_address(0)`).
- `GUARD_RETURN(expr)` / `GUARD_RETURN_VOID()`: compare `__builtin_return_address(0)` with the shadow top, pop, return;
  mismatch → `guard_violation()`. `expr` evaluated before the check, exactly once.
- Shadow stack allocated with `mmap`, surrounded by `PROT_NONE` guard pages (overflow and underflow crash cleanly).
- Build instrumented code with `-fno-omit-frame-pointer -fno-optimize-sibling-calls`. Explain in REPORT.md why tail calls break the shadow stack.
- Test `ret_overwrite`: stack overflow overwrites the saved return address with `&win`; must terminate before `win()` runs.
  (Beware the compiler deleting the bug via UB or inlining.)
- Test `deep_recursion`: recurse until the shadow stack is exhausted; must terminate with a clear error, not UB.

### Part 2: Automatic instrumentation with `guardcc`

Source-to-source rewriter on tree-sitter (C grammar); starter code has hooks. Drop-in compiler:

```sh
./guardcc -o prog a.c b.c
```

Rewrites each file to a temporary `.guard.c`, compiles with gcc using the Part 1 flags, links with libguard.
All benign tests must give identical output with and without guardcc; rewritten code must compile without warnings.
No need to support function definitions or indirect calls inside macros. Document 2 limitations in REPORT.md.
guardcc runs on **uninstrumented** source: make uninstrumented copies of test cases that already contain `GUARD_*`
macros (callbacks, fptr_overwrite, ret_overwrite, ...) to avoid double instrumentation.

**Rewriter 1 — Address-taken analysis and registration (4 pt)**
- Address-taken = name appears anywhere other than as callee of a direct call: assignments, initializers (incl. tables
  of function pointers), arguments, return values, `&f`.
- Per translation unit, generate an `__attribute__((constructor))` function calling `guard_register` for each.
- Insert `guard_seal()` as first statement of `main`. Explain in REPORT.md why sealing in a constructor is unreliable with multiple TUs.
- Test `multi_tu`: ≥2 `.c` files, function defined in one is called through a pointer in the other.

**Rewriter 2 — Indirect call rewriting (4 pt)**
- Indirect if callee is not a plain identifier: `s->cb(x)`, `(*fp)(x)`, `table[i](x)`, `get_handler()(x)`.
- A plain identifier callee is indirect if declared as a variable/parameter in the current scope. Track scopes and
  shadowing. Identifiers not declared in the file (e.g. `printf`) are direct.
- `callbacks` covers all forms. Test script must grep the rewritten output: every indirect call wrapped, no direct call wrapped.

**Rewriter 3 — Shadow stack instrumentation (6 pt)**
- `GUARD_ENTER()` as first statement of every function definition.
- `return expr;` → `GUARD_RETURN(expr);`, `return;` → `GUARD_RETURN_VOID();`, including nested blocks and unbraced if/else branches.
- Insert `GUARD_RETURN_VOID();` at the end of void functions that can fall off the end.
- Run Rewriters 2+3 on uninstrumented copies of all tests; Part 1 attacks must still be blocked.

### Part 3: Extending the protections

**Required — Real-world robustness (6 pt)** (support without disabling protection)
- Library functions as targets (`fp = puts;`, `dlsym`): extend the table to loaded shared objects by walking their
  dynamic symbol tables with `dl_iterate_phdr`.
- Non-local jumps: make the shadow stack survive `setjmp`/`longjmp` (e.g. record shadow depth in a `setjmp` wrapper).
- Threads: per-thread shadow stack, allocated at thread start, freed at exit.
- Test `robust`: exercises all three, no false positives.

**Optional — Finer-grained CFI (3 bonus pt)** (rewriter compatibility may be ignored)
- Registration takes a caller and callee pointer; bitmap becomes an adjacency matrix/list for a CFG.
- Extend guardcc for the new registration.
- Test `fine_grained`: calls a function not registered as a target for that caller.

### Part 4: Evaluation (8 pt) — in `REPORT.md` (this directory)

- **Security analysis (4 pt):** three attacks the implementation does not stop, and a working test case `bypass` for
  one (ideas: different valid target, data-only attacks, arbitrary write targeting the shadow stack).
- **Performance (4 pt):** run-time overhead of forward-edge and backward-edge protection *separately* on a call-heavy
  benchmark (recursive Fibonacci, or qsort with a guarded comparator in your own sort). Median of ≥10 runs per configuration.

### Submission

- Zip of the whole repo (excluding object files and generated `.guard.c`) with all sources, Makefile, guardcc, all tests, REPORT.md.
- `make test` must build everything and run every test case, reporting pass/fail per test.
