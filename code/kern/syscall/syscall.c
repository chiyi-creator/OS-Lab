#include <unistd.h>
#include <console.h>
#include <error.h>
#include <kmalloc.h>
#include <proc.h>
#include <syscall.h>
#include <trap.h>
#include <stdio.h>
#include <pmm.h>
#include <vmm.h>
#include <string.h>
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

// 2413669(Lab5): adapt process-management syscalls to proc.c's do_* APIs.
static int
sys_fork(uint64_t arg[])
{
    (void)arg;
    if (current == NULL || current->tf == NULL)
    {
        return -E_INVAL;
    }
    return do_fork(0, 0, current->tf);
}

static int
sys_wait(uint64_t arg[])
{
    int64_t pid64 = (int64_t)arg[0];
    int pid = (int)pid64;
    int *code_store = NULL;

    if ((int64_t)pid != pid64)
    {
        return -E_INVAL;
    }

    if (arg[1] != 0)
    {
        uintptr_t code_store_addr = (uintptr_t)arg[1];
        // 2413669(Lab5): reject misaligned user exit-code storage.
        if (code_store_addr % sizeof(int) != 0 ||
            current == NULL || current->mm == NULL ||
            !user_mem_check(current->mm, code_store_addr, sizeof(int), 1))
        {
            return -E_FAULT;
        }
        code_store = (int *)code_store_addr;
    }

    return do_wait(pid, code_store);
}

static int
sys_exec(uint64_t arg[])
{
    size_t name_len = (size_t)arg[1];
    size_t binary_size = (size_t)arg[3];
    uintptr_t name_addr = (uintptr_t)arg[0];
    uintptr_t binary_addr = (uintptr_t)arg[2];
    size_t name_buf_len;
    char *kernel_name;
    unsigned char *kernel_binary;
    int ret;

    if (current == NULL || current->mm == NULL ||
        name_addr == 0 || name_len == 0 ||
        binary_addr == 0 || binary_size == 0 ||
        (uint64_t)name_len != arg[1] ||
        (uint64_t)binary_size != arg[3])
    {
        return -E_INVAL;
    }

    // 2413669(Lab5): reserve a NUL-safe name buffer for future do_execve.
    if (name_len == (size_t)-1)
    {
        return -E_INVAL;
    }
    name_buf_len = (name_len > PROC_NAME_LEN ? name_len : PROC_NAME_LEN) + 1;

    if (!user_mem_check(current->mm, name_addr, name_len, 0) ||
        !user_mem_check(current->mm, binary_addr, binary_size, 0))
    {
        return -E_FAULT;
    }

    ret = -E_NO_MEM;
    kernel_name = kmalloc(name_buf_len);
    if (kernel_name == NULL)
    {
        return ret;
    }
    memset(kernel_name, 0, name_buf_len);

    kernel_binary = kmalloc(binary_size);
    if (kernel_binary == NULL)
    {
        kfree(kernel_name);
        return ret;
    }

    ret = -E_FAULT;
    if (!copy_from_user(current->mm, kernel_name, (const void *)name_addr,
                        name_len, 0) ||
        !copy_from_user(current->mm, kernel_binary,
                        (const void *)binary_addr, binary_size, 0))
    {
        goto out;
    }
    kernel_name[name_len] = '\0';
    kernel_name[name_buf_len - 1] = '\0';

    ret = do_execve(kernel_name, name_len, kernel_binary, binary_size);

out:
    kfree(kernel_binary);
    kfree(kernel_name);
    return ret;
}

static int
sys_exit(uint64_t arg[])
{
    return do_exit((int)(int64_t)arg[0]);
}

static int
sys_yield(uint64_t arg[])
{
    (void)arg;
    return do_yield();
}

static int
sys_kill(uint64_t arg[])
{
    int64_t pid64 = (int64_t)arg[0];
    int pid = (int)pid64;

    if ((int64_t)pid != pid64)
    {
        return -E_INVAL;
    }
    return do_kill(pid);
}

// 2413669(Lab5): print the page table used by the current process.
static int
sys_pgdir(uint64_t arg[])
{
    (void)arg;
    if (current == NULL)
    {
        return -E_INVAL;
    }
    print_pgdir();
    return 0;
}

static int (*const syscalls[SYS_pgdir + 1])(uint64_t arg[]) = {
    [SYS_exit] = sys_exit,
    [SYS_fork] = sys_fork,
    [SYS_wait] = sys_wait,
    [SYS_exec] = sys_exec,
    [SYS_yield] = sys_yield,
    [SYS_kill] = sys_kill,
    [SYS_getpid] = sys_getpid,
    [SYS_putc] = sys_putc,
    [SYS_pgdir] = sys_pgdir,
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
