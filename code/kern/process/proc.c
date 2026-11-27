#include <proc.h>
#include <kmalloc.h>
#include <kmonitor.h>
#include <string.h>
#include <sync.h>
#include <pmm.h>
#include <error.h>
#include <sched.h>
#include <elf.h>
#include <vmm.h>
#include <trap.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>

/* ------------- process/thread mechanism design&implementation -------------
(an simplified Linux process/thread mechanism )
introduction:
  ucore implements a simple process/thread mechanism. process contains the independent memory sapce, at least one threads
for execution, the kernel data(for management), processor state (for context switch), files(in lab6), etc. ucore needs to
manage all these details efficiently. In ucore, a thread is just a special kind of process(share process's memory).
------------------------------
process state       :     meaning               -- reason
    PROC_UNINIT     :   uninitialized           -- alloc_proc
    PROC_SLEEPING   :   sleeping                -- try_free_pages, do_wait, do_sleep
    PROC_RUNNABLE   :   runnable(maybe running) -- proc_init, wakeup_proc,
    PROC_ZOMBIE     :   almost dead             -- do_exit

-----------------------------
process state changing:

  alloc_proc                                 RUNNING
      +                                   +--<----<--+
      +                                   + proc_run +
      V                                   +-->---->--+
PROC_UNINIT -- proc_init/wakeup_proc --> PROC_RUNNABLE -- try_free_pages/do_wait/do_sleep --> PROC_SLEEPING --
                                           A      +                                                           +
                                           |      +--- do_exit --> PROC_ZOMBIE                                +
                                           +                                                                  +
                                           -----------------------wakeup_proc----------------------------------
-----------------------------
process relations
parent:           proc->parent  (proc is children)
children:         proc->cptr    (proc is parent)
older sibling:    proc->optr    (proc is younger sibling)
younger sibling:  proc->yptr    (proc is older sibling)
-----------------------------
related syscall for process:
SYS_exit        : process exit,                           -->do_exit
SYS_fork        : create child process, dup mm            -->do_fork-->wakeup_proc
SYS_wait        : wait process                            -->do_wait
SYS_exec        : after fork, process execute a program   -->load a program and refresh the mm
SYS_clone       : create child thread                     -->do_fork-->wakeup_proc
SYS_yield       : process flag itself need resecheduling, -- proc->need_sched=1, then scheduler will rescheule this process
SYS_sleep       : process sleep                           -->do_sleep
SYS_kill        : kill process                            -->do_kill-->proc->flags |= PF_EXITING
                                                                 -->wakeup_proc-->do_wait-->do_exit
SYS_getpid      : get the process's pid

*/

// the process set's list
list_entry_t proc_list;

#define HASH_SHIFT 10
#define HASH_LIST_SIZE (1 << HASH_SHIFT)
#define pid_hashfn(x) (hash32(x, HASH_SHIFT))

// has list for process set based on pid
static list_entry_t hash_list[HASH_LIST_SIZE];

// idle proc
struct proc_struct *idleproc = NULL;
// init proc
struct proc_struct *initproc = NULL;
// current proc
struct proc_struct *current = NULL;

static int nr_process = 0;

void kernel_thread_entry(void);
void forkrets(struct trapframe *tf);
void switch_to(struct context *from, struct context *to);
static void put_pgdir(struct mm_struct *mm);
static int user_main(void *arg);

//lab4 function2 2413669

static struct proc_struct *
alloc_proc(void)
{
    struct proc_struct *proc = kmalloc(sizeof(struct proc_struct));
    if (proc == NULL)
    {
        return NULL;
    }

    memset(proc, 0, sizeof(struct proc_struct));
    proc->state = PROC_UNINIT;
    proc->pid = -1;
    proc->pgdir = boot_pgdir_pa;

    list_init(&proc->list_link);
    list_init(&proc->hash_link);

    return proc;
}

//lab5 function2 YOUR CODE update alloc_proc


