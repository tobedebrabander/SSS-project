# SSS Project

Source-level control-flow integrity (CFI) for C, built for the SSS course.

The code lives in [`starter/`](starter/):

- `libguard/`: the Guard runtime (function-pointer checks and a shadow stack)
- `rewriter/`: Python source rewriter that inserts the `GUARD_*` instrumentation
- `testcases/`: benign and attack test programs

## Build and test

```sh
cd starter
make        # build libguard and the test binaries
make test   # run the test suite
```

See [`starter/README.md`](starter/README.md) for details.
# Assignment From Ufora
# Introduction

In this project you will build **Guard**, a source-level control-flow integrity (CFI) system for C. Guard protects:
* **Forward edges** (indirect calls through function pointers) in the style of Microsoft Control Flow Guard (CFG).
* **Backward edges** (function returns) with a shadow stack.

You start by writing a runtime library and instrumenting programs by hand with macros. You then write a tool, `guardcc`, that parses C source with tree-sitter and inserts that instrumentation automatically. Finally, you attack your own system and measure what it costs.

* **Group assignment:** Groups of 4. Please enroll in a group on Ufora.
* **Starting code:** Available on Ufora.

---

# Background

Memory corruption bugs let an attacker overwrite code pointers and redirect execution. Two targets matter here:
* **Forward edges:** A function pointer (in a struct, a global, a callback table) is overwritten, so the next indirect call jumps to attacker-chosen code.
* **Backward edges:** A stack buffer overflow overwrites a saved return address, so the function returns into attacker-chosen code (classic stack smashing, return-oriented programming).

CFI restricts each control transfer to targets the program could legitimately reach.

### Forward Edges (CFG-style)
We take inspiration from Microsoft Control Flow Guard. It is a coarse-grained forward-edge CFI. The compiler records every function whose address is taken. At load time, these become a bitmap of valid call targets. Before every indirect call, the compiler inserts a check that terminates the process if the target is not in the bitmap. Any valid target is allowed at any call site; the target only has to be the start of an address-taken function.

### Backward Edges (Shadow Stack)
A shadow stack keeps a second copy of every return address in a separate memory region (i.e., separate from the normal stack). On return, the actual return address is compared with the shadow copy. Intel CET implements this in hardware, but you will implement it in software.

### Source-Level Approach
The real implementations work at the compiler level. You will work at the source level: Guard is a library plus a source-to-source rewriter, and the output is ordinary C compiled by an unmodified `gcc`.

---

# Getting Started

Download the starting code from Ufora. It contains:
* `libguard/guard.h` and `libguard/guard.c`: An empty runtime library with the API you must implement.
* `testcases/`: Small C programs.
  * Some are benign (e.g., `callbacks`) and must behave identically with and without Guard.
  * Others (e.g., `fptr_overwrite`) contain a deliberate memory corruption bug that hijacks control flow.
* A `Makefile` that builds the library, builds the test cases with and without Guard, and runs them.
* When you add a test case, register its filename in both the `Makefile` (in its `BENIGN` or `ATTACK` list) and `run_tests.sh` (the matching loop) so `make test` builds and runs it.

### Testing Rules
* Every attack test case must first succeed without Guard: show that the hijack actually happens (e.g., a `win()` function that should never run prints a message).
* With Guard, the same input must be detected and the program must terminate before the hijacked transfer happens.
* Some attacks only work with specific flags. The `Makefile` builds attack test cases with `-fno-stack-protector -no-pie -fcf-protection=none` so that stack canaries, ASLR, and hardware CET do not hide the effect of your own protection. Do not use these flags for the library itself.

---

# Part 1: Runtime Library and Manual Instrumentation

In this part you implement `libguard` and instrument the test cases by hand with macros. Part 2 will generate exactly the same instrumentation automatically, so design the macros with that in mind.

## Component 1: Valid Call Target Table (6 points)
Implement the table of valid indirect call targets:
* Implement `guard_register(void *fn)`, which adds a function to the table, and `guard_seal()`, which makes the table immutable. Registering after sealing must terminate the process.
* Store the table as a bitmap over the program's text segment: one bit per 16-byte slot. Find the text segment bounds with the linker symbols `__executable_start` and `_etext`. The code already builds with `-falign-functions=16` so every function starts on a slot boundary.
* Allocate the bitmap with `mmap` and make it read-only with `mprotect` in `guard_seal()`.
* A lookup must be O(1). Addresses outside the text segment, and addresses inside a function but not at its start, are invalid. The latter can be enforced by checking whether the address is divisible by 16 after a hit in the bitmap.
* Write a test case named `api_demo` in the `testcases` directory that showcases the implemented APIs: at least one registration, a seal, and a passing and a failing call to `guard_is_valid`.

