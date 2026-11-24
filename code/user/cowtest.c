#include <stdio.h>
#include <ulib.h>

// 2413669(Lab5): global writable data used to exercise COW.
volatile int cow_global = 100;

int
main(void)
{
    // 2413669(Lab5): stack-local writable page used to exercise COW.
    volatile int cow_stack = 200;
    int pid;

    cprintf("COW test: before fork, global=%d, stack=%d\n",
            cow_global, cow_stack);

    pid = fork();
    if (pid < 0)
    {
        panic("cowtest: fork failed: %d\n", pid);
    }

    if (pid == 0)
    {
        assert(cow_global == 100);
        assert(cow_stack == 200);

        cow_global = 300;
        cow_stack = 400;

        assert(cow_global == 300);
        assert(cow_stack == 400);
        cprintf("Child: global=%d, stack=%d\n", cow_global, cow_stack);
        exit(0);
    }

    assert(pid > 0);
    assert(wait() == 0);
    assert(cow_global == 100);
    assert(cow_stack == 200);

    cprintf("Parent: global=%d, stack=%d\n", cow_global, cow_stack);
    cprintf("cowtest pass!\n");
    return 0;
}