// set_proc_name - set the name of proc
char *
set_proc_name(struct proc_struct *proc, const char *name)
{
    memset(proc->name, 0, sizeof(proc->name));
    return memcpy(proc->name, name, PROC_NAME_LEN);
}

// get_proc_name - get the name of proc
char *
get_proc_name(struct proc_struct *proc)
{
    static char name[PROC_NAME_LEN + 1];
    memset(name, 0, sizeof(name));
    return memcpy(name, proc->name, PROC_NAME_LEN);
}

// set_links - set the relation links of process
static void
set_links(struct proc_struct *proc)
{
    list_add(&proc_list, &(proc->list_link));
    proc->yptr = NULL;
    if ((proc->optr = proc->parent->cptr) != NULL)
    {
        proc->optr->yptr = proc;
    }
    proc->parent->cptr = proc;
    nr_process++;
}

// remove_links - clean the relation links of process
static void
remove_links(struct proc_struct *proc)
{
    list_del(&(proc->list_link));
    if (proc->optr != NULL)
    {
        proc->optr->yptr = proc->yptr;
    }
    if (proc->yptr != NULL)
    {
        proc->yptr->optr = proc->optr;
    }
    else
    {
        proc->parent->cptr = proc->optr;
    }
    nr_process--;
}

// get_pid - alloc a unique pid for process
static int
get_pid(void)
{
    static_assert(MAX_PID > MAX_PROCESS);
    struct proc_struct *proc;
    list_entry_t *list = &proc_list, *le;
    static int next_safe = MAX_PID, last_pid = MAX_PID;
    if (++last_pid >= MAX_PID)
    {
        last_pid = 1;
        goto inside;
    }
    if (last_pid >= next_safe)
    {
    inside:
        next_safe = MAX_PID;
    repeat:
        le = list;
        while ((le = list_next(le)) != list)
        {
            proc = le2proc(le, list_link);
            if (proc->pid == last_pid)
            {
                if (++last_pid >= next_safe)
                {
                    if (last_pid >= MAX_PID)
                    {
                        last_pid = 1;
                    }
                    next_safe = MAX_PID;
                    goto repeat;
                }
            }
            else if (proc->pid > last_pid && next_safe > proc->pid)
            {
                next_safe = proc->pid;
            }
        }
    }
    return last_pid;
}






// forkret -- the first kernel entry point of a new thread/process
// NOTE: the addr of forkret is setted in copy_thread function
//       after switch_to, the current proc will execute here.
static void
forkret(void)
{
    forkrets(current->tf);
}

// hash_proc - add proc into proc hash_list
static void
hash_proc(struct proc_struct *proc)
{
    list_add(hash_list + pid_hashfn(proc->pid), &(proc->hash_link));
}

// unhash_proc - delete proc from proc hash_list
static void
unhash_proc(struct proc_struct *proc)
{
    list_del(&(proc->hash_link));
}

// find_proc - find proc frome proc hash_list according to pid
struct proc_struct *
find_proc(int pid)
{
    if (0 < pid && pid < MAX_PID)
    {
        list_entry_t *list = hash_list + pid_hashfn(pid), *le = list;
        while ((le = list_next(le)) != list)
        {
            struct proc_struct *proc = le2proc(le, hash_link);
            if (proc->pid == pid)
            {
                return proc;
            }
        }
    }
    return NULL;
}

//lab5 function2 YOUR CODE update kernel_thread


// setup_kstack - alloc pages with size KSTACKPAGE as process kernel stack
static int
setup_kstack(struct proc_struct *proc)
{
    struct Page *page = alloc_pages(KSTACKPAGE);
    if (page != NULL)
    {
        proc->kstack = (uintptr_t)page2kva(page);
        return 0;
    }
    return -E_NO_MEM;
}

// put_kstack - free the memory space of process kernel stack
static void
put_kstack(struct proc_struct *proc)
{
    free_pages(kva2page((void *)(proc->kstack)), KSTACKPAGE);
}

