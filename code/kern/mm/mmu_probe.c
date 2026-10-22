#include <defs.h>
#include <stdio.h>

#ifdef LAB2_MMU_PROBE
/* Dedicated page prevents unrelated global accesses warming the data TLB. */
volatile uint64_t mmu_probe_data[512] __attribute__((aligned(4096))) = {
    0x1122334455667788ULL
};
volatile uint64_t mmu_probe_result[2];

void __attribute__((noinline))
lab2_mmu_probe(void) {
    uintptr_t address = (uintptr_t)mmu_probe_data;
    uint64_t first, second;
    asm volatile(
        "sfence.vma\n"
        ".global lab2_probe_first\n"
        "lab2_probe_first:\n"
        "ld %0, 0(%2)\n"
        ".global lab2_probe_second\n"
        "lab2_probe_second:\n"
        "ld %1, 0(%2)\n"
        ".global lab2_probe_done\n"
        "lab2_probe_done:\n"
        : "=&r"(first), "=&r"(second) : "r"(address) : "memory");
    mmu_probe_result[0] = first;
    mmu_probe_result[1] = second;
    cprintf("MMU probe: VA=0x%016lx first=0x%016lx second=0x%016lx\n",
            address, first, second);
}
#endif