## Component 2: Indirect Call Checks (6 points)
Check every indirect call before it happens:
* Implement a macro `GUARD_ICALL(fp)` that checks `fp` against the table and evaluates to `fp`, so that `fp(x)` becomes `GUARD_ICALL(fp)(x)`.
  * The macro must evaluate `fp` exactly once, including when `fp` has side effects (e.g., `table[i++]`).
  * *Tip:* You can use GNU C "macro statement expressions".
* On a violation, call `guard_violation()`: print the call site (`file`, `line`) and the offending address to `stderr` and terminate with `abort()`.
  * *Tip:* Use standard macros to get the current file and line.
* Instrument all indirect calls in the test cases by hand.
* **Test cases:**
  * `fptr_overwrite` (provided): A struct holds a buffer followed by a function pointer, and an overflow of the buffer redirects the pointer to a non-address-taken function `win()`. Verify your implementation blocks the call.
  * `fptr_midfunc`: Redirects a pointer into the middle of a registered function. Verify it is blocked.
  * `fork_icall`: Guard functions, fork the process, and then check the indirect calls still work. This lets you find bugs related to the `mmap` flags.

## Component 3: Shadow Stack (6 points)
Protect return addresses with a software shadow stack:
* Implement `GUARD_ENTER()`, placed as the first statement of a function, which pushes the return address onto the shadow stack.
  * *Tip:* Look for builtin GCC/Clang functions (`__builtin_*`) that obtain the return address.
* Implement `GUARD_RETURN(expr)` and `GUARD_RETURN_VOID()`, which compare `__builtin_return_address(0)` with the top of the shadow stack, pop it, and return.
  * A mismatch calls `guard_violation()`.
  * `expr` must be evaluated before the check and exactly once.
* Allocate the shadow stack with `mmap`, surrounded by `PROT_NONE` guard pages, so that both overflow and underflow crash cleanly instead of corrupting memory.
* Build instrumented code with `-fno-omit-frame-pointer -fno-optimize-sibling-calls`. Explain in `REPORT.md` why tail calls break your shadow stack.
* **Test cases:**
  * `ret_overwrite`: A stack buffer overflow overwrites the saved return address with the address of `win()`. Verify the program terminates before `win()` runs. *(Writing a reliable return-address overflow is harder than it looks, because the compiler is allowed to delete your vulnerability due to undefined behaviour or inlining).*
  * `deep_recursion`: Recurses deep enough to exhaust the shadow stack. Verify it terminates with a clear error rather than undefined behaviour.

---

# Part 2: Automatic Instrumentation with guardcc

Hand instrumentation does not scale and is easy to forget. In this part you write `guardcc`, a source-to-source rewriter built on tree-sitter and its C grammar. Starting code with hooks that act on C grammar is provided. It must be usable as a drop-in compiler:

```bash
./guardcc -o prog a.c b.c
```

`guardcc` rewrites each file into a temporary `.guard.c` file, compiles those with `gcc` using the flags from Part 1, and links them with `libguard`. All your benign test cases must produce identical output with and without `guardcc`, and the rewritten code must compile without warnings.

* You do not need to support code that defines functions or indirect calls inside macros. Document 2 limitations of your approach in `REPORT.md`.
* Refer to the comments and documentation at the top of the provided Python files for implementation guidance.
* `guardcc` instruments plain C, so it must run on uninstrumented source. The provided test cases (`callbacks`, `fptr_overwrite`) and the ones you instrumented by hand in Part 1 (such as `ret_overwrite`) already contain the `GUARD_*` macros; make an uninstrumented copy of each before running `guardcc` on it, otherwise it will instrument the code twice.

## Rewriter 1: Address-Taken Analysis and Registration (4 points)
Find every function whose address escapes and register it automatically:
* A function is address-taken when its name appears anywhere other than as the callee of a direct call: assignments, initializers (including tables of function pointers), arguments, return values, and `&f`.
* For each translation unit, generate a function marked `__attribute__((constructor))` that calls `guard_register` for each address-taken function in that unit.
* Insert `guard_seal()` as the first statement of `main`. Explain in `REPORT.md` why sealing in a constructor would be unreliable with multiple translation units.
* Write a test case named `multi_tu`, split over at least two `.c` files, where a function defined in one file is called through a pointer in the other.