// do_exit - called by sys_exit
//   1. call exit_mmap & put_pgdir & mm_destroy to free the almost all memory space of process
//   2. set process' state as PROC_ZOMBIE, then call wakeup_proc(parent) to ask parent reclaim itself.
//   3. call scheduler to switch to other process
int
do_exit(int error_code)
{
    struct mm_struct *old_mm;
    bool last_mm_ref = 0, adopted_zombie = 0;
    bool intr_flag;

    if (current == NULL || current == idleproc || current == initproc)
    {
        panic("do_exit: invalid current process");
    }

    local_intr_save(intr_flag);
    old_mm = current->mm;
    if (old_mm != NULL)
    {
        current->mm = NULL;
        current->pgdir = boot_pgdir_pa;
        lsatp(boot_pgdir_pa);
        flush_tlb();
        last_mm_ref = (mm_count_dec(old_mm) == 0);
    }
    local_intr_restore(intr_flag);

    if (last_mm_ref)
    {
        exit_mmap(old_mm);
        put_pgdir(old_mm);
        mm_destroy(old_mm);
    }

    local_intr_save(intr_flag);
    current->exit_code = error_code;
    current->state = PROC_ZOMBIE;
    current->wait_state = 0;

    struct proc_struct *parent = current->parent;
    if (parent != NULL && parent->state == PROC_SLEEPING && parent->wait_state == WT_CHILD)
    {
        wakeup_proc(parent);
    }

    while (current->cptr != NULL)
    {
        struct proc_struct *child = current->cptr;
        current->cptr = child->optr;
        if (current->cptr != NULL)
        {
            current->cptr->yptr = NULL;
        }

        child->parent = initproc;
        child->yptr = NULL;
        child->optr = initproc->cptr;
        if (child->optr != NULL)
        {
            child->optr->yptr = child;
        }
        initproc->cptr = child;
        if (child->state == PROC_ZOMBIE)
        {
            adopted_zombie = 1;
        }
    }

    if (adopted_zombie && initproc->state == PROC_SLEEPING && initproc->wait_state == WT_CHILD)
    {
        wakeup_proc(initproc);
    }
    local_intr_restore(intr_flag);

    schedule();
    panic("process exit!!.\n");
}

int
do_wait(int pid, int *code_store)
{
    bool intr_flag;

    if (current == NULL || pid < 0)
    {
        return -E_BAD_PROC;
    }
    if (code_store != NULL &&
        ((uintptr_t)code_store % sizeof(*code_store) != 0 ||
         !user_mem_check(current->mm, (uintptr_t)code_store, sizeof(*code_store), 1)))
    {
        return -E_FAULT;
    }

    for (;;)
    {
        struct proc_struct *zombie = NULL;
        bool has_match = 0;

        local_intr_save(intr_flag);
        if (current->flags & PF_EXITING)
        {
            local_intr_restore(intr_flag);
            return do_exit(-E_KILLED);
        }

        if (pid == 0)
        {
            for (struct proc_struct *child = current->cptr; child != NULL; child = child->optr)
            {
                has_match = 1;
                if (child->state == PROC_ZOMBIE)
                {
                    zombie = child;
                    break;
                }
            }
        }
        else
        {
            struct proc_struct *child = find_proc(pid);
            if (child != NULL && child->parent == current)
            {
                has_match = 1;
                if (child->state == PROC_ZOMBIE)
                {
                    zombie = child;
                }
            }
        }
        if (!has_match)
        {
            local_intr_restore(intr_flag);
            return -E_BAD_PROC;
        }
        if (zombie == NULL)
        {
            current->state = PROC_SLEEPING;
            current->wait_state = WT_CHILD;
            local_intr_restore(intr_flag);
            schedule();
            continue;
        }

        int exit_code = zombie->exit_code;
        local_intr_restore(intr_flag);
        if (code_store != NULL &&
            !copy_to_user(current->mm, code_store, &exit_code, sizeof(exit_code)))
        {
            return -E_FAULT;
        }

        local_intr_save(intr_flag);
        assert(zombie->parent == current && zombie->state == PROC_ZOMBIE);
        unhash_proc(zombie);
        remove_links(zombie);
        local_intr_restore(intr_flag);

        put_kstack(zombie);
        kfree(zombie);
        return 0;
    }
}

