#ifndef __KERN_MM_BUDDY_PMM_H__
#define __KERN_MM_BUDDY_PMM_H__

#include <pmm.h>

/* Whole-block interface: free with the original allocation's base and n.
 * A request of n pages owns 2^ceil(log2(n)) pages, including the padding.
 */
extern const struct pmm_manager buddy_pmm_manager;

#endif
