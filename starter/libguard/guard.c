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

/* Linker symbols: start of the executable and end of its code. */
extern char __executable_start[], _etext[];

static uintptr_t text_lo;    /* text segment: [text_lo, text_hi) */
static uintptr_t text_hi;
static uint8_t  *bitmap;     /* one bit per 16-byte slot */
static size_t    bitmap_len; /* in bytes */
static int       sealed;     /* set by guard_seal() */

__attribute__((constructor)) static void guard_init(void)
{
    text_lo = (uintptr_t)__executable_start;
    text_hi = (uintptr_t)_etext;

    size_t slots = (text_hi - text_lo + 15) / 16; /* round up */
    bitmap_len   = (slots + 7) / 8;              /* round up */

    /* Anonymous memory starts zeroed: no valid targets yet. */
    void *p = mmap(NULL, bitmap_len, PROT_READ | PROT_WRITE,
                   MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (p == MAP_FAILED) {
        perror("guard: mmap");
        abort();
    }
    bitmap = p;
}

void guard_register(void *fn)
{
    uintptr_t addr = (uintptr_t)fn;

    if (sealed) {
        fprintf(stderr, "guard: guard_register(%p) after guard_seal()\n", fn);
        abort();
    }
    /* Outside the text segment the index would fall outside the bitmap. */
    if (addr < text_lo || addr >= text_hi) {
        fprintf(stderr, "guard: guard_register(%p) outside text segment\n", fn);
        abort();
    }
    /* guard_is_valid() rejects unaligned addresses, so registering one
     * would silently cause a false positive later. */
    if (addr % 16 != 0) {
        fprintf(stderr, "guard: guard_register(%p) not 16-byte aligned\n", fn);
        abort();
    }

    size_t slot = (addr - text_lo) / 16;
    bitmap[slot / 8] |= (uint8_t)(1u << (slot % 8));
}

void guard_seal(void)
{
    if (mprotect(bitmap, bitmap_len, PROT_READ) != 0) {
        perror("guard: mprotect");
        abort();
    }
    sealed = 1;
}

int guard_is_valid(const void *addr)
{
    uintptr_t a = (uintptr_t)addr;

    if (a < text_lo || a >= text_hi)
        return 0;

    size_t slot = (a - text_lo) / 16;
    if (!(bitmap[slot / 8] & (1u << (slot % 8))))
        return 0;

    /* Hit: only the start of the slot is a function entry. */
    return a % 16 == 0;
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