// init_main - the second kernel thread used to create user_main kernel threads
static int
init_main(void *arg)
{
    assert(current != NULL && current == initproc);
    assert(idleproc != NULL && current->parent == idleproc);
    assert(current->mm == NULL && current->pgdir == boot_pgdir_pa);
    assert(nr_process == 2 && current->cptr == NULL);

    cprintf("this initproc, pid = %d, name = \"%s\"\n",
            current->pid, get_proc_name(current));
    cprintf("To U: \"%s\".\n", (const char *)arg);
    cprintf("To U: \"en.., Bye, Bye. :)\"\n");

    size_t free_pages_before = nr_free_pages();
    size_t heap_before = kallocated();
    int child_pid = kernel_thread(user_main, NULL, 0);
    if (child_pid <= 0)
    {
        panic("init_main: kernel_thread returned %d", child_pid);
    }

    for (;;)
    {
        int wait_ret = do_wait(0, NULL);
        if (wait_ret == -E_BAD_PROC)
        {
            break;
        }
        if (wait_ret != 0)
        {
            panic("init_main: do_wait returned %d", wait_ret);
        }
    }

    assert(initproc->cptr == NULL && nr_process == 2);
    assert(list_next(&proc_list) == &initproc->list_link &&
           list_prev(&proc_list) == &initproc->list_link);
    assert(find_proc(initproc->pid) == initproc && find_proc(child_pid) == NULL);
    cprintf("all user-mode processes have quit.\n");

    size_t free_pages_after = nr_free_pages();
    size_t heap_after = kallocated();
    if (free_pages_after != free_pages_before || heap_after != heap_before)
    {
        panic("init_main: free pages %lu -> %lu, allocated bytes %lu -> %lu",
              (unsigned long)free_pages_before, (unsigned long)free_pages_after,
              (unsigned long)heap_before, (unsigned long)heap_after);
    }
    cprintf("init check memory pass.\n");

    intr_disable();
    for (;;)
    {
        kmonitor(NULL);
    }
}

// setup_pgdir - alloc one page as PDT
static int
setup_pgdir(struct mm_struct *mm)
{
    struct Page *page;
    if ((page = alloc_page()) == NULL)
    {
        return -E_NO_MEM;
    }
    pde_t *pgdir = page2kva(page);
    memcpy(pgdir, boot_pgdir_va, PGSIZE);

    mm->pgdir = pgdir;
    return 0;
}

// put_pgdir - free the memory space of PDT
static void
put_pgdir(struct mm_struct *mm)
{
    free_page(kva2page(mm->pgdir));
}

//lab5 function3 YOUR CODE update copy_mm

static int
copy_mm(uint32_t clone_flags, struct proc_struct *proc)
{
    (void)clone_flags;

    assert(current != NULL && proc != NULL);
    assert(current->mm == NULL);
    assert(proc->mm == NULL);

    return 0;
}

//lab5 function2 YOUR CODE

static void
copy_thread(struct proc_struct *proc, uintptr_t stack, struct trapframe *tf)
{
    assert(proc != NULL && tf != NULL);
    assert(proc->kstack != 0);

    proc->tf = (struct trapframe *)(proc->kstack + KSTACKSIZE -
                                    sizeof(struct trapframe));
    *proc->tf = *tf;
    proc->tf->gpr.a0 = 0;

    if (stack == 0)
    {
        proc->tf->gpr.sp = (uintptr_t)proc->tf;
    }
    else
    {
        proc->tf->gpr.sp = stack;
    }

    proc->context.ra = (uintptr_t)forkret;
    proc->context.sp = (uintptr_t)proc->tf;
}

//lab5 function3 YOUR CODE