## Rewriter 2: Indirect Call Rewriting (4 points)
Wrap every indirect call in `GUARD_ICALL`:
* A call is indirect when its callee is not a plain identifier (e.g., `s->cb(x)`, `(*fp)(x)`, `table[i](x)`, `get_handler()(x)`).
* A call through a plain identifier is indirect when that identifier is declared as a variable or parameter in the current scope. Track scopes and shadowing. Identifiers not declared in the file (e.g., `printf`) are treated as direct calls.
* The provided `callbacks` test case covers every callee form listed above. Verify with a `grep` in your test script that every indirect call in the rewritten output is wrapped, and no direct call is.

## Rewriter 3: Shadow Stack Instrumentation (6 points)
Insert the shadow stack macros into every function body:
* Insert `GUARD_ENTER()` as the first statement of every function definition.
* Rewrite every `return expr;` into `GUARD_RETURN(expr);` and every `return;` into `GUARD_RETURN_VOID();`, including returns in nested blocks and in unbraced `if`/`else` branches.
* Insert `GUARD_RETURN_VOID();` at the end of void functions that can fall off the end.
* Run Rewriter 2 and Rewriter 3 together on the uninstrumented copies of all test cases, and verify the attack test cases from Part 1 are still blocked.

---

# Part 3: Extending the Protections

There is one required extension, and one optional. The optional extension counts for bonus points as compensation if mistakes were made elsewhere.

## Required: Real-World Robustness (6 points)
Real programs do things that break naive CFI. Support them without disabling protection:
* **Library functions as targets:** Code like `fp = puts;` or a pointer obtained with `dlsym` points outside the main executable's text segment. Extend the table to cover loaded shared objects by walking their dynamic symbol tables with `dl_iterate_phdr`.
* **Non-local jumps:** `longjmp` skips the returns of the frames it unwinds. Make the shadow stack survive `setjmp`/`longjmp`, e.g. by recording the shadow stack depth in a wrapper around `setjmp`.
* **Threads:** Give each thread its own shadow stack, allocated when the thread starts and freed when it exits.
* Write a test case named `robust` that exercises all three and verifies no false positives occur.

## Optional: Fine(r)-Grained CFI (3 bonus points)
Coarse-grained CFI accepts any valid target at any call site. Restrict forward-edge CFI such that not only the call target, but also its caller is taken into account. *(You may ignore rewriter compatibility for this part because it requires a sophisticated analysis of call targets).*
* Change the guard registration function such that it takes a caller and callee pointer. Then extend the CFG bitmap such that it represents an adjacency matrix (or list) for a CFG.
* Extend `guardcc` to handle the new registration mechanism.
* Write a test case named `fine_grained` that demonstrates this functionality by calling a function not registered as a possible call target for a caller.

---

# Part 4: Evaluation (8 points)

In a file named `REPORT.md` in the root of your repository:
1. **Security analysis (4 points):** Describe three attacks your implementation does not stop, and write a working test case named `bypass` for one of them. *Hint:* Think about situations of calling a different valid target, data-only attacks, and an attacker with an arbitrary write who targets the shadow stack itself.
2. **Performance (4 points):** Measure the run-time overhead of forward-edge and backward-edge protection separately, on a call-heavy benchmark (e.g. recursive Fibonacci, or `qsort` with a guarded comparator in your own sort). Report the median of at least 10 runs for each configuration.

---

# Submission Guidelines

* **Deadline:** See the Ufora assignment page.
* Submit your entire repository (excluding object files and generated `.guard.c` files) as a zip file to Ufora.
* The zip must include all source files, the `Makefile`, `guardcc`, all test cases, and `REPORT.md`.
* `make test` must build everything and run every test case, reporting pass or fail per test.

---

# Non-Functional Requirements

* `libguard` and `guardcc` (if written in C) must compile without warnings with `-Wall -Wextra`. Code generated by `guardcc` must compile without warnings too.
* The code must contain no memory leaks, buffer overflows or other memory safety bugs, apart from the deliberate bugs in the attack test cases. *(Tip: test with Valgrind and/or ASAN).*
* The code must contain no undefined behaviour, apart from the attack test cases. *(Tip: compile with `-fsanitize=undefined`).*
* Use proper error handling: check the return value of every library call that can realistically fail.
* Benign programs must never be terminated by Guard. A false positive counts as a failed test case.
* Indicate in `REPORT.md` which group member worked on which part.
