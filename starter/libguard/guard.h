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

/* ------------------------------------------------------------------ */
/* Shadow stack                                                       */
/* ------------------------------------------------------------------ */

/* Push a return address onto the shadow stack. */
void guard_ss_push(void *ret);

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

/* TODO: check fp, evaluate it exactly once, yield fp.
 * Usage: fp(x)  ->  GUARD_ICALL(fp)(x)                                */
#define GUARD_ICALL(fp)       (fp)

/* TODO: first statement of every function.                            */
#define GUARD_ENTER()         ((void)0)

/* TODO: evaluate expr once, check the return address,
 * then return. Usage: return x;  ->  GUARD_RETURN(x);
 * NOTE: Must evaluate the expression before the shadow stack check.   */
#define GUARD_RETURN(expr) return (expr)

/* TODO: same for void functions.                                      */
#define GUARD_RETURN_VOID() return

#endif /* GUARD_DISABLE */

#endif /* GUARD_H */