int
do_fork(uint32_t clone_flags, uintptr_t stack, struct trapframe *tf)
{
    int ret = -E_NO_FREE_PROC;
    struct proc_struct *proc = NULL;
    bool intr_flag;

    assert(current != NULL && tf != NULL);

    if (nr_process >= MAX_PROCESS)
    {
        goto fork_out;
    }

    ret = -E_NO_MEM;
    proc = alloc_proc();
    if (proc == NULL)
    {
        goto fork_out;
    }

    proc->parent = current;

    if ((ret = setup_kstack(proc)) != 0)
    {
        goto bad_fork_cleanup_proc;
    }

    if ((ret = copy_mm(clone_flags, proc)) != 0)
    {
        goto bad_fork_cleanup_kstack;
    }

    copy_thread(proc, stack, tf);

    local_intr_save(intr_flag);
    {
        proc->pid = get_pid();
        hash_proc(proc);
        set_links(proc);

        wakeup_proc(proc);
        ret = proc->pid;
    }
    local_intr_restore(intr_flag);

fork_out:
    return ret;

bad_fork_cleanup_kstack:
    put_kstack(proc);

bad_fork_cleanup_proc:
    kfree(proc);
    goto fork_out;
}

//lab5 function4 YOUR CODE

int
kernel_thread(int (*fn)(void *), void *arg, uint32_t clone_flags)
{
    struct trapframe tf;

    memset(&tf, 0, sizeof(tf));
    tf.gpr.s0 = (uintptr_t)fn;
    tf.gpr.s1 = (uintptr_t)arg;
    tf.status = (read_csr(sstatus) | SSTATUS_SPP | SSTATUS_SPIE) &
                ~SSTATUS_SIE;
    tf.epc = (uintptr_t)kernel_thread_entry;

    return do_fork(clone_flags | CLONE_VM, 0, &tf);
}

void
proc_init(void)
{
    static const char idle_name[PROC_NAME_LEN + 1] = "idle";
    static const char init_name[PROC_NAME_LEN + 1] = "init";
    int pid;

    list_init(&proc_list);
    for (int i = 0; i < HASH_LIST_SIZE; i++)
    {
        list_init(&hash_list[i]);
    }
    nr_process = 0;

    idleproc = alloc_proc();
    if (idleproc == NULL)
    {
        panic("alloc idleproc failed.\n");
    }

    idleproc->pid = 0;
    idleproc->state = PROC_RUNNABLE;
    idleproc->kstack = (uintptr_t)bootstack;
    idleproc->need_resched = 1;
    set_proc_name(idleproc, idle_name);

    current = idleproc;
    nr_process++;

    pid = kernel_thread(init_main, "Hello world!!", 0);
    if (pid <= 0)
    {
        panic("create initproc failed: %d.\n", pid);
    }

    initproc = find_proc(pid);
    assert(initproc != NULL);
    set_proc_name(initproc, init_name);

    assert(current == idleproc);
    assert(idleproc->pid == 0);
    assert(initproc->pid == 1);
    assert(initproc->parent == idleproc);
    assert(initproc->state == PROC_RUNNABLE);
    assert(nr_process == 2);
}

//lab4 function3 2413669

void
proc_run(struct proc_struct *proc)
{
    if (proc == current)
    {
        return;
    }

    bool intr_flag;
    local_intr_save(intr_flag);

    struct proc_struct *prev = current;
    current = proc;

    write_csr(satp, 0x8000000000000000UL |
                         (proc->pgdir >> RISCV_PGSHIFT));
    flush_tlb();
    switch_to(&prev->context, &proc->context);

    local_intr_restore(intr_flag);
}

void
cpu_idle(void)
{
    while (1)
    {
        if (current->need_resched)
        {
            schedule();
        }
    }
}

// 2413669(Lab5): defer yielding to the Trap return scheduling path.
int
do_yield(void)
{
    if (current == NULL)
    {
        return -E_BAD_PROC;
    }
    current->need_resched = 1;
    return 0;
}

