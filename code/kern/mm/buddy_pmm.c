#include <buddy_pmm.h>
#include <list.h>
#include <stdio.h>
#include <string.h>

#define BUDDY_MAX_ORDER 15

static list_entry_t buddy_free_lists[BUDDY_MAX_ORDER + 1];
static size_t buddy_nr_free;

static unsigned int
buddy_get_order(size_t n) {
    assert(n > 0);
    unsigned int order = 0;
    size_t remaining = n - 1;
    while (remaining > 0) {
        remaining /= 2;
        order++;
    }
    return order;
}

static void
buddy_init(void) {
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_init(&buddy_free_lists[order]);
    }
    buddy_nr_free = 0;
}

/* List membership does not change the total count: splitting and merging
 * only reorganize pages which are already accounted for. */
static void
buddy_add_block(struct Page *base, unsigned int order) {
    assert(order <= BUDDY_MAX_ORDER);
    assert(page2ppn(base) % (1UL << order) == 0);
    base->property = 1UL << order;
    SetPageProperty(base);
    list_add(&buddy_free_lists[order], &base->page_link);
}

/* Both initialization and free use absolute PPN alignment and XOR pairing.
 * Coalescing here also supports adjacent init_memmap calls. */
static void
buddy_merge_block(struct Page *base, unsigned int order) {
    while (order < BUDDY_MAX_ORDER) {
        size_t block_pages = 1UL << order;
        size_t base_ppn = page2ppn(base);
        size_t buddy_ppn = base_ppn ^ block_pages;
        if (buddy_ppn < nbase || buddy_ppn >= npage ||
            block_pages > npage - buddy_ppn) {
            break;
        }
        struct Page *buddy = &pages[buddy_ppn - nbase];
        if (!PageProperty(buddy) || buddy->property != block_pages) {
            break;
        }
        list_del(&buddy->page_link);
        ClearPageProperty(buddy);
        buddy->property = 0;
        if (buddy < base) {
            base = buddy;
        }
        order++;
    }
    buddy_add_block(base, order);
}

