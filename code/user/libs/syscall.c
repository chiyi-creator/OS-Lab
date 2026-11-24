#include <defs.h>
#include <unistd.h>
#include <syscall.h>

// 2413669(Lab5): 修正用户态系统调用参数传递与返回值约束。
static inline int
syscall(int64_t num, uint64_t arg0, uint64_t arg1, uint64_t arg2,
        uint64_t arg3, uint64_t arg4)
{
    register uint64_t a0 asm("a0") = (uint64_t)num;
    register uint64_t a1 asm("a1") = arg0;
    register uint64_t a2 asm("a2") = arg1;
    register uint64_t a3 asm("a3") = arg2;
    register uint64_t a4 asm("a4") = arg3;
    register uint64_t a5 asm("a5") = arg4;

    asm volatile("ecall"
                 : "+r"(a0)
                 : "r"(a1), "r"(a2), "r"(a3), "r"(a4), "r"(a5)
                 : "memory");
    return (int)(int64_t)a0;
}

int
sys_exit(int64_t error_code) {
    return syscall(SYS_exit, (uint64_t)error_code, 0, 0, 0, 0);
}

int
sys_fork(void) {
    return syscall(SYS_fork, 0, 0, 0, 0, 0);
}

int
sys_wait(int64_t pid, int *store) {
    return syscall(SYS_wait, (uint64_t)pid, (uint64_t)store,
                   0, 0, 0);
}

int
sys_yield(void) {
    return syscall(SYS_yield, 0, 0, 0, 0, 0);
}

int
sys_kill(int64_t pid) {
    return syscall(SYS_kill, (uint64_t)pid, 0, 0, 0, 0);
}

int
sys_getpid(void) {
    return syscall(SYS_getpid, 0, 0, 0, 0, 0);
}

int
sys_putc(int64_t c) {
    return syscall(SYS_putc, (uint64_t)c, 0, 0, 0, 0);
}

int
sys_pgdir(void) {
    return syscall(SYS_pgdir, 0, 0, 0, 0, 0);
}

