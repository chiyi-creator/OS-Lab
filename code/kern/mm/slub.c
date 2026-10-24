#include <slub.h>
#include <assert.h>
#include <list.h>
#include <pmm.h>
#include <string.h>

#define SLUB_ALIGNMENT 16
#define SLUB_CACHE_COUNT 8
#define SLUB_BITMAP_BYTES (PGSIZE / SLUB_ALIGNMENT / 8)  // 32

struct slub_cache {  // 它表示一个负责特定对象大小的分配缓存。一个cache可以管理多个slab，而每个slab里存放相同大小的对象槽。我们实现管理8个大小类别的。
    size_t object_size; // 管理的对象槽大小
    list_entry_t slabs;
};
/* list_entry_t，它是双向链表节点
struct list_entry {
    struct list_entry *prev, *next;
};
这里的 slabs 是链表头节点
*/

/* This header lives in the allocated page, independently of struct Page. */
// slub_page 这个管理头部，就放在SLUB申请到的4 KiB物理页的开头。
struct slub_page { // 管理某一个具体 slab 中的对象分配情况，回答：我属于哪个 cache？我对应哪个物理页？这一页能放多少对象？现在用了多少？哪些槽已经被占用？
    list_entry_t link;  // 把当前 slab 加入 cache 链表
    struct slub_cache *cache;
    struct Page *page;
    size_t capacity; // 最多可以分配多少个对象槽
    size_t inuse;
    uint8_t bitmap[SLUB_BITMAP_BYTES];  // 记录哪些对象槽被占用 32个字节，256位，因为最多256个对象槽。
};

static struct slub_cache caches[SLUB_CACHE_COUNT];  // 表示提前准备8个cache管理结构。
static bool initialized; // 用于记录SLUB是否已经初始化，初始值为 false

static size_t
slub_object_offset(void) {
    return ROUNDUP(sizeof(struct slub_page), SLUB_ALIGNMENT);
}// 计算管理头部自身占多少字节,将这个大小向上取整到16的倍数,后面才放对象槽

void
slub_init(void) {
    if (initialized) {
        return;
    }
    assert(slub_object_offset() + 2048 <= PGSIZE);
    for (size_t i = 0; i < SLUB_CACHE_COUNT; i++) {
        caches[i].object_size = (size_t)SLUB_ALIGNMENT << i;
        list_init(&caches[i].slabs); // 此时链表是空的，表示这个 cache 暂时没有管理任何 slab
    }
    initialized = 1;
}

static struct slub_page *
slub_new_page(struct slub_cache *cache) {
    struct Page *page = alloc_pages(1);// 申请一个页，返回页描述符地址
    if (page == NULL) {
        return NULL;
    }

    /*
     * entry.S maps PA [0x80000000, 0xc0000000) at
     * VA [0xffffffffc0000000, 0xffffffffffffffff].
     * Use the fixed boot offset: va_pa_offset is not initialized in the
     * current PMM skeleton. Restrict allocations to the declared RAM range.
     */
    uintptr_t pa = page2pa(page);  // 被分配物理页的起始物理地址
    if (pa < KERNEL_BEGIN_PADDR ||
        pa >= PHYSICAL_MEMORY_END ||
        (pa & (PGSIZE - 1)) != 0) {
        free_pages(page, 1);        // 与0xfff，4KB对齐
        return NULL;
    }
    struct slub_page *slab =
        (struct slub_page *)(pa + (uintptr_t)PHYSICAL_MEMORY_OFFSET); // 把这个得到的物理地址转换为虚拟地址，然后slub_page从这里开始
    memset(slab, 0, sizeof(*slab));  // 把管理头部清零
    slab->cache = cache;
    slab->page = page;
    slab->capacity = (PGSIZE - slub_object_offset()) / cache->object_size;
    list_add(&cache->slabs, &slab->link); // 把新slab加入当前cache的链表，方便后续分配器查找
    return slab;
}