static int
kernel_execve(const char *name, unsigned char *binary, size_t size)
{
    if (current == NULL || current->kstack == 0 || current->tf == NULL ||
        current->mm != NULL || current->pgdir != boot_pgdir_pa)
    {
        return -E_BAD_PROC;
    }

    struct trapframe *target_tf = current->tf;
    uintptr_t tf_addr = (uintptr_t)target_tf;
    if (tf_addr < current->kstack ||
        tf_addr - current->kstack != KSTACKSIZE - sizeof(*target_tf))
    {
        return -E_BAD_PROC;
    }

    if (name == NULL || binary == NULL || size == 0)
    {
        return -E_INVAL;
    }
    size_t len = strlen(name);
    if (len == 0)
    {
        return -E_INVAL;
    }

    int ret = do_execve(name, len, binary, size);
    if (ret != 0)
    {
        return ret;
    }

    intr_disable();
    forkrets(target_tf);
    panic("forkrets returned to kernel_execve");
}

#define __KERNEL_EXECVE(name, binary, size) ({           \
    cprintf("kernel_execve: pid = %d, name = \"%s\".\n", \
            current->pid, name);                         \
    kernel_execve(name, binary, (size_t)(size));         \
})

#define KERNEL_EXECVE(x) ({                                    \
    extern unsigned char _binary_obj___user_##x##_out_start[], \
        _binary_obj___user_##x##_out_size[];                   \
    __KERNEL_EXECVE(#x, _binary_obj___user_##x##_out_start,    \
                    _binary_obj___user_##x##_out_size);        \
})

#define __KERNEL_EXECVE2(x, xstart, xsize) ({   \
    extern unsigned char xstart[], xsize[];     \
    __KERNEL_EXECVE(#x, xstart, (size_t)xsize); \
})

#define KERNEL_EXECVE2(x, xstart, xsize) __KERNEL_EXECVE2(x, xstart, xsize)

static int
user_main(void *arg)
{
    int ret;
    (void)arg;

#ifdef TEST
    ret = KERNEL_EXECVE2(TEST, TESTSTART, TESTSIZE);
#else
    ret = KERNEL_EXECVE(exit);
#endif
    if (ret < 0)
    {
        cprintf("user_main: kernel_execve failed with error %d\n", ret);
        return ret;
    }
    panic("user_main: kernel_execve unexpectedly returned %d", ret);
}

//lab5 function2 2412419

