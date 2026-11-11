/*
 * guard.h - Guard: source-level control-flow integrity for C.
 *
 * This header is included by every instrumented source file.
 * The macros below are pass-through placeholders: replace them with
 * real implementations.
 *
 * Building with -DGUARD_DISABLE turns every macro into a no-op, so the
 * same instrumented source also builds without libguard (the "plain"
 * build used to show that attacks succeed without protection).
 */
#ifndef GUARD_H
#define GUARD_H

/* ------------------------------------------------------------------ */
/* Valid call target table                                            */
/* ------------------------------------------------------------------ */

/* Add fn to the table of valid indirect call targets.
 * Calling this after guard_seal() terminates the process. */
void guard_register(void *fn);

/* Make the table immutable (read-only). */
void guard_seal(void);

/* Return nonzero if addr is the start of a registered function. O(1). */
int guard_is_valid(const void *addr);

/* ------------------------------------------------------------------ */
/* Violations                                                         */
/* ------------------------------------------------------------------ */

/* Report a CFI violation on stderr and abort(). Never returns. */
_Noreturn void guard_violation(const char *what, const void *addr,
                               const char *file, int line);

/* Report a shadow stack error (overflow, underflow, return address
 * mismatch) on stderr and abort(). Never returns. */
_Noreturn void guard_ss_violation(const char *what, const void *addr,
                                  const char *file, int line);

/* ------------------------------------------------------------------ */
/* Shadow stack                                                       */
/* ------------------------------------------------------------------ */

/* Push a return address onto the shadow stack.
 * Overflowing the shadow stack is a violation. */
void guard_ss_push(void *ret, const char *file, int line);

/* Compare ret with the top of the shadow stack and pop it.
 * A mismatch is a violation. */
void guard_ss_check_pop(void *ret, const char *file, int line);

/* ------------------------------------------------------------------ */
/* Instrumentation macros                                             */
/* ------------------------------------------------------------------ */

#ifdef GUARD_DISABLE

#define GUARD_ICALL(fp)       (fp)
#define GUARD_ENTER()         ((void)0)
#define GUARD_RETURN(expr)    return (expr)
#define GUARD_RETURN_VOID()   return

#else

/* Check fp against the valid target table, evaluating it exactly once,
 * and yield the checked value. Usage: fp(x)  ->  GUARD_ICALL(fp)(x)   */
#define GUARD_ICALL(fp) ({                                          \
    __typeof__(fp) guard_fp_ = (fp);                                \
    if (!guard_is_valid((const void *)guard_fp_))                   \
        guard_violation("icall", (const void *)guard_fp_,           \
                        __FILE__, __LINE__);                        \
    guard_fp_;                                                      \
})

/* First statement of every function: push this function's return
 * address onto the shadow stack. This must be a macro and not a
 * function, so __builtin_return_address(0) is evaluated in the frame
 * of the instrumented function itself.                                */
#define GUARD_ENTER()                                               \
    guard_ss_push(__builtin_return_address(0), __FILE__, __LINE__)

/* Evaluate expr exactly once, then check the return address against
 * the shadow stack, then return the saved value.
 * Usage: return x;  ->  GUARD_RETURN(x);
 * __auto_type (instead of __typeof__) also works when expr is a
 * function name, since it decays to a function pointer.
 * do { } while (0) makes the macro a single statement, so it is safe
 * in unbraced if/else branches.                                       */
#define GUARD_RETURN(expr) do {                                     \
    __auto_type guard_ret_ = (expr);                                \
    guard_ss_check_pop(__builtin_return_address(0),                 \
                       __FILE__, __LINE__);                         \
    return guard_ret_;                                              \
} while (0)

/* Same for void functions. Usage: return;  ->  GUARD_RETURN_VOID();   */
#define GUARD_RETURN_VOID() do {                                    \
    guard_ss_check_pop(__builtin_return_address(0),                 \
                       __FILE__, __LINE__);                         \
    return;                                                         \
} while (0)

#endif /* GUARD_DISABLE */

#endif /* GUARD_H */
