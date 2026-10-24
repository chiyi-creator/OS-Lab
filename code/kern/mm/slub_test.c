#include <slub.h>
#include <assert.h>
#include <pmm.h>
#include <stdio.h>

#define SLUB_TEST_COUNT 300

/* Static storage keeps the boot stack small. Tests are not reentrant. */
static void *objects[SLUB_TEST_COUNT];
static const size_t sizes[] = {// 看边界是否正确
    1, 15, 16, 17, 31, 32, 33, 63, 64, 65,
    127, 128, 129, 255, 256, 257, 511, 512, 513,
    1023, 1024, 1025, 2047, 2048
};

static uint8_t
pattern(size_t object, size_t byte) {
    return (uint8_t)(object * 37 + byte * 13 + (byte >> 8));
}

static void
fill_object(void *ptr, size_t size, size_t tag) {// 写入数据
    uint8_t *bytes = ptr;
    for (size_t j = 0; j < size; j++) {
        bytes[j] = pattern(tag, j);
    }
}

static void
check_object(void *ptr, size_t size, size_t tag) {
    const uint8_t *bytes = ptr;
    for (size_t j = 0; j < size; j++) {
        assert(bytes[j] == pattern(tag, j));
    }
}

static uintptr_t
object_page(void *ptr) {
    return (uintptr_t)ptr & ~((uintptr_t)PGSIZE - 1);
}

static void
check_sizes(void) { // 不同申请大小、边界、16字节对齐、数据完整性
    size_t baseline = nr_free_pages();
    size_t count = sizeof(sizes) / sizeof(sizes[0]);
    for (size_t i = 0; i < count; i++) {
        objects[i] = slub_alloc(sizes[i]);
        assert(objects[i] != NULL);
        assert(((uintptr_t)objects[i] & 15) == 0);// 满足16字节对齐
        for (size_t j = 0; j < i; j++) {// 申请的对象不能重复
            assert(objects[i] != objects[j]);
        }
        fill_object(objects[i], sizes[i], i);
        /* Verify earlier live allocations after each new write. */
        for (size_t j = 0; j <= i; j++) {
            check_object(objects[j], sizes[j], j);
        }
    }
    /* Reverse order also exercises non-FIFO release. */
    for (size_t i = count; i > 0; i--) {// 倒序释放，释放不依赖于存入顺序
        check_object(objects[i - 1], sizes[i - 1], i - 1);
        slub_free(objects[i - 1]);
        objects[i - 1] = NULL;
        for (size_t j = 0; j + 1 < i; j++) {
            check_object(objects[j], sizes[j], j);
        }
    }
    assert(nr_free_pages() == baseline);
    cprintf("slub_check: sizes, boundaries and alignment passed\n");
}

static void
check_reuse(void) {// 释放后再申请，检查是否复用
    size_t baseline = nr_free_pages();
    void *a = slub_alloc(64);
    void *b = slub_alloc(64);
    void *c = slub_alloc(64);
    assert(a != NULL && b != NULL && c != NULL);
    assert(a != b && a != c && b != c);
    assert(object_page(a) == object_page(b));
    assert(object_page(b) == object_page(c));
    fill_object(a, 64, 1);
    fill_object(b, 64, 2);
    fill_object(c, 64, 3);
    size_t allocated = nr_free_pages();

    /* Keep two objects live so freeing b cannot return its whole page. */
    slub_free(b);
    assert(nr_free_pages() == allocated);
    check_object(a, 64, 1);
    check_object(c, 64, 3);
    void *replacement = slub_alloc(64);// 再次申请
    assert(replacement == b);
    assert(nr_free_pages() == allocated);
    fill_object(replacement, 64, 4);
    check_object(a, 64, 1);
    check_object(c, 64, 3);
    check_object(replacement, 64, 4);

    slub_free(c);
    slub_free(a);
    check_object(replacement, 64, 4);
    assert(nr_free_pages() == allocated);
    slub_free(replacement);
    assert(nr_free_pages() == baseline);
    cprintf("slub_check: isolation and slot reuse passed\n");
}

static void
check_multiple_pages(void) {// 满了重新分配一个slab
    size_t baseline = nr_free_pages();
    size_t first_other = SLUB_TEST_COUNT; // 300
    uintptr_t first_page = 0;

    /*
     * 300 live 16-byte objects exceed even a headerless 4 KiB page.
     * Detect the page transition without copying private slab metadata.
     */
    for (size_t i = 0; i < SLUB_TEST_COUNT; i++) {
        objects[i] = slub_alloc(16);
        assert(objects[i] != NULL);
        assert(((uintptr_t)objects[i] & 15) == 0);
        if (i == 0) {
            first_page = object_page(objects[i]);
        } else if (object_page(objects[i]) != first_page &&
                   first_other == SLUB_TEST_COUNT) {
            first_other = i;
        }
        for (size_t j = 0; j < i; j++) {
            assert(objects[i] != objects[j]);
        }
        fill_object(objects[i], 16, i);
        for (size_t j = 0; j <= i; j++) {
            check_object(objects[j], 16, j);
        }
    }
    assert(first_other > 0 && first_other < SLUB_TEST_COUNT);
    size_t allocated = nr_free_pages();
    assert(allocated < baseline && baseline - allocated >= 2);// 至少减少两页

    /* Free the first slab while objects in later slabs remain live. */
    for (size_t i = 0; i < SLUB_TEST_COUNT; i++) {
        if (object_page(objects[i]) == first_page) {
            check_object(objects[i], 16, i);
            slub_free(objects[i]);
            objects[i] = NULL;
        }
    }
    assert(nr_free_pages() == allocated + 1);
    for (size_t i = 0; i < SLUB_TEST_COUNT; i++) {
        if (objects[i] != NULL) {
            check_object(objects[i], 16, i);
            slub_free(objects[i]);
            objects[i] = NULL;
        }
    }
    assert(nr_free_pages() == baseline);
    cprintf("slub_check: page exhaustion and page reclamation passed\n");
}

/*
 * Run after pmm_init, with no pre-existing live SLUB objects and no concurrent
 * allocator users. This test intentionally requires several available pages.
 * Invalid pointer frees are excluded because their contract is to panic.
 */
void
slub_check(void) {  // 最主要的函数
    size_t baseline = nr_free_pages();// nr_free_pages()，它返回PMM当前还有多少个空闲物理页
    slub_init();
    assert(nr_free_pages() == baseline);

    assert(slub_alloc(0) == NULL);
    assert(slub_alloc(2049) == NULL);
    assert(slub_alloc(PGSIZE) == NULL);
    assert(slub_alloc((size_t)-1) == NULL);
    slub_free(NULL);
    assert(nr_free_pages() == baseline);

    check_sizes();
    check_reuse();
    check_multiple_pages();
    assert(nr_free_pages() == baseline);
    cprintf("slub_check() succeeded!\n");
}
