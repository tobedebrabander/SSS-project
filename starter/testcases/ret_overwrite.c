/*
 * ret_overwrite.c - backward-edge hijack (stack smashing).
 *
 * A stack buffer overflow in vulnerable() overwrites its saved return
 * address with &win. Plain build: returns into win() -> exit 42.
 * Guard build: GUARD_RETURN_VOID() sees the corrupted return address
 * and aborts first.
 */
#include "../libguard/guard.h"
#include "attack.h"
#include <string.h>

// Never address-taken in source: Guard must *NOT* register it.
void win(void)
{
    printf("hijacked: win() ran\n");
    _exit(HIJACKED_STATUS);
}

__attribute__((noinline)) static void vulnerable(void)
{
    GUARD_ENTER();

    char buf[64];

    void *win_addr = attacker_lookup("win");

    /* x86-64 with a frame pointer: saved return address is at fp + 8. */
    void **saved_ret = (void **)((char *)__builtin_frame_address(0) + 8);

    /* Bytes from buf through the return slot. Computed at run time so the
     * compiler cannot prove the memcpy is out of bounds and delete it. */
    size_t span = (size_t)((char *)(saved_ret + 1) - buf);

    static void *payload[128];
    for (size_t i = 0; i < span / sizeof(void *) + 1; i++)
        payload[i] = win_addr;

    /* The bug: copies `span` bytes into a 64-byte buffer. */
    memcpy(buf, payload, span);

    GUARD_RETURN_VOID();
}

int main(void)
{
    GUARD_ENTER();

    vulnerable();

    printf("vulnerable() returned normally\n");
    GUARD_RETURN(0);
}