void *
slub_alloc(size_t size) {
    if (!initialized || size == 0 ||
        size > caches[SLUB_CACHE_COUNT - 1].object_size) {
        return NULL;
    }

    struct slub_cache *cache = &caches[0];
    while (cache->object_size < size) {
        cache++;
    }// 从最小的逐个往上找

    struct slub_page *slab = NULL;
    list_entry_t *le;
    for (le = list_next(&cache->slabs); le != &cache->slabs; // 从链表头第一个节点开始遍历，直到回到链表头
         le = list_next(le)) {
        struct slub_page *candidate = to_struct(le, struct slub_page, link);  // 通过链表节点，找回它所属的 slub_page 结构体 
        if (candidate->inuse < candidate->capacity) {
            slab = candidate;
            break;
        }
    }
    if (slab == NULL) {
        slab = slub_new_page(cache);
        if (slab == NULL) {
            return NULL;
        }
    }

    for (size_t i = 0; i < slab->capacity; i++) { // i表示对象槽的编号 一个 bitmap 字节可以记录8个槽
        uint8_t mask = (uint8_t)(1U << (i % 8)); // bitmap字节索引 i/8,字节内部位编号 i%8
        if ((slab->bitmap[i / 8] & mask) == 0) {
            slab->bitmap[i / 8] |= mask;
            slab->inuse++;
            return (char *)slab + slub_object_offset() +
                   i * cache->object_size; // 计算并返回对象槽的虚拟地址
        }
    }
    panic("slub_alloc: inconsistent slab bitmap");
    return NULL;
}

void
slub_free(void *ptr) {
    if (ptr == NULL) {
        return;
    }
    if (!initialized) {// SLUB尚未初始化，却试图释放一个非空指针。这属于错误使用，因此触发 panic。
        panic("slub_free: allocator not initialized");
        return;
    }

    uintptr_t address = (uintptr_t)ptr;
    uintptr_t base = address & ~((uintptr_t)PGSIZE - 1); // 掩码与运算，得到页对齐的基地址，即该对象所在的页的起始地址（虚拟
    struct slub_page *slab = NULL;

    /*
     * Compare against owned pages before dereferencing anything derived
     * from ptr. Even an unmapped or already returned page is safe to reject.
     */
    for (size_t i = 0; i < SLUB_CACHE_COUNT && slab == NULL; i++) {// 遍历所有cache，确认页面属于SLUB
        list_entry_t *head = &caches[i].slabs;
        for (list_entry_t *le = list_next(head); le != head; 
             le = list_next(le)) {// 内层循环遍历每个cache管理的slab链表
            struct slub_page *candidate =
                to_struct(le, struct slub_page, link);
            if ((uintptr_t)candidate == base) {// 检查它的起始地址是否等于刚才计算出的页面起始地址，相等说明找到
                slab = candidate;
                break;
            }
        }
    }
    if (slab == NULL) {
        panic("slub_free: pointer does not belong to a live slab");
        return;
    }

    size_t offset = address - base;
    size_t start = slub_object_offset();
    size_t object_size = slab->cache->object_size;
    if (offset < start || (offset - start) % object_size != 0) {
        panic("slub_free: pointer is not an object start");
        return;
    }
    size_t index = (offset - start) / object_size; // 第几个
    if (index >= slab->capacity) {
        panic("slub_free: pointer outside object area");
        return;
    }
    uint8_t mask = (uint8_t)(1U << (index % 8)); // 是不是已经释放过
    if ((slab->bitmap[index / 8] & mask) == 0) {
        panic("slub_free: object already free");
        return;
    }
    assert(slab->inuse > 0);
    slab->bitmap[index / 8] &= (uint8_t)~mask;
    slab->inuse--;

    if (slab->inuse == 0) {
        struct Page *page = slab->page;
        list_del(&slab->link);
        free_pages(page, 1); // 先保存Page指针、移除cache链表，再释放页面
        /* The page, including its header, must not be touched again. */
    }
}
