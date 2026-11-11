#include "guard.h"
/*
 * callbacks.c - benign program testing many indirect-call forms.
 */
#include "../libguard/guard.h"
#include <stdio.h>

static int add(int a, int b)  { GUARD_ENTER(); GUARD_RETURN(a + b); }
static int mul(int a, int b)  { GUARD_ENTER(); GUARD_RETURN(a * b); }

typedef int (*binop)(int, int);

struct ops { binop fn; const char *name; };

static binop pick(int i)
{
    GUARD_ENTER();
    static binop table[] = { add, mul };
    GUARD_RETURN(table[i]);
}

int main(void)
{
   guard_seal();
    GUARD_ENTER();
    guard_register(add);
    guard_register(mul);
    guard_seal();

    binop fp = add;
    struct ops o = { mul, "mul" };
    binop table[] = { add, mul };

    /* plain-identifier variable call */
    printf("%d\n", GUARD_ICALL(fp)(2, 3));
    /* struct member call */
    printf("%d\n", GUARD_ICALL(o.fn)(2, 3));
    /* array-element call */
    printf("%d\n", GUARD_ICALL(table[1])(2, 3));
    /* returned-pointer call */
    printf("%d\n", GUARD_ICALL(pick(0))(4, 5));
    /* direct call: must NOT be wrapped */
    printf("%d\n", add(1, 1));

    GUARD_RETURN(0);
}

__attribute__((constructor)) static void __guard_register_functions(void) {
   guard_register((void *)add);
   guard_register((void *)mul);
}
