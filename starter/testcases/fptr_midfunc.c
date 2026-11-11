#include "../libguard/guard.h"
#include "attack.h"
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef void (*func_ptr)(void);

void win(void)
{
    printf("HIJACK SUCCEEDED: win() was executed!\n");
    exit(HIJACKED_STATUS);
}

/*
 * The assembly deliberately contains a jump to win().
 * The jump instruction starts at offset 0.
 */
__attribute__((naked, noinline))
void target(void)
{
    __asm__ volatile(
        "nop\n\t"             // offset 0
        "jmp win\n\t"         // offset 1
    );
}

int main(void)
{
    GUARD_ENTER();
    (void)attacker_lookup; /* from attack.h, unused here: silence -Wunused-function */
    guard_register(target);
    guard_seal();

    /*
     * Redirect the function pointer one byte into target().
     * This skips the initial NOP and executes the jump to win().
     */
    uintptr_t addr = (uintptr_t)target;
    func_ptr fp = (func_ptr)(addr + 1);

    printf("Calling function pointer at %p\n", (void *)fp);
    fflush(stdout);

    GUARD_ICALL(fp)();

    GUARD_RETURN(0);
}