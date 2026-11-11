/*
 * deep_recursion - exhaust the shadow stack.
 *
 * Each call pushes a return address, so recursing past the shadow
 * stack's capacity must stop the program with a clear "shadow stack
 * overflow" error. MAX_DEPTH is well above that capacity; reaching it
 * means the overflow went undetected and the test fails (exit 1).
 */
#include "../libguard/guard.h"
#include <stdio.h>

#define MAX_DEPTH 1000000L

/* noinline: every level must be a real call with its own frame. */
__attribute__((noinline)) static long recurse(long depth)
{
    GUARD_ENTER();

    if (depth >= MAX_DEPTH)
        GUARD_RETURN(depth);

    /* Not a tail call: the addition happens after the call returns. */
    GUARD_RETURN(recurse(depth + 1) + 1);
}

int main(void)
{
    GUARD_ENTER();

    long result = recurse(0);

    /* Only reached if the overflow was not detected. */
    printf("deep_recursion: reached depth %ld without a shadow stack overflow\n",
           result);
    GUARD_RETURN(1);
}