static void
buddy_init_memmap(struct Page *base, size_t n) {
    assert(n > 0);
    assert(base >= pages && base < pages + (npage - nbase));
    assert(n <= npage - page2ppn(base));
    struct Page *p = base;
    for (; p != base + n; p++) {
        assert(PageReserved(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    p = base;
    size_t remaining = n;
    while (remaining > 0) {
        unsigned int order = BUDDY_MAX_ORDER;
        while ((1UL << order) > remaining ||
               page2ppn(p) % (1UL << order) != 0) {
            order--;
        }
        size_t block_pages = 1UL << order;
        buddy_merge_block(p, order);
        p += block_pages;
        remaining -= block_pages;
    }
    buddy_nr_free += n;
}

static int
buddy_find_free_order(unsigned int target_order) {
    for (unsigned int order = target_order; order <= BUDDY_MAX_ORDER; order++) {
        if (!list_empty(&buddy_free_lists[order])) {
            return (int)order;
        }
    }
    return -1;
}

static struct Page *
buddy_alloc_pages(size_t n) {
    assert(n > 0);
    if (n > buddy_nr_free) {
        return NULL;
    }
    unsigned int target_order = buddy_get_order(n);
    int found_order = buddy_find_free_order(target_order);
    if (found_order < 0) {
        return NULL;
    }
    unsigned int order = (unsigned int)found_order;
    list_entry_t *le = list_next(&buddy_free_lists[order]);
    struct Page *page = le2page(le, page_link);
    list_del(le);
    ClearPageProperty(page);
    while (order > target_order) {
        order--;
        struct Page *buddy = page + (1UL << order);
        buddy_add_block(buddy, order);
    }
    size_t allocated_pages = 1UL << target_order;
    page->property = allocated_pages;  /* allocated size, PG_property is clear */
    buddy_nr_free -= allocated_pages;
    return page;
}

static void
buddy_free_pages(struct Page *base, size_t n) {
    assert(n > 0);
    assert(base >= pages && base < pages + (npage - nbase));
    unsigned int order = buddy_get_order(n);
    assert(order <= BUDDY_MAX_ORDER);
    size_t released_pages = 1UL << order;
    assert(released_pages <= npage - page2ppn(base));
    assert(page2ppn(base) % released_pages == 0);
    assert(!PageReserved(base) && !PageProperty(base));
    assert(base->property == released_pages);
    for (struct Page *p = base; p != base + released_pages; p++) {
        assert(!PageReserved(p) && !PageProperty(p));
        p->flags = p->property = 0;
        set_page_ref(p, 0);
    }
    buddy_merge_block(base, order);
    /* Partners were already free: count only the newly released block. */
    buddy_nr_free += released_pages;
}

static size_t
buddy_nr_free_pages(void) {
    return buddy_nr_free;
}

/* Check list links, flags, alignment, non-overlap, bounds and accounting
 * independently of allocation order. The lab manages at most 128 MiB. */
static unsigned char checked_pages[(1UL << BUDDY_MAX_ORDER) / 8];
static void
buddy_check_invariants(void) {
    assert(npage - nbase <= (1UL << BUDDY_MAX_ORDER));
    memset(checked_pages, 0, sizeof(checked_pages));
    size_t total = 0;
    for (unsigned int order = 0; order <= BUDDY_MAX_ORDER; order++) {
        list_entry_t *head = &buddy_free_lists[order];
        size_t count = 0, block_pages = 1UL << order;
        for (list_entry_t *le = list_next(head); le != head; le = list_next(le)) {
            assert(++count <= npage - nbase);
            assert(le->next->prev == le && le->prev->next == le);
            struct Page *p = le2page(le, page_link);
            assert(p >= pages && p < pages + (npage - nbase));
            assert(PageProperty(p) && !PageReserved(p));
            assert(p->property == block_pages);
            assert(page2ppn(p) % block_pages == 0);
            assert(block_pages <= npage - page2ppn(p));
            for (size_t j = 0; j < block_pages; j++) {
                size_t index = p + j - pages;
                unsigned char bit = 1U << (index % 8);
                assert(!(checked_pages[index / 8] & bit));
                checked_pages[index / 8] |= bit;
                assert(!PageReserved(p + j) && page_ref(p + j) == 0);
                if (j != 0) {
                    assert(!PageProperty(p + j) && p[j].property == 0);
                }
            }
            if (order < BUDDY_MAX_ORDER) {
                size_t other = page2ppn(p) ^ block_pages;
                if (other >= nbase && other < npage) {
                    struct Page *q = &pages[other - nbase];
                    assert(!PageProperty(q) || q->property != block_pages);
                }
            }
            total += block_pages;
        }
    }
    assert(total == buddy_nr_free);
}

/* A descriptor-only fixture, not a claim that these are physical free pages.
 * The real descriptor array and all list sentinels are restored afterwards. */
static struct Page fixture[64];
static void
fixture_reset(size_t count) {
    buddy_init();
    npage = nbase + count;
    memset(fixture, 0, sizeof(fixture));
    for (size_t i = 0; i < count; i++) {
        SetPageReserved(&fixture[i]);
    }
}

static void
buddy_check_fixture(void) {
    struct Page *saved_pages = pages;
    size_t saved_npage = npage, saved_free = buddy_nr_free;
    list_entry_t saved_lists[BUDDY_MAX_ORDER + 1];
    for (unsigned int i = 0; i <= BUDDY_MAX_ORDER; i++) {
        saved_lists[i] = buddy_free_lists[i];
    }
    pages = fixture;
    fixture_reset(64);
    assert(buddy_alloc_pages(1) == NULL);
    assert(buddy_alloc_pages((size_t)-1) == NULL);
    cprintf("[Buddy 1] empty pool and oversized request: PASS\n");

    buddy_init_memmap(fixture + 1, 13);
    assert(buddy_nr_free == 13);
    assert(buddy_alloc_pages(8) == NULL);
    assert(buddy_alloc_pages(13) == NULL);
    struct Page *single[13];
    for (size_t i = 0; i < 13; i++) {
        single[i] = buddy_alloc_pages(1);
        assert(single[i] >= fixture + 1 && single[i] < fixture + 14);
    }
    assert(buddy_nr_free == 0 && buddy_alloc_pages(1) == NULL);
    for (size_t i = 0; i < 13; i++) {
        buddy_free_pages(single[(i * 5) % 13], 1);
    }
    buddy_check_invariants();
    assert(buddy_nr_free == 13 && PageReserved(fixture));
    assert(PageReserved(fixture + 14));
    cprintf("[Buddy 2] unaligned 13-page region, exhaust and restore: PASS\n");

    fixture_reset(64);
    buddy_init_memmap(fixture, 64);
    size_t requests[] = {1, 2, 3, 5, 8, 16};
    size_t expected[] = {1, 2, 4, 8, 8, 16};
    struct Page *allocated[6];
    size_t used = 0;
    for (size_t i = 0; i < 6; i++) {
        allocated[i] = buddy_alloc_pages(requests[i]);
        assert(allocated[i] != NULL && allocated[i]->property == expected[i]);
        assert(!PageProperty(allocated[i]));
        used += expected[i];
        assert(buddy_nr_free == 64 - used);
        buddy_check_invariants();
    }
    for (int i = 5; i >= 0; i--) {
        buddy_free_pages(allocated[i], requests[i]);
        buddy_check_invariants();
    }
    assert(buddy_nr_free == 64 && fixture->property == 64);
    cprintf("[Buddy 3] orders 0..4, rounding 3->4 and 5->8: PASS\n");

    struct Page *quarters[4];
    for (size_t i = 0; i < 4; i++) {
        quarters[i] = buddy_alloc_pages(16);
        assert(quarters[i] == fixture + i * 16);
    }
    buddy_free_pages(quarters[1], 16);
    buddy_free_pages(quarters[2], 16);
    assert(buddy_nr_free == 32 && buddy_alloc_pages(32) == NULL);
    buddy_free_pages(quarters[0], 16);
    struct Page *half = buddy_alloc_pages(32);
    assert(half == fixture);
    buddy_free_pages(quarters[3], 16);
    buddy_free_pages(half, 32);
    buddy_check_invariants();
    assert(buddy_nr_free == 64 && fixture->property == 64);
    cprintf("[Buddy 4] adjacent non-buddies stay separate; recursive merge: PASS\n");

    fixture_reset(13);
    buddy_init_memmap(fixture, 13);
    struct Page *tail = buddy_alloc_pages(1);
    assert(tail == fixture + 12);
    buddy_free_pages(tail, 1);  /* XOR partner is beyond npage. */
    buddy_check_invariants();
    assert(buddy_nr_free == 13);
    cprintf("[Buddy 5] upper-bound partner never dereferenced: PASS\n");

    fixture_reset(64);
    buddy_init_memmap(fixture, 8);
    buddy_init_memmap(fixture + 8, 8);
    struct Page *joined = buddy_alloc_pages(16);
    assert(joined == fixture && buddy_nr_free == 0);
    buddy_free_pages(joined, 16);
    buddy_check_invariants();
    cprintf("[Buddy 6] adjacent init_memmap regions coalesce: PASS\n");

    pages = saved_pages;
    npage = saved_npage;
    buddy_nr_free = saved_free;
    for (unsigned int i = 0; i <= BUDDY_MAX_ORDER; i++) {
        buddy_free_lists[i] = saved_lists[i];
    }
    buddy_check_invariants();
}

static void
buddy_check(void) {
    size_t initial = buddy_nr_free;
    buddy_check_invariants();
    cprintf("Buddy self-test start: free=%lu pages\n", initial);
    buddy_check_fixture();

    /* Real RAM test: every successful allocation is disjoint, writable,
     * and retains data while other blocks are allocated/freed. */
    struct Page *slots[32] = {0};
    size_t sizes[32] = {0};
    uint32_t state = 0x20261021;
    for (size_t step = 0; step < 512; step++) {
        state = state * 1664525U + 1013904223U;
        size_t i = (state >> 16) % 32;
        if (slots[i] != NULL) {
            buddy_free_pages(slots[i], sizes[i]);
            slots[i] = NULL;
        } else {
            size_t n = 1 + ((state >> 8) % 31);
            struct Page *p = buddy_alloc_pages(n);
            assert(p != NULL);
            slots[i] = p;
            sizes[i] = n;
            size_t bytes = p->property * PGSIZE;
            volatile unsigned char *data =
                (volatile unsigned char *)(page2pa(p) + va_pa_offset);
            data[0] = (unsigned char)(i + 1);
            data[bytes - 1] = (unsigned char)(255 - i);
        }
        size_t used = 0;
        for (size_t a = 0; a < 32; a++) {
            if (slots[a] == NULL) {
                continue;
            }
            struct Page *p = slots[a];
            used += p->property;
            volatile unsigned char *data =
                (volatile unsigned char *)(page2pa(p) + va_pa_offset);
            assert(data[0] == a + 1);
            assert(data[p->property * PGSIZE - 1] == 255 - a);
            for (size_t b = a + 1; b < 32; b++) {
                if (slots[b] != NULL) {
                    assert(p + p->property <= slots[b] ||
                           slots[b] + slots[b]->property <= p);
                }
            }
        }
        assert(buddy_nr_free + used == initial);
        if (step % 64 == 0) {
            buddy_check_invariants();
        }
    }
    for (size_t i = 0; i < 32; i++) {
        if (slots[i] != NULL) {
            buddy_free_pages(slots[i], sizes[i]);
        }
    }
    buddy_check_invariants();
    assert(buddy_nr_free == initial);
    cprintf("[Buddy 7] 512 real-RAM operations, disjointness and data: PASS\n");
    assert(buddy_alloc_pages(initial + 1) == NULL);
    assert(buddy_alloc_pages((1UL << BUDDY_MAX_ORDER) + 1) == NULL);
    cprintf("[Buddy 8] capacity failure leaves state unchanged: PASS\n");
    cprintf("Buddy self-test: ALL PASS; initial=%lu final=%lu\n",
            initial, buddy_nr_free);
}

const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
