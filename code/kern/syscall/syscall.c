#include <unistd.h>
#include <console.h>
#include <error.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <assert.h>

// 2413669(Lab 5-1): getpid has no user-space side effects.
static int
sys_getpid(uint64_t arg[])
{
    (void)arg;
    return current->pid;
}

// 2413669(Lab 5-1): putc consumes only the first syscall argument.
static int
sys_putc(uint64_t arg[])
{
    cons_putc((int)arg[0]);
    return 0;
}

static int (*const syscalls[SYS_pgdir + 1])(uint64_t arg[]) = {
    [SYS_getpid] = sys_getpid,
    [SYS_putc] = sys_putc,
};

#define NUM_SYSCALLS        ((sizeof(syscalls)) / (sizeof(syscalls[0])))

//lab5 function1 2413669
// 2413669(Lab 5-1): dispatch validated syscall numbers and return through a0.
void
syscall(void)
{
    struct trapframe *tf;
    uint64_t args[5];
    int64_t syscall_num;
    int ret = -E_INVAL;

    if (current == NULL || (tf = current->tf) == NULL)
    {
        return;
    }

    syscall_num = (int64_t)tf->gpr.a0;
    args[0] = tf->gpr.a1;
    args[1] = tf->gpr.a2;
    args[2] = tf->gpr.a3;
    args[3] = tf->gpr.a4;
    args[4] = tf->gpr.a5;

    if (syscall_num >= 0 && (uint64_t)syscall_num < NUM_SYSCALLS)
    {
        int (*handler)(uint64_t arg[]) = syscalls[syscall_num];
        if (handler != NULL)
        {
            ret = handler(args);
        }
    }

    tf->gpr.a0 = (uint64_t)(int64_t)ret;
}
