#include <default_pmm.h>
#include <best_fit_pmm.h>
#include <defs.h>
#include <error.h>
#include <memlayout.h>
#include <mmu.h>
#include <pmm.h>
#include <sbi.h>
#include <stdio.h>
#include <string.h>
#include <../sync/sync.h>
#include <riscv.h>
#include <dtb.h>

// virtual address of physical page array
struct Page *pages;
// amount of physical memory (in pages)
size_t npage = 0;
// the kernel image is mapped at VA=KERNBASE and PA=info.base
uint64_t va_pa_offset;
// memory starts at 0x80000000 in RISC-V
// DRAM_BASE defined in riscv.h as 0x80000000
const size_t nbase = DRAM_BASE / PGSIZE;

// virtual address of boot-time page directory
uintptr_t *satp_virtual = NULL;
// physical address of boot-time page directory
uintptr_t satp_physical;

// physical memory management
const struct pmm_manager *pmm_manager;


static void check_alloc_page(void);

// init_pmm_manager - initialize a pmm_manager instance
static void init_pmm_manager(void) {
    pmm_manager = &best_fit_pmm_manager;
    cprintf("memory management: %s\n", pmm_manager->name);
    pmm_manager->init();
}

// init_memmap - call pmm->init_memmap to build Page struct for free memory
static void init_memmap(struct Page *base, size_t n) {
    pmm_manager->init_memmap(base, n);
}

// alloc_pages - call pmm->alloc_pages to allocate a continuous n*PAGESIZE
// memory
struct Page *alloc_pages(size_t n) {
    struct Page *page = NULL;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        page = pmm_manager->alloc_pages(n);
    }
    local_intr_restore(intr_flag);
    return page;
}

// free_pages - call pmm->free_pages to free a continuous n*PAGESIZE memory
void free_pages(struct Page *base, size_t n) {
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        pmm_manager->free_pages(base, n);
    }
    local_intr_restore(intr_flag);
}

// nr_free_pages - call pmm->nr_free_pages to get the size (nr*PAGESIZE)
// of current free memory
size_t nr_free_pages(void) {
    size_t ret;
    bool intr_flag;
    local_intr_save(intr_flag);
    {
        ret = pmm_manager->nr_free_pages();
    }
    local_intr_restore(intr_flag);
    return ret;
}

/*
 * Initialize the physical page descriptors and hand the usable physical
 * memory after the kernel and descriptor array to the current PMM.
 */
static void
page_init(void) {
    extern char end[];

    va_pa_offset = PHYSICAL_MEMORY_OFFSET;

    uint64_t mem_base = get_memory_base();
    uint64_t mem_size = get_memory_size();
    if (mem_base < DRAM_BASE || mem_size == 0 ||
        mem_base > (uint64_t)-1 - mem_size) {
        panic("page_init: invalid physical memory range");
    }

    uint64_t mem_end = mem_base + mem_size;
    uint64_t mem_limit = KERNTOP - PHYSICAL_MEMORY_OFFSET;
    if (mem_end > mem_limit) {
        panic("page_init: physical memory exceeds supported range");
    }

    /*
     * Page descriptors cover every physical page number in [nbase, npage).
     * The last partial page, if any, is not represented in this range.
     */
    npage = PPN(mem_end);
    if (npage <= nbase) {
        panic("page_init: no manageable physical pages");
    }

    uintptr_t kernel_end_pa = PADDR((uintptr_t)end);
    if (kernel_end_pa < mem_base || kernel_end_pa >= mem_end) {
        panic("page_init: kernel end is outside physical memory");
    }

    /*
     * Place the descriptor array after the kernel image. Its first page must
     * be aligned, while the array itself may end in the middle of a page.
     */
    uintptr_t pages_pa = ROUNDUP(kernel_end_pa, PGSIZE);
    size_t descriptor_count = npage - nbase;
    if (descriptor_count > (uint64_t)-1 / sizeof(struct Page)) {
        panic("page_init: page descriptor array is too large");
    }

    size_t pages_bytes = descriptor_count * sizeof(struct Page);
    if (pages_pa > (uint64_t)-1 - pages_bytes) {
        panic("page_init: page descriptor array address overflow");
    }
    uintptr_t pages_end_pa = pages_pa + pages_bytes;
    if (pages_end_pa > mem_end) {
        panic("page_init: page descriptor array exceeds physical memory");
    }

    pages = (struct Page *)(pages_pa + va_pa_offset);

    /*
     * Reserved is the safe initial state. init_memmap() clears it only for
     * the pages that are subsequently added to its free list.
     */
    for (size_t i = 0; i < descriptor_count; i++) {
        struct Page *page = &pages[i];
        page->ref = 0;
        page->flags = 0;
        page->property = 0;
        list_init(&page->page_link);
        SetPageReserved(page);
    }

    /*
     * Only complete pages in the DTB range are released. Rounding the start
     * up and the end down prevents partial pages from being made allocatable.
     */
    uintptr_t free_start_pa = ROUNDUP(pages_end_pa, PGSIZE);
    uintptr_t dram_start_pa = ROUNDUP(mem_base, PGSIZE);
    if (free_start_pa < dram_start_pa) {
        free_start_pa = dram_start_pa;
    }
    uintptr_t free_end_pa = ROUNDDOWN(mem_end, PGSIZE);
    if (free_start_pa >= free_end_pa) {
        panic("page_init: no complete physical pages available");
    }

    size_t nr_pages = (free_end_pa - free_start_pa) / PGSIZE;
    struct Page *free_base = pa2page(free_start_pa);
    init_memmap(free_base, nr_pages);

    cprintf("physical memory: [0x%016lx, 0x%016lx), %lu MB\n",
            (unsigned long)mem_base, (unsigned long)mem_end,
            (unsigned long)(mem_size / (1024 * 1024)));
    cprintf("page array: [0x%016lx, 0x%016lx), %lu descriptors\n",
            (unsigned long)pages_pa, (unsigned long)pages_end_pa,
            (unsigned long)descriptor_count);
    cprintf("free memory: [0x%016lx, 0x%016lx), %lu pages\n",
            (unsigned long)free_start_pa, (unsigned long)free_end_pa,
            (unsigned long)nr_pages);
}

/* pmm_init - initialize the physical memory management */
void pmm_init(void) {
    // We need to alloc/free the physical memory (granularity is 4KB or other size).
    // So a framework of physical memory manager (struct pmm_manager)is defined in pmm.h
    // First we should init a physical memory manager(pmm) based on the framework.
    // Then pmm can alloc/free the physical memory.
    // Now the first_fit/best_fit/worst_fit/buddy_system pmm are available.
    init_pmm_manager();

    // detect physical memory space, reserve already used memory,
    // then use pmm->init_memmap to create free page list
    page_init();

    // use pmm->check to verify the correctness of the alloc/free function in a pmm
    check_alloc_page();

    extern char boot_page_table_sv39[];
    satp_virtual = (pte_t*)boot_page_table_sv39;
    satp_physical = PADDR(satp_virtual);
    cprintf("satp virtual address: 0x%016lx\nsatp physical address: 0x%016lx\n", satp_virtual, satp_physical);
}

static void check_alloc_page(void) {
    pmm_manager->check();
    cprintf("check_alloc_page() succeeded!\n");
}
