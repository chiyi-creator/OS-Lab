#ifndef __KERN_MM_SLUB_H__
#define __KERN_MM_SLUB_H__

#include <defs.h>
// 这个文件声明了四个函数
/*
 * Single-core, non-reentrant allocator for the current boot-time direct map.
 * Call slub_init after pmm_init. Repeated initialization is a no-op.
 */
void slub_init(void);
// 初始化SLUB分配器。它在PMM初始化后调用。没有完整的并发保护。重复初始化是无操作的。

/*
 * Allocate 1..2048 bytes, aligned to 16 bytes. Contents are not initialized.
 * Return NULL for unsupported sizes, before initialization, or on page shortage.
 * Successive allocations need not have consecutive addresses.
 */
void *slub_alloc(size_t size);
// 传入希望申请的字节数，申请小对象。规则：1.允许申请1～2048字节。2.申请的内存地址是16字节对齐的。3.返回NULL表示不支持的大小、在初始化之前或页面不足。4.连续的分配不需要有连续的地址。

/*
 * NULL is a no-op. Other pointers must be live allocation start addresses.
 * Invalid pointers and detectable double frees cause a kernel panic.
 * A slab is returned to the PMM immediately when its last object is freed.
 */
void slub_free(void *ptr);
// 释放小对象。规则：1.传入NULL是无操作。2.其他指针必须是有效的分配起始地址。3.无效指针和可检测的重复释放会导致内核恐慌panic。4.当最后一个对象被释放时，slab会立即返回给PMM。
// panic 表示内核检测到严重错误，无法安全地继续正常执行，于是输出错误信息并停止正常运行。情况：1.SLUB尚未初始化就释放非空指针。2.释放的指针不属于任何活动的slab。3.释放的指针属于活动slab，但不是对象的起始地址。4.释放的指针属于活动slab，但该对象已经被释放过了。
/* Run after PMM initialization, with no live SLUB objects or concurrent users. */
void slub_check(void);
// 在PMM初始化后运行，没有活动的SLUB对象或并发用户。

#endif /* !__KERN_MM_SLUB_H__ */
