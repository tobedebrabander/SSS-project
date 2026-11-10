/*
 * fptr_overwrite.c - forward-edge hijack.
 */
#include "../libguard/guard.h"
#include "attack.h"
#include <string.h>

static void greet(void)
{
    printf("hello\n");
}

// Never address-taken in source: Guard must *NOT* register it.
void win(void)
{
    printf("hijacked: win() ran\n");
    _exit(HIJACKED_STATUS);
}

struct widget {
    char  name[16];
    void (*on_click)(void);
};

int main(void)
{
    GUARD_ENTER();
    guard_register(greet);
    guard_seal();

    struct widget w;
    w.on_click = greet;

    // The "attacker" overflows name into on_click. */
    void *target = attacker_lookup("win");
    memcpy(w.name, &target, sizeof target);           // step 1: fill name
    memcpy(&w.on_click, &target, sizeof target);      // step 2: clobber ptr

    GUARD_ICALL(w.on_click)(); // blocked by Guard

    printf("on_click returned normally\n");
    GUARD_RETURN(0);
}
