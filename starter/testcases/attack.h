/*
 * attack.h - helpers for the attack test cases.
 *
 * The attacker knows the address of win() (the binary is built with
 * -no-pie). We look it up by name with dlsym, so that win() is never
 * address-taken in the source and Guard must not register it.
 */
#ifndef ATTACK_H
#define ATTACK_H

#define _GNU_SOURCE
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Exit status of win(): tells the test script the hijack succeeded. */
#define HIJACKED_STATUS 42

static void *attacker_lookup(const char *symbol)
{
    void *addr = dlsym(RTLD_DEFAULT, symbol);
    if (addr == NULL) {
        fprintf(stderr, "attack: cannot find %s (built with -rdynamic?)\n",
                symbol);
        exit(2);
    }
    return addr;
}

#endif /* ATTACK_H */
