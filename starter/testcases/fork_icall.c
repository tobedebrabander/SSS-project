#include "../libguard/guard.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>

typedef int (*binop)(int, int);

int add(int a, int b)
{
    GUARD_ENTER();
    GUARD_RETURN(a + b);
}

int mul(int a, int b)
{
    GUARD_ENTER();
    GUARD_RETURN(a * b);
}

int main(void)
{
    GUARD_ENTER();

    /* Register valid indirect-call targets. */
    guard_register(add);
    guard_register(mul);

    guard_seal();

    /* Prepare function pointers. */
    binop fp1 = add;
    binop fp2 = mul;

    pid_t pid = fork();

    if (pid < 0) {
        perror("fork");
        GUARD_RETURN(1);
    }

    if (pid == 0) {
        /* Child process. */

        int result1 = GUARD_ICALL(fp1)(10, 5);
        int result2 = GUARD_ICALL(fp2)(10, 5);

        printf("Child: add(10, 5) = %d\n", result1);
        printf("Child: mul(10, 5) = %d\n", result2);

        _exit((result1 == 15 && result2 == 50) ? 0 : 1);
    }

    /* Parent process. */
    int status;

    if (waitpid(pid, &status, 0) < 0) {
        perror("waitpid");
        GUARD_RETURN(1);
    }

    if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
        printf("fork_icall: PASS\n");
        GUARD_RETURN(0);
    }

    printf("fork_icall: FAIL\n");
    GUARD_RETURN(1);
}