static int
load_icode(unsigned char *binary, size_t size)
{
    enum {
        ELFCLASS64 = 2, ELFDATA2LSB = 1, EV_CURRENT = 1,
        ET_EXEC = 2, EM_RISCV = 243
    };
    struct elfhdr elf;
    struct mm_struct *mm = NULL;
    uintptr_t previous_mem_end = 0, last_vma_end = 0;
    uint32_t previous_access = 0;
    bool entry_in_exec = 0;
    int ret = -E_INVAL_ELF;

    if (binary == NULL)
    {
        ret = -E_INVAL;
        goto out;
    }
    if (size < sizeof(elf))
    {
        goto out;
    }
    memcpy(&elf, binary, sizeof(elf));
    if (elf.e_magic != ELF_MAGIC || elf.e_elf[0] != ELFCLASS64 ||
        elf.e_elf[1] != ELFDATA2LSB || elf.e_elf[2] != EV_CURRENT ||
        elf.e_type != ET_EXEC || elf.e_machine != EM_RISCV ||
        elf.e_version != EV_CURRENT || elf.e_ehsize != sizeof(elf))
    {
        goto out;
    }
    if (elf.e_phnum == 0 || elf.e_phentsize != sizeof(struct proghdr) ||
        elf.e_phoff < sizeof(elf) || elf.e_phoff > size ||
        elf.e_phnum > (size - elf.e_phoff) / sizeof(struct proghdr))
    {
        goto out;
    }
    const unsigned char *ph_table = binary + elf.e_phoff;

    for (size_t i = 0; i < elf.e_phnum; ++i)
    {
        struct proghdr ph;
        memcpy(&ph, ph_table + i * sizeof(ph), sizeof(ph));
        if (ph.p_type != ELF_PT_LOAD)
        {
            continue;
        }
        if (ph.p_filesz > ph.p_memsz ||
            (ph.p_filesz != 0 &&
             (ph.p_offset > size || ph.p_filesz > size - ph.p_offset)))
        {
            goto out;
        }
        if (ph.p_memsz == 0)
        {
            continue;
        }
        if (ph.p_va < USERBASE || ph.p_va >= USERTOP ||
            ph.p_memsz > USERTOP - ph.p_va)
        {
            goto out;
        }
        if (ph.p_va % PGSIZE != ph.p_offset % PGSIZE ||
            (ph.p_align > 1 &&
             ((ph.p_align & (ph.p_align - 1)) != 0 ||
              ph.p_va % ph.p_align != ph.p_offset % ph.p_align)))
        {
            goto out;
        }

        uintptr_t mem_end = ph.p_va + ph.p_memsz;
        uint32_t access = ph.p_flags & (ELF_PF_R | ELF_PF_W | ELF_PF_X);
        if (access == 0 || ((access & ELF_PF_W) && !(access & ELF_PF_R)))
        {
            goto out;
        }
        if (ph.p_va < previous_mem_end ||
            (ROUNDDOWN(ph.p_va, PGSIZE) < previous_mem_end && access != previous_access))
        {
            goto out;
        }
        if ((access & ELF_PF_X) && ph.p_va <= elf.e_entry && elf.e_entry < mem_end)
        {
            entry_in_exec = 1;
        }
        previous_mem_end = mem_end;
        previous_access = access;
    }
    if (!entry_in_exec || (elf.e_entry & 1U) != 0)
    {
        goto out;
    }

    mm = mm_create();
    if (mm == NULL)
    {
        ret = -E_NO_MEM;
        goto out;
    }
    ret = setup_pgdir(mm);
    if (ret != 0)
    {
        goto discard_mm;
    }

    for (size_t i = 0; i < elf.e_phnum; ++i)
    {
        struct proghdr ph;
        memcpy(&ph, ph_table + i * sizeof(ph), sizeof(ph));
        if (ph.p_type != ELF_PT_LOAD || ph.p_memsz == 0)
        {
            continue;
        }

        uintptr_t page_begin = ROUNDDOWN(ph.p_va, PGSIZE);
        uintptr_t page_end = ROUNDUP(ph.p_va + ph.p_memsz, PGSIZE);
        uintptr_t file_end = ph.p_va + ph.p_filesz;
        uint32_t vm_flags = 0, perm = PTE_U;
        if (ph.p_flags & ELF_PF_R)
        {
            vm_flags |= VM_READ;
            perm |= PTE_R;
        }
        if (ph.p_flags & ELF_PF_W)
        {
            vm_flags |= VM_WRITE;
            perm |= PTE_W;
        }
        if (ph.p_flags & ELF_PF_X)
        {
            vm_flags |= VM_EXEC;
            perm |= PTE_X;
        }

        uintptr_t map_begin = page_begin > last_vma_end ? page_begin : last_vma_end;
        if (map_begin < page_end)
        {
            ret = mm_map(mm, map_begin, page_end - map_begin, vm_flags, NULL);
            if (ret != 0)
            {
                goto discard_image;
            }
            last_vma_end = page_end;
        }

        for (uintptr_t va = page_begin; va < page_end; va += PGSIZE)
        {
            struct Page *page = get_page(mm->pgdir, va, NULL);
            bool fresh = (page == NULL);
            if (fresh)
            {
                page = pgdir_alloc_page(mm->pgdir, va, perm);
                if (page == NULL)
                {
                    ret = -E_NO_MEM;
                    goto discard_image;
                }
            }
            unsigned char *kva = page2kva(page);
            if (fresh)
            {
                memset(kva, 0, PGSIZE);
            }

            uintptr_t copy_begin = va > ph.p_va ? va : ph.p_va;
            uintptr_t copy_end = va + PGSIZE < file_end ? va + PGSIZE : file_end;
            if (copy_begin < copy_end)
            {
                memcpy(kva + (copy_begin - va),
                       binary + ph.p_offset + (copy_begin - ph.p_va),
                       copy_end - copy_begin);
            }
        }
    }

    const uintptr_t stack_top = USTACKTOP;
    const uintptr_t stack_bottom = stack_top - USTACKSIZE;
    const size_t initial_stack_pages = 4;

    if (current == NULL || current->mm != NULL || current->tf == NULL)
    {
        ret = -E_BAD_PROC;
        goto discard_image;
    }
    struct trapframe *target_tf = current->tf;

    if (last_vma_end > stack_bottom)
    {
        ret = -E_INVAL_ELF;
        goto discard_image;
    }
    ret = mm_map(mm, stack_bottom, USTACKSIZE,
                 VM_READ | VM_WRITE | VM_STACK, NULL);
    if (ret != 0)
    {
        goto discard_image;
    }

    for (size_t i = 0; i < initial_stack_pages; ++i)
    {
        uintptr_t va = stack_top - (i + 1) * PGSIZE;
        struct Page *page = pgdir_alloc_page(mm->pgdir, va, PTE_U | PTE_R | PTE_W);
        if (page == NULL)
        {
            ret = -E_NO_MEM;
            goto discard_image;
        }
        memset(page2kva(page), 0, PGSIZE);
    }

    struct trapframe next_tf = {0};
    next_tf.gpr.sp = stack_top;
    next_tf.epc = elf.e_entry;
    next_tf.status = (read_csr(sstatus) | SSTATUS_SPIE | SSTATUS_SUM) &
                     ~(SSTATUS_SPP | SSTATUS_SIE);
    uintptr_t new_pgdir_pa = PADDR(mm->pgdir);

    bool intr_flag;
    local_intr_save(intr_flag);
    {
        mm_count_inc(mm);
        current->mm = mm;
        current->pgdir = new_pgdir_pa;
        lsatp(new_pgdir_pa);
        flush_tlb();
        *target_tf = next_tf;
        asm volatile("fence.i" ::: "memory");
    }
    local_intr_restore(intr_flag);
    return 0;

discard_image:
    exit_mmap(mm);
    put_pgdir(mm);
discard_mm:
    mm_destroy(mm);
out:
    return ret;
}

