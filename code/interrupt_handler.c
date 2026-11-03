//lab3 function2 2412419

void interrupt_handler(struct trapframe *tf) {
    extern void sbi_shutdown(void);
    enum { SHUTDOWN_AFTER = 10 };
    static size_t tick_print_count = 0;

    uintptr_t cause = tf->cause & ~((uintptr_t)1 << 63);

    switch (cause) {
    case IRQ_U_SOFT:
        cprintf("User software interrupt\n");
        break;

    case IRQ_S_SOFT:
        cprintf("Supervisor software interrupt\n");
        break;

    case IRQ_M_SOFT:
        cprintf("Machine software interrupt\n");
        break;

    case IRQ_U_TIMER:
        cprintf("User timer interrupt\n");
        break;

    case IRQ_S_TIMER:
        clock_set_next_event();
        ticks++;

        if (ticks % TICK_NUM == 0) {
            print_ticks();
            tick_print_count++;

            if (tick_print_count >= SHUTDOWN_AFTER) {
                sbi_shutdown();
            }
        }
        break;

    case IRQ_M_TIMER:
        cprintf("Machine timer interrupt\n");
        break;

    case IRQ_U_EXT:
        cprintf("User external interrupt\n");
        break;

    case IRQ_S_EXT:
        cprintf("Supervisor external interrupt\n");
        break;

    case IRQ_M_EXT:
        cprintf("Machine external interrupt\n");
        break;

    default:
        cprintf("Unknown interrupt: cause %lu\n",
                (unsigned long)cause);
        print_trapframe(tf);
        break;
    }
}
