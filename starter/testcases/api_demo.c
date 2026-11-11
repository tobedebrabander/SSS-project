/*
 * api_demo.c - benign program showing the valid call target table API:
 * register, seal, and a passing and a failing guard_is_valid().
 */
#include "../libguard/guard.h"
#include <stdio.h>

static void registered(void) { }
static void unregistered(void) { }

int main(void)
{
    guard_register((void *)registered);
    guard_seal();

    int ok_reg   = guard_is_valid((void *)registered);
    int ok_unreg = guard_is_valid((void *)unregistered);

    printf("guard_is_valid(registered)   = %d\n", ok_reg);
    printf("guard_is_valid(unregistered) = %d\n", ok_unreg);

    return (ok_reg && !ok_unreg) ? 0 : 1;
}