int
do_execve(const char *name, size_t len, unsigned char *binary, size_t size)
{
    if (current == NULL || current->tf == NULL)
    {
        return -E_BAD_PROC;
    }
    if (name == NULL || len == 0 || binary == NULL || size == 0)
    {
        return -E_INVAL;
    }

    char new_name[PROC_NAME_LEN + 1] = {0};
    memcpy(new_name, name, len < PROC_NAME_LEN ? len : PROC_NAME_LEN);

    struct mm_struct *old_mm;
    uintptr_t old_pgdir_pa;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        old_mm = current->mm;
        old_pgdir_pa = current->pgdir;
        current->mm = NULL;
        current->pgdir = boot_pgdir_pa;
        lsatp(boot_pgdir_pa);
        flush_tlb();
    }
    local_intr_restore(intr_flag);

    int ret = load_icode(binary, size);
    if (ret != 0)
    {
        local_intr_save(intr_flag);
        {
            current->mm = old_mm;
            current->pgdir = old_pgdir_pa;
            lsatp(old_pgdir_pa);
            flush_tlb();
        }
        local_intr_restore(intr_flag);
        return ret;
    }

    if (old_mm != NULL)
    {
        bool last_owner;
        local_intr_save(intr_flag);
        {
            last_owner = (mm_count_dec(old_mm) == 0);
        }
        local_intr_restore(intr_flag);

        if (last_owner)
        {
            exit_mmap(old_mm);
            put_pgdir(old_mm);
            mm_destroy(old_mm);
        }
    }

    set_proc_name(current, new_name);
    return 0;
}
