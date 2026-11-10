/*
 * guard.c - Guard runtime library.
 *
 * Every function below is a stub. Implement them in Part 1.
 */
#include "guard.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

#include <sys/mman.h>

__attribute__((constructor)) static void guard_init(void)
{
    // You can use this function to initialize the libguard library before any other code runs.
}

void guard_register(void *fn)
{
    // TODO: set the bit for fn in the bitmap.
    (void) fn;
}

void guard_seal(void)
{
    // TODO: mprotect the bitmap read-only.
}

int guard_is_valid(const void *addr)
{
    // TODO: O(1) bitmap lookup.
    (void) addr;
    return 1;
}

_Noreturn void guard_violation(const char *what, const void *addr,
                               const char *file, int line)
{
    fprintf(stderr, "guard: %s violation at %s:%d (target %p)\n", what, file, line, addr);
    abort();
}

void guard_ss_push(void *ret)
{
    // TODO: push onto an mmap'ed shadow stack which is surrounded by PROT_NONE guard pages.
    (void)ret;
}

void guard_ss_check_pop(void *ret, const char *file, int line)
{
    // TODO: compare with the top, then either pop or report a CFI violation.
    (void) ret;
    (void) file;
    (void) line;
}
