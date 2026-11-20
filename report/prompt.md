# Lab4 提示词记录
# Lab4 prompt 成员1：张懿鸾 2413669

# 扫描lab4分支的lab2与lab3代码缺口

> 我们现在在lab4分支上。请在不修改任何文件的情况下扫描一下整个仓库，看看需要补充lab2/kab3代码的地方有哪些

<details><summary>5 previous messages</summary>

> 我会以当前 `lab4` 分支为准做只读扫描：先确认分支、目录结构和各 lab 的代码差异，再定位缺失的 `lab2`/`lab3` 接口、实现或构建接入点；不会修改任何文件。
>
> 当前仓库的 `lab4` 是一次独立导入，与 `lab2`/`lab3` 没有直接提交历史。下一步我会以两个历史分支的完整代码作为参照，逐文件比对实现和构建接入，而不是只看 `LAB4` 标记。
>
> 已确认 `lab4` 中有三组显式占位：`pmm.c` 的 Lab2 内存映射入口、`trap.c`/`trapentry.S` 的 Lab3 异常与上下文保存/恢复；此外 Lab2 的分配器文件整体没有带入。我要继续核对调用链和头文件声明，区分“Lab4 必须恢复”与“旧实验功能但 Lab4 不依赖”的部分。
>
> 从调用关系看，缺失不止占位注释本身：`default_pmm.c` 找不到 `best_fit_pmm.h`，且没有定义 `default_pmm_manager`；`pmm.c` 调用了未定义的 `page_init`，Lab4 测试还会调用 `page_insert/page_remove`。我会在 `/tmp` 的副本里做一次只读构建诊断，仓库本身保持零改动。
>
> 扫描结论已经比较清晰：需要移植的是“函数级”代码，不能整文件覆盖。`lab4` 的 Trap 入口和内存布局与历史分支已有差异，尤其是 `SAVE_ALL`、VMM 和中断保护逻辑；此外 Lab4 自身还有独立待实现项，不能算作简单的前置移植。
</details>

我已经按当前 `lab4` 对 72 个受版本控制文件做了只读扫描，并在 `/tmp` 副本中尝试构建。仓库本身没有修改，`git status` 仍为干净的 `lab4...origin/lab4`。

## 必须补回的 Lab2 代码

| 位置 | 当前问题 | 可复用来源/注意事项 |
|---|---|---|
| [pmm.c](./code/kern/mm/pmm.c:90) | `page_init()` 被清空，但第 149 行仍在调用 | 可从 `lab2`/`lab3` 的 `aca2025` 提交恢复。当前没有 `PHYSICAL_MEMORY_END`，需适配为 `KERNTOP - PHYSICAL_MEMORY_OFFSET` |
| [default_pmm.c](./code/kern/mm/default_pmm.c:6) | 分配器主体和 `default_pmm_manager` 被删除；同时只剩 `best_fit_check()`，其中的 `free_list`、`nr_free` 也未定义 | 可从 `lab3` 恢复 First-Fit 的 `default_init`、`default_init_memmap`、`default_alloc_pages`、`default_free_pages`、`default_nr_free_pages` 和管理器对象 |
| [default_pmm.c](./code/kern/mm/default_pmm.c:3) | 引用了仓库中不存在的 `best_fit_pmm.h`，这是当前第一处实际编译错误 | 如果恢复 First-Fit，应移除这个残留引用；如果坚持 Best-Fit，则必须补回对应头文件 |
| `code/kern/mm/best_fit_pmm.c`、`best_fit_pmm.h` | 整个文件在 `lab4` 中不存在 | `lab2`/`lab3` 中有完整实现；只有需要保留 Best-Fit 时才需要移植，并同步修改 [pmm.c](./code/kern/mm/pmm.c:38) 的 manager 选择 |

这里建议采用“恢复 Lab2 First-Fit 到现有 `default_pmm.c`”的最小方案，因为当前 [pmm.c](./code/kern/mm/pmm.c:40) 已经引用 `default_pmm_manager`。`lab2` 的 Best-Fit 单独位于 `best_fit_pmm.c`，不能直接整文件覆盖当前的 `default_pmm.c`。

## 必须补回的 Lab3 代码

| 位置 | 当前问题 | 可复用来源/注意事项 |
|---|---|---|
| [trapentry.S](./code/kern/trap/trapentry.S:55) | `RESTORE_ALL` 为空，没有恢复 `sepc`、`sstatus`、通用寄存器和最后的 `sp` | 可参考 `lab3` 的 `d3ec3a0`，但只移植该宏，不能整文件覆盖 |
| [trapentry.S](./code/kern/trap/trapentry.S:59) | `__alltraps` 没有执行 `mv a0, sp` 和 `jal trap`，因此永远不会进入 C 层 `trap()` | 同上；保留当前新版 `SAVE_ALL` 和 `forkrets` |
| [trap.c](./code/kern/trap/trap.c:84) | `interrupt_handler()` 被清空；时钟重设、ticks 更新、周期输出和关机逻辑缺失 | 可从 `lab3` 的 `365d063` 移植函数，当前 `IRQ_*`、`ticks`、SBI 接口都已具备 |
| [trap.c](./code/kern/trap/trap.c:86) | `exception_handler()` 被清空；断点、非法指令及 EPC 步进处理缺失 | 可从 `lab3` 的 `365d063` 移植 |
| [trap.c](./code/kern/trap/trap.c:102) | 已调用两个 handler，但没有对应实现或声明 | 当前编译只会产生隐式声明警告，最终链接会缺失符号 |
| [trap.h](./code/kern/trap/trap.h:51) | 没有声明 `interrupt_handler()`、`exception_handler()`，`trap_in_kernel()` 也被删除 | 建议补回接口声明，但应按 `lab4` 当前 TrapFrame 布局适配 |

不要直接覆盖整个 [trap.c](./code/kern/trap/trap.c:26) 或 [trapentry.S](./code/kern/trap/trapentry.S:1)：`lab4` 的 `SAVE_ALL`、`idt_init()`、VMM 引用和 `forkrets` 已与历史分支不同，整文件覆盖会丢掉这些内容。

## Lab4 自身仍缺失的代码

这些不属于 Lab2/Lab3 的直接移植范围，但不补也无法运行：

- [pmm.c](./code/kern/mm/pmm.c:223)：缺 `page_insert()`、`page_remove()`、`pgdir_alloc_page()`、`print_pgdir()`。其中 `page_insert/page_remove` 在 `pmm_init()` 自检里已经会被调用。
- [proc.c](./code/kern/process/proc.c:221)：缺 `alloc_proc()`、`copy_thread()`、`do_fork()`、`kernel_thread()`、`proc_init()` 等。
- [proc.c](./code/kern/process/proc.c:224)：还缺 `proc_run()`、`cpu_idle()` 等进程运行部分。
- [sched.c](./code/kern/schedule/sched.c:13)：缺 `schedule()`。
- [switch.S](./code/kern/process/switch.S:1)：缺 `switch_to()` 上下文切换。

临时构建首先停在：

```text
kern/mm/default_pmm.c:3:10: fatal error: best_fit_pmm.h: No such file or directory
```

继续静态检查可见未解析符号包括：

```text
default_pmm_manager
page_init
page_insert
page_remove
interrupt_handler
exception_handler
proc_init
```

因此建议顺序是：先恢复 `page_init` 和 PMM，再补 Trap 入口及两个 handler，然后处理 Lab4 自己的页表映射、进程和调度代码。

另外，[CMakeLists.txt](./code/CMakeLists.txt:33) 引用了若干不存在的文件，并且没有加入启动、Trap 和上下文切换汇编，目前不能作为可用构建入口；实际应继续使用 `Makefile`。

> 现在开始操作系统 Lab4 的第一阶段代码恢复：仅恢复 Lab2 的物理内存管理功能。
>
> 请严格遵循以下要求。本次不恢复 Lab3，不实现任何 Lab4 新功能。
>
> ### 一、当前环境
>
> - 当前 Git 分支：lab4
> - 当前代码目录：code/
> - 当前 Lab4 原始代码已经提交并推送。
> - Lab4 的代码基础必须保留。
> - 本次仅允许修改：
>   1. code/kern/mm/pmm.c
>   2. code/kern/mm/default_pmm.c
>
> ### 二、首先确认历史代码来源
>
> 执行只读检查：
>
> 1. git status，确认当前工作区状态。
> 2. git fetch origin，更新远程跟踪分支。
> 3. 比较本地 lab2 与 origin/lab2。
> 4. 确认此前最终完成的 Lab2 代码版本，特别注意 Buddy 和 MMU Probe 已经从最终实验实现中移除的约定。
> 5. 确认 Lab2 中 Best-Fit 算法的实际实现文件和提交 SHA。
> 6. 参考 Lab3 的最终代码，确认 page_init() 已经适配的内存范围计算方式。
>
> 不要切换分支，不要执行 merge、rebase、cherry-pick、reset，不要修改其他分支。
>
> ### 三、恢复 pmm.c 中的 page_init()
>
> 当前 Lab4 的 pmm.c 已经存在完整的框架，但 page_init() 缺失。
>
> 请从 Lab3 最终版本的 pmm.c 中提取已经适配过的 page_init() 函数，恢复到当前 Lab4 的对应位置。
>
> 要求：
>
> 1. 保留 get_memory_base() 和 get_memory_size() 获取物理内存范围的逻辑。
> 2. 正确设置 va_pa_offset。
> 3. 正确计算 npage。
> 4. 根据内核结束地址 end 分配 Page 描述符数组。
> 5. 初始化所有 Page 描述符。
> 6. 正确设置 PageReserved。
> 7. 正确计算可用物理内存起始地址和结束地址。
> 8. 最终通过 init_memmap() 将可用物理页交给 PMM 管理。
>
> 特别注意：
>
> Lab4 的 memlayout.h 没有 PHYSICAL_MEMORY_END。
>
> 应采用已经在 Lab3 中验证过的兼容计算：
>
> uint64_t mem_limit = KERNTOP - PHYSICAL_MEMORY_OFFSET;
>
> 不要直接使用旧 Lab2 中的 PHYSICAL_MEMORY_END。
>
> Lab4 中 va_pa_offset 的声明为 uint_t，不要为了照搬旧代码而随意改变其类型。
>
> 只恢复 page_init()，不要用旧版 pmm.c 覆盖整个文件。
>
> 必须完整保留：
>
> - Lab4 的 pmm_init()
> - enable_paging()
> - boot_map_segment()
> - boot_alloc_page()
> - get_pte()
> - get_page()
> - check_pgdir()
> - check_boot_pgdir()
>
> 尤其不要修改 page_insert()、page_remove() 等 Lab4 待实现功能的位置。
>
> ### 四、恢复 default_pmm.c 中的 Best-Fit 算法
>
> 这是本次最重要的部分。
>
> Lab2 最终实现中包含 First-Fit 和 Best-Fit 两种算法，但 Lab3 最终实际使用的是 Best-Fit。
>
> 当前 Lab4 的 default_pmm.c 已经保留了 basic_check() 和 best_fit_check() 两个测试函数。
>
> 其中 best_fit_check() 有一项明确的 Best-Fit 行为测试：
>
> free_pages(p0 + 1, 2);\
> free_pages(p0 + 4, 1);\
> assert(alloc_pages(4) == NULL);\
> assert((p1 = alloc_pages(1)) != NULL);\
> assert(p0 + 4 == p1);
>
> 这要求长度为 1 的空闲块优先于长度为 2 的空闲块被分配。
>
> 因此：
>
> - 不要将 First-Fit 算法作为本次恢复后的实际分配算法。
> - 不要删除或修改 basic_check()。
> - 不要删除或修改 best_fit_check()。
> - 不要将 Best-Fit 测试替换成 default_check()。
>
> 请从此前 Lab2 的 best_fit_pmm.c 中提取以下实现：
>
> - free_area 管理结构
> - free_list 和 nr_free
> - best_init()
> - best_init_memmap()
> - best_alloc_pages()
> - best_free_pages()
> - best_nr_free_pages()
>
> 将它们适配到当前 Lab4 的 default_pmm.c 中。
>
> 由于 Lab4 的 pmm.c 当前使用：
>
> pmm_manager = &default_pmm_manager;
>
> 因此需要将 Best-Fit 实现适配为 default_pmm_manager 接口。
>
> 要求：
>
> - 导出的管理器符号为 default_pmm_manager。
> - 管理器的 alloc_pages/free_pages/nr_free_pages 指向 Best-Fit 实现。
> - 管理器的 check 指向现有 best_fit_check()。
> - 管理器名称可以保留 best_fit_pmm_manager，以体现实际算法。
> - 保留 Lab4 原有的两个测试函数及其完整内容。
> - 清理当前不存在的 best_fit_pmm.h 头文件引用，改为使用当前实际存在的接口。
>
> 注意：不要直接用 Lab2 的 default_pmm.c 整文件覆盖 Lab4 文件。Lab2 的 default_pmm.c 实现的是 First-Fit，而当前 Lab4 的测试明确要求 Best-Fit。
>
> ### 五、本阶段明确不做的事情
>
> 不要恢复 Trap 相关代码。
>
> 不要修改：
>
> - trap.c
> - trapentry.S
> - trap.h
> - clock.c
> - intr.c
>
> 不要实现：
>
> - page_insert()
> - page_remove()
> - pgdir_alloc_page()
> - print_pgdir()
> - alloc_proc()
> - do_fork()
> - kernel_thread()
> - proc_init()
> - schedule()
> - switch_to()
>
> 这些分别属于 Lab3 或 Lab4 后续阶段。
>
> ### 六、完成后检查
>
> 请执行：
>
> 1. git diff --stat
> 2. git diff --check
> 3. git status --short --branch
>
> 并检查：
>
> - page_init() 是否完整。
> - PHYSICAL_MEMORY_END 是否仍有不正确引用。
> - default_pmm_manager 是否正确导出。
> - Best-Fit 算法是否真正参与 alloc_pages()。
> - best_fit_check() 是否保持原样。
> - 是否存在重复定义或缺少声明。
> - 是否误修改了 Lab4 的其他代码。
>
> 请尝试对 pmm.c 和 default_pmm.c 进行单文件编译检查。
>
> 由于后续 Lab4 功能尚未实现，完整内核链接失败时，不要擅自补齐 Lab4 的 TODO。
>
> 最后向我输出：
>
> 1. 修改文件清单。
> 2. 采用的历史代码提交 SHA。
> 3. 具体恢复的函数。
> 4. 与 Lab4 原始代码相比的接口适配。
> 5. 编译检查结果。
> 6. 完整 diff。
>
> 不要 commit，不要 push。等待人工审查。
>
> 在代码补充结束后，检查一下整个仓库，是否有代码冲突的地方

<details><summary>7 previous messages</summary>

> 我会先按你指定的来源核对 `lab2`、`origin/lab2` 和 `lab3` 的最终提交与文件内容，确认 Best-Fit、Buddy/MMU Probe 清理状态以及 `page_init()` 的兼容版本；随后只修改 `pmm.c` 和 `default_pmm.c`，最后做单文件编译和全仓库冲突扫描，不链接、不补后阶段 TODO。
>
> `git fetch origin` 已完成。接下来我会确认本地 `lab2` 与远程的差异、Best-Fit 的实际提交来源，以及最终 Lab2/Lab3 代码是否确实排除了 Buddy、MMU Probe 和 SLUB，再开始编辑。
>
> 来源已确认：Best-Fit 的核心实现来自 `539c8ce`，Lab3 恢复物理内存时使用 `aca2025` 的兼容版 `page_init()`。本地与远程 `lab2` 的源码一致，差异只在报告图片；Buddy/SLUB/MMU Probe 在 Lab3 最终源码中均已剔除，本次不会带入。现在开始只改两个指定文件。
>
> <details><summary>Edited 2 files</summary>
>
> - Wrote `./code/kern/mm/default_pmm.c` (+139 -2)
> - Wrote `./code/kern/mm/pmm.c` (+110 -1)
>
> </details>
>
> 两个文件已按函数级方式恢复：`pmm.c` 只插入 Lab3 兼容版 `page_init()`；`default_pmm.c` 加入 Best-Fit 管理实现，并让 `default_pmm_manager` 指向现有 `best_fit_check()`，两个原有检查函数未改动。现在核对 diff、符号和单文件编译。
>
> 两个目标文件均已用 RISC-V 交叉编译器单独编译成功，`git diff --check` 也通过。下一步我会在 `/tmp` 副本中做一次全树增量构建和冲突扫描，确认恢复代码没有引入重复符号或额外编译错误，并明确哪些剩余未解析符号属于按要求保留的 Lab3/Lab4 后续项。
>
> 恢复代码本身已通过 `-Werror` 单文件编译，两个原有检查函数经字节级比对确认完全未变，`default_pmm_manager` 也已正确导出并被 `pmm.o` 引用。全树构建现在只剩按要求未实现的 Lab3/Lab4 符号；我再做最后一次重复符号、未解析符号和工作区检查后汇总完整 diff。
</details>

Lab4 第一阶段恢复完成。仅修改了 `pmm.c` 和 `default_pmm.c`，未恢复 Lab3、未实现任何 Lab4 后续功能，也未 commit 或 push。

## 1. 修改文件

- [pmm.c](./code/kern/mm/pmm.c:90)：`+110 / -1`
- [default_pmm.c](./code/kern/mm/default_pmm.c:1)：`+139 / -2`

`git status`：

```text
## lab4...origin/lab4
 M code/kern/mm/default_pmm.c
 M code/kern/mm/pmm.c
```

## 2. 历史代码来源

- Best-Fit 核心实现：`539c8cef179f41ef7224e3bd0580d4ac8eed0914`
- Lab3 兼容版 `page_init()`：`aca2025b3daf95615fc89d8f022fede5623cb204`
- 最终 Lab2 树：`d6a96f24099ec65156bb79ea6d5c0c7e9124844c`
- `origin/lab2`：`912cbec4d3779eded64fafe42ef1754c334dcd34`
- 最终 Lab3 树：`f2006d93595e656f009f06e546eb1ea65afa0197`

本地 `lab2` 与 `origin/lab2` 的 `code/` 内容一致，差异只有报告图片。Buddy、SLUB 和 MMU Probe 在 Lab3 最终源码中已经移除，本次没有导入。

## 3. 恢复的函数

`pmm.c`：

- `page_init()`

`default_pmm.c`：

- `free_area`
- `free_list`、`nr_free`
- `best_init()`
- `best_init_memmap()`
- `best_alloc_pages()`
- `best_free_pages()`
- `best_nr_free_pages()`
- `default_pmm_manager`

`basic_check()` 和 `best_fit_check()` 经字节级比对均保持原样。

## 4. 接口适配

- `page_init()` 使用 `KERNTOP - PHYSICAL_MEMORY_OFFSET`，没有引用不存在的 `PHYSICAL_MEMORY_END`。
- 保留 Lab4 的 `uint_t va_pa_offset` 类型。
- 保留 Lab4 的 `pmm_init()`、分页、页表检查以及 `page_insert/page_remove` 占位位置。
- 删除不存在的 `best_fit_pmm.h` 引用，改为 [default_pmm.h](./code/kern/mm/default_pmm.h:1)。
- 导出符号为 `default_pmm_manager`，名称保留为 `"best_fit_pmm_manager"`。
- `alloc_pages/free_pages/nr_free_pages` 均指向 Best-Fit 实现，`check` 指向 `best_fit_check()`。
- [pmm.c](./code/kern/mm/pmm.c:40) 现有的 `pmm_manager = &default_pmm_manager` 无需修改即可实际启用 Best-Fit。

## 5. 检查结果

单文件编译使用 RISC-V 交叉编译器和 `-Werror`：

```text
pmm.c: PASS
default_pmm.c: PASS
```

其他检查：

```text
git diff --check: PASS
重复强符号扫描: 无重复
PHYSICAL_MEMORY_END 引用: 无
best_fit_pmm.h 引用: 无
merge conflict 标记: 无
```

在 `/tmp` 副本中执行全树 `make -k`，所有指定源文件均编译成功。链接失败仅来自按要求未实现的后续功能：

```text
cpu_idle
proc_init
page_insert
page_remove
```

此外，Lab3 的 `interrupt_handler()` 和 `exception_handler()` 仍为空，本次按要求未处理。

全仓库扫描还发现以下既有问题，均未修改：

- `trap.c` 目前会对两个 handler 产生隐式声明警告，属于后续 Lab3 工作。
- `kmalloc.c` 存在 `kva2page()` 整数转指针警告，不是本次恢复引入。
- `CMakeLists.txt` 引用了不存在的 IDE、swap、rand 等文件，也未列汇编入口；当前正式构建仍应使用 `Makefile`。

## 6. 完整 diff

```diff
diff --git a/code/kern/mm/default_pmm.c b/code/kern/mm/default_pmm.c
index 1a2bcda..e188ec6 100644
--- a/code/kern/mm/default_pmm.c
+++ b/code/kern/mm/default_pmm.c
@@ -1,9 +1,136 @@
 #include <pmm.h>
 #include <list.h>
-#include <best_fit_pmm.h>
+#include <default_pmm.h>
 #include <stdio.h>

-//lab2 请补充你在lab2中的代码
+static free_area_t free_area;
+
+#define free_list (free_area.free_list)
+#define nr_free (free_area.nr_free)
+
+static void
+best_init(void) {
+    list_init(&free_list);
+    nr_free = 0;
+}
+
+static void
+best_init_memmap(struct Page *base, size_t n) {
+    assert(n > 0);
+    struct Page *p = base;
+    for (; p != base + n; p ++) {
+        assert(PageReserved(p));
+        p->flags = p->property = 0;
+        set_page_ref(p, 0);
+    }
+    base->property = n;
+    SetPageProperty(base);
+    nr_free += n;
+
+    if (list_empty(&free_list)) {
+        list_add(&free_list, &(base->page_link));
+    } else {
+        list_entry_t *le = &free_list;
+        while ((le = list_next(le)) != &free_list) {
+            struct Page *page = le2page(le, page_link);
+            if (base < page) {
+                list_add_before(le, &(base->page_link));
+                break;
+            } else if (list_next(le) == &free_list) {
+                list_add(le, &(base->page_link));
+                break;
+            }
+        }
+    }
+}
+
+static struct Page *
+best_alloc_pages(size_t n) {
+    assert(n > 0);
+    if (n > nr_free) {
+        return NULL;
+    }
+
+    struct Page *best = NULL;
+    list_entry_t *le = &free_list;
+    while ((le = list_next(le)) != &free_list) {
+        struct Page *page = le2page(le, page_link);
+        if (page->property >= n &&
+            (best == NULL || page->property < best->property)) {
+            best = page;
+        }
+    }
+
+    if (best != NULL) {
+        list_entry_t *prev = list_prev(&(best->page_link));
+        list_del(&(best->page_link));
+        if (best->property > n) {
+            struct Page *remaining = best + n;
+            remaining->property = best->property - n;
+            SetPageProperty(remaining);
+            list_add(prev, &(remaining->page_link));
+        }
+        nr_free -= n;
+        ClearPageProperty(best);
+    }
+    return best;
+}
+
+static void
+best_free_pages(struct Page *base, size_t n) {
+    assert(n > 0);
+    struct Page *p = base;
+    for (; p != base + n; p ++) {
+        assert(!PageReserved(p) && !PageProperty(p));
+        p->flags = p->property = 0;
+        set_page_ref(p, 0);
+    }
+    base->property = n;
+    SetPageProperty(base);
+    nr_free += n;
+
+    if (list_empty(&free_list)) {
+        list_add(&free_list, &(base->page_link));
+    } else {
+        list_entry_t *le = &free_list;
+        while ((le = list_next(le)) != &free_list) {
+            struct Page *page = le2page(le, page_link);
+            if (base < page) {
+                list_add_before(le, &(base->page_link));
+                break;
+            } else if (list_next(le) == &free_list) {
+                list_add(le, &(base->page_link));
+                break;
+            }
+        }
+    }
+
+    list_entry_t *le = list_prev(&(base->page_link));
+    if (le != &free_list) {
+        p = le2page(le, page_link);
+        if (p + p->property == base) {
+            p->property += base->property;
+            ClearPageProperty(base);
+            list_del(&(base->page_link));
+            base = p;
+        }
+    }
+
+    le = list_next(&(base->page_link));
+    if (le != &free_list) {
+        p = le2page(le, page_link);
+        if (base + base->property == p) {
+            base->property += p->property;
+            ClearPageProperty(p);
+            list_del(&(p->page_link));
+        }
+    }
+}
+
+static size_t
+best_nr_free_pages(void) {
+    return nr_free;
+}

 // Framework tests: students must not modify basic_check or best_fit_check.
 static void
@@ -137,3 +264,13 @@ best_fit_check(void) {
     #endif
 }

+const struct pmm_manager default_pmm_manager = {
+    .name = "best_fit_pmm_manager",
+    .init = best_init,
+    .init_memmap = best_init_memmap,
+    .alloc_pages = best_alloc_pages,
+    .free_pages = best_free_pages,
+    .nr_free_pages = best_nr_free_pages,
+    .check = best_fit_check,
+};
+
diff --git a/code/kern/mm/pmm.c b/code/kern/mm/pmm.c
index 9ccda7b..8bd4729 100644
--- a/code/kern/mm/pmm.c
+++ b/code/kern/mm/pmm.c
@@ -87,7 +87,116 @@ size_t nr_free_pages(void)
     return ret;
 }

-//lab2 请补充你在lab2中的代码
+/*
+ * Initialize the physical page descriptors and hand the usable physical
+ * memory after the kernel and descriptor array to the current PMM.
+ */
+static void
+page_init(void)
+{
+    extern char end[];
+
+    va_pa_offset = PHYSICAL_MEMORY_OFFSET;
+
+    uint64_t mem_base = get_memory_base();
+    uint64_t mem_size = get_memory_size();
+    if (mem_base < DRAM_BASE || mem_size == 0 ||
+        mem_base > (uint64_t)-1 - mem_size)
+    {
+        panic("page_init: invalid physical memory range");
+    }
+
+    uint64_t mem_end = mem_base + mem_size;
+    uint64_t mem_limit = KERNTOP - PHYSICAL_MEMORY_OFFSET;
+    if (mem_end > mem_limit)
+    {
+        panic("page_init: physical memory exceeds supported range");
+    }
+
+    /*
+     * Page descriptors cover every physical page number in [nbase, npage).
+     * The last partial page, if any, is not represented in this range.
+     */
+    npage = PPN(mem_end);
+    if (npage <= nbase)
+    {
+        panic("page_init: no manageable physical pages");
+    }
+
+    uintptr_t kernel_end_pa = PADDR((uintptr_t)end);
+    if (kernel_end_pa < mem_base || kernel_end_pa >= mem_end)
+    {
+        panic("page_init: kernel end is outside physical memory");
+    }
+
+    /*
+     * Place the descriptor array after the kernel image. Its first page must
+     * be aligned, while the array itself may end in the middle of a page.
+     */
+    uintptr_t pages_pa = ROUNDUP(kernel_end_pa, PGSIZE);
+    size_t descriptor_count = npage - nbase;
+    if (descriptor_count > (uint64_t)-1 / sizeof(struct Page))
+    {
+        panic("page_init: page descriptor array is too large");
+    }
+
+    size_t pages_bytes = descriptor_count * sizeof(struct Page);
+    if (pages_pa > (uint64_t)-1 - pages_bytes)
+    {
+        panic("page_init: page descriptor array address overflow");
+    }
+    uintptr_t pages_end_pa = pages_pa + pages_bytes;
+    if (pages_end_pa > mem_end)
+    {
+        panic("page_init: page descriptor array exceeds physical memory");
+    }
+
+    pages = (struct Page *)(pages_pa + va_pa_offset);
+
+    /*
+     * Reserved is the safe initial state. init_memmap() clears it only for
+     * the pages that are subsequently added to its free list.
+     */
+    for (size_t i = 0; i < descriptor_count; i++)
+    {
+        struct Page *page = &pages[i];
+        page->ref = 0;
+        page->flags = 0;
+        page->property = 0;
+        list_init(&page->page_link);
+        SetPageReserved(page);
+    }
+
+    /*
+     * Only complete pages in the DTB range are released. Rounding the start
+     * up and the end down prevents partial pages from being made allocatable.
+     */
+    uintptr_t free_start_pa = ROUNDUP(pages_end_pa, PGSIZE);
+    uintptr_t dram_start_pa = ROUNDUP(mem_base, PGSIZE);
+    if (free_start_pa < dram_start_pa)
+    {
+        free_start_pa = dram_start_pa;
+    }
+    uintptr_t free_end_pa = ROUNDDOWN(mem_end, PGSIZE);
+    if (free_start_pa >= free_end_pa)
+    {
+        panic("page_init: no complete physical pages available");
+    }
+
+    size_t nr_pages = (free_end_pa - free_start_pa) / PGSIZE;
+    struct Page *free_base = pa2page(free_start_pa);
+    init_memmap(free_base, nr_pages);
+
+    cprintf("physical memory: [0x%016lx, 0x%016lx), %lu MB\n",
+            (unsigned long)mem_base, (unsigned long)mem_end,
+            (unsigned long)(mem_size / (1024 * 1024)));
+    cprintf("page array: [0x%016lx, 0x%016lx), %lu descriptors\n",
+            (unsigned long)pages_pa, (unsigned long)pages_end_pa,
+            (unsigned long)descriptor_count);
+    cprintf("free memory: [0x%016lx, 0x%016lx), %lu pages\n",
+            (unsigned long)free_start_pa, (unsigned long)free_end_pa,
+            (unsigned long)nr_pages);
+}

 static void enable_paging(void)
 {
```

当前工作区等待人工审查，没有提交或推送。

> 现在开始操作系统 Lab4 的第二阶段代码恢复：恢复此前 Lab3 已经完成的 RISC-V Trap 入口、上下文保存与恢复、中断和异常处理机制。
>
> 本次任务是历史代码迁移与兼容性整合，不是重新设计 Trap 机制，也不是提前实现 Lab4 的进程管理功能。
>
> 请严格遵循以下要求。
>
> # 一、当前项目状态
>
> 当前 Git 仓库：OS-Lab
>
> 当前分支：lab4
>
> 当前工作目录：
>
> \~/oslab/OS-Lab
>
> 当前代码目录：
>
> code/
>
> Lab4 原始代码已经提交，Lab2 物理内存管理恢复也已经完成并提交。
>
> 当前已经恢复的 Lab2 功能：
>
> - pmm.c 中的 page_init()
> - default_pmm.c 中的 Best-Fit 分配器
> - default_pmm_manager 接口
>
> 本次不要修改上述 Lab2 代码。
>
> # 二、Git 操作约束
>
> 首先检查：
>
> git status --short --branch
>
> git log -5 --oneline --decorate
>
> 确认当前分支仍然为 lab4，且工作区没有未提交修改。
>
> 然后执行：
>
> git fetch origin
>
> 检查本地 lab3 和 origin/lab3 的提交历史：
>
> git log --oneline --decorate --graph lab3 origin/lab3 -n 15
>
> 必须检查实际提交中的代码，不能仅凭提交信息猜测。
>
> 禁止：
>
> - 切换分支。
> - 修改 lab2、lab3、main。
> - merge、rebase、cherry-pick。
> - reset。
> - force push。
> - 自动 commit 或 push。
>
> # 三、第一项：恢复 trapentry.S
>
> 目标文件：
>
> code/kern/trap/trapentry.S
>
> 当前 Lab4 已经有自己的 SAVE_ALL 宏和 Trap 入口框架，但 RESTORE_ALL 为空，\_\_alltraps 也没有完整调用 C 层 trap()。
>
> 请首先完整阅读当前 Lab4 的 trapentry.S 和 trap.h。
>
> 然后对照 Lab3 最终版本，恢复缺失的代码。
>
> ## 1. RESTORE_ALL
>
> 恢复 Lab3 中已经完成的寄存器恢复机制。
>
> 要求：
>
> - 恢复 sepc。
> - 恢复 sstatus。
> - 恢复所有由 SAVE_ALL 保存的通用寄存器。
> - 恢复 ra、gp、tp 等对应寄存器。
> - 恢复所有临时寄存器、参数寄存器和保存寄存器。
> - 最后正确恢复 sp。
> - 确保恢复顺序不会提前破坏当前 TrapFrame 的栈地址。
>
> 特别重要：
>
> RESTORE_ALL 必须严格按照当前 Lab4 SAVE_ALL 实际保存的寄存器顺序和偏移进行恢复。
>
> 不要直接照搬旧 Lab3 的偏移，必须以当前 Lab4 的 TrapFrame 结构和 SAVE_ALL 为准。
>
> 请输出一个寄存器偏移对照表：
>
> | 寄存器/CSR | SAVE_ALL 保存偏移 | RESTORE_ALL 恢复偏移 | trap.h 对应字段 |
> | ------- | ------------- | ---------------- | ----------- |
>
> 确保所有对应关系一致。
>
> ## 2. \_\_alltraps
>
> 恢复 Lab3 中的 C 层 Trap 调用逻辑。
>
> 需要实现的关键流程：
>
> - 执行 SAVE_ALL。
> - 将当前 TrapFrame 的栈地址传递给 a0。
> - 调用 C 函数 trap()。
> - 在 trap() 返回后进入 Trap 返回流程。
> - 执行 RESTORE_ALL。
> - 使用 sret 返回被中断或异常的执行环境。
>
> 重点检查：
>
> mv a0, sp
>
> jal trap
>
> 这两个关键操作是否正确放置。
>
> 不要重复定义标签，也不要改变现有汇编文件的整体组织结构。
>
> ## 3. 必须保留 Lab4 的 forkrets
>
> Lab4 当前已经新增：
>
> forkrets:
> move sp, a0
> j \_\_trapret
>
> 这是 Lab4 进程首次运行时恢复上下文的重要入口。
>
> 必须完整保留。
>
> 同时检查 \_\_trapret 标签及其与 RESTORE_ALL 的关系，保证：
>
> - 正常 Trap 返回可以使用 \_\_trapret。
> - 新建进程首次运行也可以通过 forkrets 进入 \_\_trapret。
> - 两条路径最终都能正确恢复 TrapFrame 并执行 sret。
>
> 不允许直接用 Lab3 的整个 trapentry.S 覆盖 Lab4 文件。
>
> # 四、第二项：恢复 trap.c
>
> 目标文件：
>
> code/kern/trap/trap.c
>
> 当前 Lab4 已经保留：
>
> - idt_init()
> - print_trapframe()
> - print_regs()
> - trap()
> - 当前 Lab4 的 Trap 分发框架
> - 与 VMM 等模块相关的接口
>
> 这些内容必须保留。
>
> 当前主要缺失：
>
> - interrupt_handler()
> - exception_handler()
>
> 请从 Lab3 最终版本中恢复这两个函数，并根据 Lab4 当前接口进行适配。
>
> ## 1. interrupt_handler()
>
> 重点恢复：
>
> - 时钟中断处理。
> - clock_set_next_event() 的调用。
> - ticks 计数。
> - 周期性时钟输出。
> - 外部中断等其他 Lab3 已经实现的处理逻辑。
> - 未知中断原因的处理。
>
> 必须结合当前 Lab4 的：
>
> - IRQ\_\* 定义。
> - ticks 定义。
> - clock.c。
> - SBI 接口。
> - TrapFrame 结构。
>
> 确认历史代码中的变量和接口在 Lab4 中是否仍然存在。
>
> 不要凭空新增重复的 ticks 变量。
>
> 不要擅自改变时钟中断的处理周期。
>
> 如果 Lab3 中有关机或测试退出逻辑，请保留其原有语义，但需要检查是否与 Lab4 当前环境兼容。
>
> ## 2. exception_handler()
>
> 从 Lab3 最终代码中恢复异常处理逻辑。
>
> 请重点检查：
>
> - 断点异常。
> - 非法指令异常。
> - 系统调用异常（如果 Lab3 实际实现了）。
> - EPC 更新机制。
> - 其他 Lab3 已经实现的异常处理分支。
>
> 尤其注意：
>
> 断点异常或系统调用异常需要根据实际指令长度正确更新 sepc，不能一律随意增加 4。
>
> 请严格参考旧 Lab3 的实现和当前异常定义，不要自行增加未经实验代码支持的功能。
>
> ## 3. trap() 与 Handler 的调用关系
>
> 请检查当前 trap() 的分发逻辑，确认：
>
> - interrupt_handler() 在正确的中断路径中被调用。
> - exception_handler() 在正确的异常路径中被调用。
> - 中断和异常原因的识别方式正确。
> - 不会重复分发或遗漏异常。
> - TrapFrame 指针正确传递。
>
> 保留当前 Lab4 trap() 的整体结构，只补充缺失的功能。
>
> # 五、第三项：恢复 trap.h
>
> 目标文件：
>
> code/kern/trap/trap.h
>
> 请对照 Lab3 检查：
>
> - struct pushregs
> - struct trapframe
> - 必要的函数声明
> - trap_in_kernel() 等历史辅助函数（如果实际仍需要）
>
> 当前 Lab4 的 TrapFrame 是汇编保存与 C 语言处理之间的接口。
>
> 要求：
>
> 1. 不要直接用 Lab3 的 trap.h 覆盖 Lab4。
> 2. 保留 Lab4 已有的结构和字段。
> 3. 如果缺少 interrupt_handler()、exception_handler() 的声明，请补充。
> 4. 如果 trap_in_kernel() 在迁移后的代码中确实需要，请从历史代码恢复并检查其适用性。
> 5. 不要为了保持与旧 Lab3 完全一致而添加当前代码中不再需要的字段。
>
> 最终确保 C 语言结构体与汇编保存布局一致。
>
> # 六、第四项：只读检查 clock.c、intr.c 和 init.c
>
> 本次不要主动修改以下文件：
>
> - code/kern/driver/clock.c
> - code/kern/driver/clock.h
> - code/kern/driver/intr.c
> - code/kern/driver/intr.h
> - code/kern/init/init.c
>
> 但需要与 Lab3 最终版本进行对照。
>
> 检查：
>
> 1. clock.c 是否保留 rdtime、clock_init、clock_set_next_event 和 ticks 相关实现。
> 2. intr.c 是否保留 intr_enable、intr_disable、local_intr_save、local_intr_restore。
> 3. init.c 是否正确初始化 Trap 和中断。
> 4. Lab4 init.c 是否仍然保留 VMM、proc_init、clock_init、cpu_idle 等初始化流程。
>
> 如果发现缺失，请先报告差异和原因，不要直接修改这些文件。
>
> # 七、必须保留的 Lab4 功能
>
> 以下内容不能因为迁移 Lab3 而丢失：
>
> 1. trapentry.S 中的 SAVE_ALL。
> 2. trapentry.S 中的 forkrets。
> 3. trapentry.S 中的 \_\_trapret 入口结构。
> 4. trap.c 中现有的 idt_init()。
> 5. trap.c 中现有的 print_trapframe() 和 print_regs()。
> 6. trap.c 中现有的 trap()。
> 7. trap.c 中与 Lab4 VMM 相关的引用和接口。
> 8. trap.h 中 Lab4 当前的 TrapFrame 定义。
>
> 不要修改 Lab2 已经完成的：
>
> - pmm.c
> - default_pmm.c
>
> 不要提前实现 Lab4 的代码
>
> # 八、实际工作顺序
>
> 请按以下顺序工作。
>
> 第一步：只读分析历史代码。
>
> 输出：
>
> - Lab3 最终有效提交。
> - trapentry.S 历史版本。
> - trap.c 历史版本。
> - trap.h 历史版本。
> - 当前 Lab4 与 Lab3 的差异。
>
> 第二步：建立迁移对照表。
>
> | 文件 | Lab3 原有实现 | Lab4 当前状态 | 需要迁移的内容 | 需要保留的 Lab4 内容 |
> | -- | --------- | --------- | ------- | ------------- |
>
> 第三步：逐文件修改。
>
> 先修改 trapentry.S，再修改 trap.h，最后修改 trap.c。
>
> 每修改完一个文件，检查其 diff，避免多个文件同时出现问题后难以定位。
>
> 第四步：进行静态检查。
>
> 检查：
>
> - RESTORE_ALL 与 SAVE_ALL 的寄存器对应关系。
> - TrapFrame 结构体偏移。
> - \_\_alltraps 到 trap() 的调用过程。
> - trap() 到两个 Handler 的调用过程。
> - forkrets 是否完整保留。
> - \_\_trapret 是否正确。
> - 是否存在重复定义。
> - 是否存在缺失函数声明。
> - 是否存在错误的 CSR 偏移。
> - 是否存在 Lab4 原有代码丢失。
>
> 第五步：编译检查。
>
> 使用项目现有 Makefile 和 RISC-V 工具链。
>
> 至少单独编译：
>
> - trap.c
> - trapentry.S
>
> 然后尝试全树 make -k。
>
> 目前 Lab4 进程管理和页表操作尚未实现，因此全树链接仍可能因为以下符号失败：
>
> - cpu_idle
> - proc_init
> - page_insert
> - page_remove
>
> 这些失败属于尚未完成的 Lab4 工作，不要擅自补齐。
>
> 第六步：输出审查报告。
>
> 报告包含：
>
> 1. 实际修改的文件清单。
> 2. 每个文件恢复的函数。
> 3. 使用的 Lab3 历史提交 SHA。
> 4. Lab4 兼容性修改及其理由。
> 5. SAVE_ALL 与 RESTORE_ALL 的寄存器偏移对照表。
> 6. forkrets 保留情况。
> 7. trap() 与 Handler 的调用流程。
> 8. 单文件编译结果。
> 9. 全树构建结果。
> 10. git diff --stat。
> 11. git diff --check。
> 12. git status。
>
> 最后提供三个文件的完整 diff，供人工审查。
>
> 不要 commit，不要 push。
>
> 如果发现历史代码和 Lab4 当前结构存在无法确定的冲突，请先停止对应文件的修改并报告，不要自行猜测。
> 在代码补充结束后，检查一下整个仓库，是否有代码冲突的地方

<details><summary>12 previous messages</summary>

> 我会先确认当前工作区和 `lab4` 提交状态，尤其核对 Lab2 的两个文件是否已如你所述提交；随后更新远程引用并逐行对照 Lab3 的实际提交。只有确认历史差异后，我才按 `trapentry.S → trap.h → trap.c` 的顺序迁移，并在每一步检查 diff。
>
> 当前状态符合描述：`lab4` 比 `origin/lab4` 领先 1 个已提交提交，工作区干净，Lab2 恢复提交为 `49014dd`。现在执行 `git fetch origin`，随后读取 Lab3 最终文件内容而不是依赖提交标题。
>
> 只读对照已完成。Lab3 的最终有效代码集中在两个提交：`d3ec3a0`（Trap 入口恢复）和 `365d063`（中断/异常 handler）；最终 `lab3` 的 `f2006d9` 与 `origin/lab3` 源码一致，后者只多了报告提交。
>
> 迁移表如下：
>
> | 文件 | Lab3 原有实现 | Lab4 当前状态 | 需要迁移的内容 | 需要保留的 Lab4 内容 |
> |---|---|---|---|---|
> | `trapentry.S` | `RESTORE_ALL`、`mv a0, sp; jal trap` | 两处为空 | 按现行 36 字布局恢复 CSR/GPR，调用 `trap()` | 当前 `SAVE_ALL`、`.align 2`、`__trapret`、`forkrets` |
> | `trap.c` | `interrupt_handler()`、`exception_handler()` | handler 区域为空 | 时钟及其他中断、断点/非法指令处理 | `vmm.h`、`idt_init()` 的 `SSTATUS_SUM`、现有 `trap()` 分发和打印函数 |
> | `trap.h` | handler 与 `trap_in_kernel()` 声明 | 仅基础声明 | `interrupt_handler/exception_handler` | 当前 `pushregs/trapframe` 布局和现有声明 |
>
> 关键兼容点：Lab4 的 `SAVE_ALL` 使用相同的 36 个 64 位槽位，但 `sscratch` 保存方式和 `stval` CSR 读取方式不同于旧 Lab3；因此只迁移 `RESTORE_ALL` 和调用逻辑，不覆盖 `SAVE_ALL`。旧 Lab3 没有实现系统调用异常，本次也不会新增。`forkrets` 与 `__trapret` 保持现有结构。接下来先修改 `trapentry.S` 并单独核对。
>
> <details><summary>Edited a file</summary>
>
> - Wrote `./code/kern/trap/trapentry.S` (+41 -2)
>
> </details>
>
> `trapentry.S` 已恢复并通过汇编/反汇编检查：36 个槽位偏移与 `trapframe` 一致，`sp` 最后恢复，`__alltraps` 正确调用 `trap`，`forkrets → __trapret → sret` 保留。现在只给 `trap.h` 增加迁移后代码实际使用的两个 handler 声明；不恢复当前没有调用者的 `trap_in_kernel()`。
>
> <details><summary>Edited a file</summary>
>
> - Wrote `./code/kern/trap/trap.h` (+2 -0)
>
> </details>
>
> `trap.h` 的接口检查已通过。对 `trap.c` 做兼容核对时发现一处必须适配：Lab3 的 `sbi_shutdown()` 是外部函数，而 Lab4 当前放在 `<sbi.h>` 中作为 `static inline` 实现，因此迁移后会显式包含 `<sbi.h>` 并移除旧的 `extern` 声明。除此之外保留 Lab3 原有中断周期、10 次输出后关机和异常 EPC 步进逻辑。
>
> <details><summary>Edited a file</summary>
>
> - Wrote `./code/kern/trap/trap.c` (+91 -1)
>
> </details>
>
> 三个文件已完成代码迁移，单文件 `-Werror` 编译通过。现在进行结构化静态验证：用编译期断言确认 `pushregs/trapframe` 的 256/264/272/280 字节偏移，再用项目 Makefile 单独构建 `trap.c`、`trapentry.S`，最后在 `/tmp` 副本中执行全树 `make -k`，避免在仓库生成构建产物。
>
> 刚才的布局断言命令本身有测试脚本问题：`REGBYTES` 依赖目标架构宏，而仓库自定义的 `offsetof` 又不适合直接用于 `_Static_assert`。代码并没有编译失败；我会用显式 `-march/-mabi` 和 `__builtin_offsetof` 重跑，并确保测试失败时不再打印成功标记。
>
> 项目 Makefile 已成功单独构建 `trap.c` 与 `trapentry.S`。全树平铺编译也通过，链接仅因任务说明中预期的 `proc_init`、`cpu_idle`、`page_insert`、`page_remove` 未实现而停止。现在做最后的保留性检查和全仓库冲突扫描，确认没有覆盖 Lab4 框架或 Lab2 提交。
>
> 保留性检查确认：`SAVE_ALL`、`__trapret`、`forkrets` 以及 `trap.c` 的 `idt_init`、打印函数和 `trap()` 都与 Lab4 原有版本逐字一致；Trap 对象没有重复强符号，`trapentry.o` 唯一外部依赖是 `trap`。最后整理统计、全仓库既有冲突报告和三个文件的完整 diff。
</details>

Lab4 第二阶段 Trap 代码恢复完成。仅修改了三个 Trap 文件，未触碰 Lab2 物理内存代码，也未实现进程管理、页表操作或调度功能，未 commit、未 push。

## 1. 修改文件

- [trapentry.S](./code/kern/trap/trapentry.S:55)
- [trap.h](./code/kern/trap/trap.h:51)
- [trap.c](./code/kern/trap/trap.c:87)

统计：

```text
 code/kern/trap/trap.c      | 92 +++++++++++++++++++++++++++++++++++++++++++++-
 code/kern/trap/trap.h      |  2 +
 code/kern/trap/trapentry.S | 43 +++++++++++++++++++++-
 3 files changed, 134 insertions(+), 3 deletions(-)
```

## 2. 恢复内容

| 文件 | 恢复内容 |
|---|---|
| `trapentry.S` | `RESTORE_ALL`、`mv a0, sp`、`jal trap` |
| `trap.h` | `interrupt_handler()`、`exception_handler()` 声明 |
| `trap.c` | `interrupt_handler()`、`exception_handler()` |

以下 Lab4 内容保持不变，并通过内容比对确认：

- `SAVE_ALL`
- `__trapret`
- `forkrets`
- `idt_init()`
- `print_trapframe()`
- `print_regs()`
- `trap()` 原有分发结构
- `trap.h` 的 `pushregs`、`trapframe`
- Lab4 的 `vmm.h` 引用和 `SSTATUS_SUM` 设置

## 3. 历史提交

- Trap 入口恢复：`d3ec3a06d0af723eeae321c50bb68a94d40ebd6e`
- 中断和异常 handler：`365d063ccc0fe7b0e6ab4080044c46b115f91727`
- 本地最终 Lab3：`f2006d93595e656f009f06e546eb1ea65afa0197`
- 远程最终 Lab3：`8866198125bc26af3fec7826e52a5b33052d0be9`
- 已提交的 Lab2 恢复：`49014ddebde845818074f3b59cf29cc9e118ab83`

本地 `lab3` 与 `origin/lab3` 的 `code/` 内容一致，远程多两个报告提交。

## 4. Lab4 兼容性适配

- 不覆盖 Lab4 的 `SAVE_ALL`。当前版本使用 `csrr s0, sscratch` 和 `csrr s3, 0x143`，与旧 Lab3 不同。
- 仅恢复 `RESTORE_ALL` 和 C 层调用逻辑。
- 当前 Lab4 的 `sbi_shutdown()` 是 `<sbi.h>` 中的 `static inline` 实现，因此 `trap.c` 新增 `<sbi.h>`，不再使用 Lab3 的 `extern void sbi_shutdown(void)`。
- 时钟周期保持 `TICK_NUM = 100` 和 `SHUTDOWN_AFTER = 10`。
- 异常 EPC 根据指令低两位判断 2 字节或 4 字节步进。
- Lab3 没有系统调用异常实现，本次没有新增。
- `trap_in_kernel()` 当前没有调用者，两个迁移后的 handler 也不需要它，因此没有恢复该函数；现有 TrapFrame 仍保留 `status`，未来需要时可以基于 `SSTATUS_SPP` 实现。

## 5. SAVE_ALL / RESTORE_ALL 偏移

RV64 下 `REGBYTES = 8`，整个 TrapFrame 为 288 字节。

| 寄存器/CSR | SAVE_ALL 保存偏移 | RESTORE_ALL 恢复偏移 | trap.h 对应字段 |
|---|---:|---:|---|
| x0 / zero | 未保存，硬连线为 0 | 不恢复 | `gpr.zero` |
| x1 / ra | 8 | 8 | `gpr.ra` |
| x2 / sp | 16 | 16，最后恢复 | `gpr.sp` |
| x3 / gp | 24 | 24 | `gpr.gp` |
| x4 / tp | 32 | 32 | `gpr.tp` |
| x5 / t0 | 40 | 40 | `gpr.t0` |
| x6 / t1 | 48 | 48 | `gpr.t1` |
| x7 / t2 | 56 | 56 | `gpr.t2` |
| x8 / s0 | 64 | 64 | `gpr.s0` |
| x9 / s1 | 72 | 72 | `gpr.s1` |
| x10 / a0 | 80 | 80 | `gpr.a0` |
| x11 / a1 | 88 | 88 | `gpr.a1` |
| x12 / a2 | 96 | 96 | `gpr.a2` |
| x13 / a3 | 104 | 104 | `gpr.a3` |
| x14 / a4 | 112 | 112 | `gpr.a4` |
| x15 / a5 | 120 | 120 | `gpr.a5` |
| x16 / a6 | 128 | 128 | `gpr.a6` |
| x17 / a7 | 136 | 136 | `gpr.a7` |
| x18 / s2 | 144 | 144 | `gpr.s2` |
| x19 / s3 | 152 | 152 | `gpr.s3` |
| x20 / s4 | 160 | 160 | `gpr.s4` |
| x21 / s5 | 168 | 168 | `gpr.s5` |
| x22 / s6 | 176 | 176 | `gpr.s6` |
| x23 / s7 | 184 | 184 | `gpr.s7` |
| x24 / s8 | 192 | 192 | `gpr.s8` |
| x25 / s9 | 200 | 200 | `gpr.s9` |
| x26 / s10 | 208 | 208 | `gpr.s10` |
| x27 / s11 | 216 | 216 | `gpr.s11` |
| x28 / t3 | 224 | 224 | `gpr.t3` |
| x29 / t4 | 232 | 232 | `gpr.t4` |
| x30 / t5 | 240 | 240 | `gpr.t5` |
| x31 / t6 | 248 | 248 | `gpr.t6` |
| `sstatus` | 256 | 经 `a0` 写回，随后恢复 `a0` | `status` |
| `sepc` | 264 | 经 `a0` 写回，随后恢复 `a0` | `epc` |
| `stval` | 272 | 不直接恢复 | `badvaddr` |
| `scause` | 280 | 不直接恢复 | `cause` |

编译期断言已验证：

```text
sizeof(struct pushregs) == 256
sizeof(struct trapframe) == 288
status = 256, epc = 264, badvaddr = 272, cause = 280
TRAPFRAME_LAYOUT_OK
```

## 6. 入口与调用流程

正常 Trap：

```text
stvec
  → __alltraps
  → SAVE_ALL
  → a0 = sp
  → jal trap
  → trap(tf)
      → cause < 0 ? interrupt_handler(tf)
      → cause >= 0 ? exception_handler(tf)
  → 返回 __trapret
  → RESTORE_ALL
  → sret
```

新建进程首次运行：

```text
forkrets
  → sp = a0（TrapFrame 地址）
  → __trapret
  → RESTORE_ALL
  → sret
```

反汇编确认：

```text
mv a0, sp
jal trap
...
ld sp, 16(sp)
sret
...
mv sp, a0
j __trapret
```

## 7. 编译检查

使用项目 Makefile 单独构建：

```text
+ cc kern/trap/trap.c
+ cc kern/trap/trapentry.S
MAKEFILE_SINGLE_TARGETS_OK
```

两个文件也通过显式 `-Werror` 编译。

全树 `make -k` 已通过所有源文件编译，链接仅因预期中的 Lab4 后续功能缺失而停止：

```text
proc_init
cpu_idle
page_insert
page_remove
```

没有链接失败涉及本次恢复的 Trap 符号。

## 8. 只读检查结果

- `clock.c`：保留 `rdtime`、`clock_init()`、`clock_set_next_event()` 和全局 `ticks`。当前 `timebase = 1e7 / 100` 与 Lab3 的 `100000` 等价。
- `intr.c`：保留 `intr_enable()`、`intr_disable()`；`local_intr_save/restore` 位于 `sync.h`，接口完整。
- `init.c`：保留 `pmm_init()`、`idt_init()`、`vmm_init()`、`proc_init()`、`clock_init()`、`intr_enable()`、`cpu_idle()` 的 Lab4 流程。
- 上述文件均未修改。

## 9. 全仓库冲突扫描

```text
merge conflict 标记: 无
Trap 重复强符号: 无
trapentry.o 外部依赖: 仅 trap
Lab2 文件改动: 无
trap_in_kernel 调用者: 无
git diff --check: PASS
```

`trap.o` 中仅存在一套：

```text
interrupt_handler
exception_handler
trap
__alltraps
__trapret
forkrets
```

仓库仍有以下既有、非本次引入的问题：

- `kmalloc.c` 存在 `kva2page()` 整数转指针警告。
- `CMakeLists.txt` 引用了不存在的 IDE、swap、rand 等文件，也没有纳入汇编入口。
- `kdebug.c` 保留 `Not Implemented` 桩。
- Lab4 页表、进程和调度 TODO 仍按原位置保留。

## 10. 最终 Git 状态

```text
## lab4...origin/lab4 [ahead 1]
 M code/kern/trap/trap.c
 M code/kern/trap/trap.h
 M code/kern/trap/trapentry.S
```

`git diff --check` 没有输出，表示检查通过。

## 11. 完整 diff

```diff
diff --git a/code/kern/trap/trap.c b/code/kern/trap/trap.c
index 8d0d77b..a3d8353 100644
--- a/code/kern/trap/trap.c
+++ b/code/kern/trap/trap.c
@@ -6,6 +6,7 @@
 #include <memlayout.h>
 #include <mmu.h>
 #include <riscv.h>
+#include <sbi.h>
 #include <stdio.h>
 #include <trap.h>
 #include <vmm.h>
@@ -83,9 +84,98 @@ void print_regs(struct pushregs *gpr)

 extern struct mm_struct *check_mm_struct;

-//lab3 请补充你在lab3中的代码
+void interrupt_handler(struct trapframe *tf)
+{
+    enum
+    {
+        SHUTDOWN_AFTER = 10
+    };
+    static size_t tick_print_count = 0;
+
+    uintptr_t cause = tf->cause & ~((uintptr_t)1 << 63);
+
+    switch (cause)
+    {
+    case IRQ_U_SOFT:
+        cprintf("User software interrupt\n");
+        break;
+
+    case IRQ_S_SOFT:
+        cprintf("Supervisor software interrupt\n");
+        break;
+
+    case IRQ_M_SOFT:
+        cprintf("Machine software interrupt\n");
+        break;
+
+    case IRQ_U_TIMER:
+        cprintf("User timer interrupt\n");
+        break;
+
+    case IRQ_S_TIMER:
+        clock_set_next_event();
+        ticks++;
+
+        if (ticks % TICK_NUM == 0)
+        {
+            print_ticks();
+            tick_print_count++;
+
+            if (tick_print_count >= SHUTDOWN_AFTER)
+            {
+                sbi_shutdown();
+            }
+        }
+        break;
+
+    case IRQ_M_TIMER:
+        cprintf("Machine timer interrupt\n");
+        break;

+    case IRQ_U_EXT:
+        cprintf("User external interrupt\n");
+        break;

+    case IRQ_S_EXT:
+        cprintf("Supervisor external interrupt\n");
+        break;

+    case IRQ_M_EXT:
+        cprintf("Machine external interrupt\n");
+        break;
+
+    default:
+        cprintf("Unknown interrupt: cause %lu\n",
+                (unsigned long)cause);
+        print_trapframe(tf);
+        break;
+    }
+}
+
+void exception_handler(struct trapframe *tf)
+{
+    switch (tf->cause)
+    {
+    case CAUSE_ILLEGAL_INSTRUCTION:
+        cprintf("Illegal instruction at 0x%016lx\n",
+                (unsigned long)tf->epc);
+        break;
+
+    case CAUSE_BREAKPOINT:
+        cprintf("Breakpoint at 0x%016lx\n",
+                (unsigned long)tf->epc);
+        break;
+
+    default:
+        print_trapframe(tf);
+        return;
+    }
+
+    /* Lab3 kernel text is readable; fetch only the first 16-bit parcel. */
+    uint16_t insn = *(const volatile uint16_t *)tf->epc;
+    /* For the target's 16/32-bit instructions, low bits 11 mean 4 bytes. */
+    tf->epc += (insn & 0x3) == 0x3 ? 4 : 2;
+}

 /* *
  * trap - handles or dispatches an exception/interrupt. if and when trap()
diff --git a/code/kern/trap/trap.h b/code/kern/trap/trap.h
index 257ee62..b03c09b 100644
--- a/code/kern/trap/trap.h
+++ b/code/kern/trap/trap.h
@@ -52,5 +52,7 @@ void trap(struct trapframe *tf);
 void idt_init(void);
 void print_trapframe(struct trapframe *tf);
 void print_regs(struct pushregs *gpr);
+void interrupt_handler(struct trapframe *tf);
+void exception_handler(struct trapframe *tf);

 #endif /* !__KERN_TRAP_TRAP_H__ */
diff --git a/code/kern/trap/trapentry.S b/code/kern/trap/trapentry.S
index 0ea81f9..1b26606 100644
--- a/code/kern/trap/trapentry.S
+++ b/code/kern/trap/trapentry.S
@@ -53,15 +53,54 @@
     .endm

     .macro RESTORE_ALL
-    #lab3:填写你在lab3中实现的代码
+    # restore sepc and sstatus through a0, then restore the saved a0 below
+    LOAD a0, 33*REGBYTES(sp)
+    csrw sepc, a0
+    LOAD a0, 32*REGBYTES(sp)
+    csrw sstatus, a0
+
+    LOAD x1, 1*REGBYTES(sp)
+    LOAD x3, 3*REGBYTES(sp)
+    LOAD x4, 4*REGBYTES(sp)
+    LOAD x5, 5*REGBYTES(sp)
+    LOAD x6, 6*REGBYTES(sp)
+    LOAD x7, 7*REGBYTES(sp)
+    LOAD x8, 8*REGBYTES(sp)
+    LOAD x9, 9*REGBYTES(sp)
+    LOAD x10, 10*REGBYTES(sp)
+    LOAD x11, 11*REGBYTES(sp)
+    LOAD x12, 12*REGBYTES(sp)
+    LOAD x13, 13*REGBYTES(sp)
+    LOAD x14, 14*REGBYTES(sp)
+    LOAD x15, 15*REGBYTES(sp)
+    LOAD x16, 16*REGBYTES(sp)
+    LOAD x17, 17*REGBYTES(sp)
+    LOAD x18, 18*REGBYTES(sp)
+    LOAD x19, 19*REGBYTES(sp)
+    LOAD x20, 20*REGBYTES(sp)
+    LOAD x21, 21*REGBYTES(sp)
+    LOAD x22, 22*REGBYTES(sp)
+    LOAD x23, 23*REGBYTES(sp)
+    LOAD x24, 24*REGBYTES(sp)
+    LOAD x25, 25*REGBYTES(sp)
+    LOAD x26, 26*REGBYTES(sp)
+    LOAD x27, 27*REGBYTES(sp)
+    LOAD x28, 28*REGBYTES(sp)
+    LOAD x29, 29*REGBYTES(sp)
+    LOAD x30, 30*REGBYTES(sp)
+    LOAD x31, 31*REGBYTES(sp)
+
+    # x2 must be restored last because sp addresses the trapframe below
+    LOAD x2, 2*REGBYTES(sp)
     .endm

     .globl __alltraps
 __alltraps:
     SAVE_ALL

-    #lab3:填写你在lab3中实现的代码
     # sp should be the same as before "jal trap"
+    mv a0, sp
+    jal trap

     .globl __trapret
 __trapret:
```

当前修改等待人工审查，没有 commit 或 push。

> 请对当前 Lab4 第二阶段 Trap 恢复结果进行一次补充审查。
>
> 本次只读检查优先，不要直接修改代码，也不要 commit 或 push。
>
> 重点检查以下问题：
>
> ### 1. x0 的 TrapFrame 保存
>
> 检查 code/kern/trap/trapentry.S 中 SAVE_ALL：
>
> - 是否保存 x0 到 0\*REGBYTES(sp)？
> - 如果没有保存，检查 trap.h 中 gpr.zero 是否会被 print_regs() 或其他函数读取。
> - 不能将 RISC-V x0 硬连线为零与 TrapFrame 内存中的 zero 字段混为一谈。
>
> 如果确实存在未初始化字段，请给出最小修复方案。
>
> ### 2. exception_handler() 的 default 分支
>
> 将当前 Lab4 的 exception_handler() 与 Lab3 最终版本进行逐行对比。
>
> 历史版本：
>
> 8866198125bc26af3fec7826e52a5b33052d0be9
>
> 重点检查：
>
> - default 分支是否只是 print_trapframe() 后返回？
> - 是否存在 panic、死循环或其他终止机制？
> - 当前代码是否改变了历史版本的异常处理语义？
>
> 分析如果未处理的异常在 EPC 不变的情况下执行 sret，会发生什么。
>
> 如果历史代码本身存在重复 Trap 风险，请明确说明，不要擅自将其描述为正确行为。
>
> ### 3. interrupt_handler() 的 default 分支
>
> 检查未知中断打印 TrapFrame 后返回是否可能导致重复中断。
>
> 分析是否需要终止执行或采取其他处理方式，并对照 Lab3 历史代码确认原有设计。
>
> ### 4. Trap 入口与返回
>
> 重新核对：
>
> - SAVE_ALL 保存的所有寄存器。
> - RESTORE_ALL 恢复的所有寄存器。
> - offset 0 到 offset 35 的布局。
> - sp 是否最后恢复。
> - sepc 和 sstatus 是否正确恢复。
> - forkrets 是否仍然可以进入 \_\_trapret。
>
> 特别检查 SAVE_ALL 使用 s0、s3 作为临时寄存器时，原始值是否已经被正确保存。
>
> ### 5. 最终输出
>
> 请分别标记：
>
> - 已确认正确。
> - 存在潜在问题。
> - 需要进一步运行时验证。
>
> 如果需要修改，先给出建议和原因，等待人工确认。
>
> 不要修改 Lab2 文件，不要修改 Lab4 的进程管理和页表 TODO。

<details><summary>2 previous messages</summary>

> 我会保持工作区不变，重点做三类证据核对：当前与 Lab3 最终提交 `8866198` 的 handler 逐行差异、TrapFrame 各槽位的保存/恢复覆盖关系，以及 `print_trapframe()` 对 `gpr.zero` 的实际读取路径。结论中会区分“历史原有行为”和“确实存在的稳健性风险”。
>
> 初步发现一个需要确认的点：当前 Lab4 `SAVE_ALL` 确实没有写 offset 0，而 [print_regs()](./code/kern/trap/trap.c:49) 会读取 `gpr.zero`。我继续做逐行归一化差异和完整 0–35 槽位覆盖检查，确认该问题是否仅影响诊断输出，以及 handler 默认分支是否与历史提交完全一致。
</details>

本次仅做只读审查，没有修改任何文件，也没有 commit 或 push。当前工作区仍保留上一阶段三个 Trap 文件的修改。

总体结论：Trap 保存/恢复主流程正确，handler 语义与 Lab3 最终提交一致。不过 `TrapFrame.gpr.zero` 字段确实存在未初始化问题，另外两个 `default` 分支存在历史遗留的重复 Trap 风险。

## 1. x0 / gpr.zero

已确认 [trapentry.S](./code/kern/trap/trapentry.S:8) 没有保存 x0：

```asm
addi sp, sp, -36 * REGBYTES
STORE x1, 1*REGBYTES(sp)
STORE x3, 3*REGBYTES(sp)
...
```

因此保存槽位为：

```text
1, 2, 3–35
```

唯独 offset 0 没有被写入。

但 [trap.h](./code/kern/trap/trap.h:8) 中仍有：

```c
uintptr_t zero;
```

而 [print_regs()](./code/kern/trap/trap.c:49) 会实际读取该字段：

```c
cprintf("  zero     0x%08x\n", gpr->zero);
```

检查结果：

- `gpr.zero` 只被 `print_regs()` 读取，没有参与 Trap 分发、EPC 计算或上下文恢复。
- 因此不会直接影响 CPU 执行状态，但会打印栈上遗留的随机数据。
- `exception_handler()` 的 default 分支和 `interrupt_handler()` 的 default 分支都会调用 `print_trapframe()`，所以错误信息中的 zero 可能是伪值。
- RISC-V 架构寄存器 x0 永远为零，不代表 TrapFrame 中 offset 0 的内存会自动为零。

最小修复是在 `SAVE_ALL` 分配 TrapFrame 后增加：

```asm
STORE x0, 0*REGBYTES(sp)
```

不需要增加 `LOAD x0`，因为 x0 本身仍由 RISC-V 保证为零。Lab3 最终版本存在这条保存指令，当前 Lab4 原框架把它删除了。

建议只增加这一条指令，等待你确认后再修改。

## 2. exception_handler() default 分支

与 Lab3 最终提交 `8866198125bc26af3fec7826e52a5b33052d0be9` 逐行比较：

```c
default:
    print_trapframe(tf);
    return;
```

当前实现与历史版本语义完全一致，只有大括号格式不同。

已确认：

- 只有 `print_trapframe()`。
- 没有 `panic()`。
- 没有死循环。
- 没有修改 EPC。
- 没有其他终止机制。

风险如下：

1. 同步异常发生并进入 default 后，`tf->epc` 保持指向故障指令。
2. handler 返回，最终执行 `RESTORE_ALL` 和 `sret`。
3. `sret` 将 PC 恢复到同一个 `sepc`。
4. 同一条指令再次触发相同异常，形成持续重复 Trap。

因此这是历史代码本身存在的重复 Trap 风险。不能把 default 返回描述为“正确处理了未知异常”，只能说明它保持了 Lab3 的测试导向语义。

如果后续需要增强健壮性，建议在 default 分支打印后直接 `panic()` 或其他不会返回的 fatal 路径。不要盲目执行：

```c
tf->epc += 4;
```

未知异常可能是 16 位指令、页故障或其他无法通过跳过指令安全恢复的情况。

对于非法指令和断点，当前逻辑根据指令低两位选择 2 或 4 字节步进，与 Lab3 一致，没有发现偏差。

## 3. interrupt_handler() default 分支

当前版本和 Lab3 最终版本均为：

```c
default:
    cprintf("Unknown interrupt: cause %lu\n",
            (unsigned long)cause);
    print_trapframe(tf);
    break;
```

函数随后正常返回，没有 panic 或死循环。

可能的问题：

- 中断返回后，如果产生该中断的硬件条件仍处于 pending/asserted 状态，`sret` 后可能立即再次进入同一个中断。
- 外部中断分支目前也只打印信息，没有执行设备确认或清除操作。
- `picirq.c` 当前是空实现，不能依赖它清除外部中断。
- 但对于时钟中断，`IRQ_S_TIMER` 分支已经重新设置下一次事件，不会因为本次时钟中断保持 pending。

这与 Lab3 历史设计一致。对于当前 Lab3 风格测试可以保留；若要支持真实外部设备，需要增加对应中断源的 ack/clear 逻辑。未知中断若无法安全恢复，也可以改为 fatal。本次不建议在历史迁移阶段改变行为。

## 4. Trap 入口和返回

### 已保存槽位

| 槽位 | 内容 | 保存方式 |
|---:|---|---|
| 0 | `gpr.zero` | 未保存，当前问题 |
| 1 | `gpr.ra` | `STORE x1` |
| 2 | `gpr.sp` | 原 sp 暂存于 `sscratch`، 再存 `s0` |
| 3 | `gpr.gp` | `STORE x3` |
| 4 | `gpr.tp` | `STORE x4` |
| 5–7 | `t0–t2` | `STORE x5–x7` |
| 8–9 | `s0–s1` | `STORE x8–x9` |
| 10–17 | `a0–a7` | `STORE x10–x17` |
| 18–27 | `s2–s11` | `STORE x18–x27` |
| 28–31 | `t3–t6` | `STORE x28–x31` |
| 32 | `status` | `sstatus` |
| 33 | `epc` | `sepc` |
| 34 | `badvaddr` | CSR `0x143` / `stval` |
| 35 | `cause` | `scause` |

### 已确认正确

- 所有 x1、x3–x31 均在进入 C 代码前保存。
- `sp` 在调整栈指针前通过 `sscratch` 保存，写入 offset 2。
- `sstatus`、`sepc`、`stval`、`scause` 分别写入 offset 32–35。
- `RESTORE_ALL` 先从 offset 33 恢复 `sepc`，再从 offset 32 恢复 `sstatus`。
- 随后恢复 x1、x3–x31，包括覆盖临时寄存器使用的原始 a0。
- `sp` 在 offset 2 最后恢复，避免提前破坏 TrapFrame 基址。
- offset 34、35 不恢复是设计使然，它们是只读诊断信息。
- x0 不恢复也是正确的；问题只在未初始化它的内存槽位。
- `forkrets` 仍执行：

```asm
move sp, a0
j __trapret
```

因此新建进程路径仍会经过 `RESTORE_ALL` 和 `sret`。

### s0、s3 临时寄存器检查

`SAVE_ALL` 使用 s0–s4 保存 CSR，但它们的原值已提前保存：

| 临时寄存器 | 原始通用寄存器 | 保存偏移 |
|---|---|---:|
| `s0` / x8 | 原始 s0 | 8 |
| `s1` / x9 | 原始 s1 | 9 |
| `s2` / x18 | 原始 s2 | 18 |
| `s3` / x19 | 原始 s3 | 19 |
| `s4` / x20 | 原始 s4 | 20 |

因此之后再执行：

```asm
csrr s0, sscratch
csrr s3, 0x143
```

不会丢失原始 s0、s3。`RESTORE_ALL` 也会分别从 offset 8 和 19 恢复它们。

编译期断言已确认：

```text
sizeof(pushregs)  = 256
sizeof(trapframe) = 288
status    offset = 256
epc       offset = 264
badvaddr  offset = 272
cause     offset = 280
```

## 5. 审查分类

### 已确认正确

- TrapFrame 大小为 36 个 64 位槽位。
- x1、x3–x31 的保存和恢复。
- sp、sepc、sstatus 的保存恢复顺序。
- s0–s4 临时使用不会丢失原值。
- `mv a0, sp`、`jal trap`。
- `__trapret` 和 `forkrets`。
- 非法指令、断点的 EPC 步进。
- 当前 handler 与 Lab3 最终版本语义一致。
- `trap.c`、`trapentry.S` 通过只读 `-Werror` 编译。

### 存在潜在问题

- `TrapFrame.gpr.zero` 未初始化，错误诊断可能打印随机值。
- 未处理异常在 EPC 不变时通过 `sret` 返回，会重复触发同一异常。
- 未知中断或未确认的外部中断可能在 `sret` 后立即重复进入。
- 外部中断分支没有设备确认逻辑；这是仓库中的既有简化。

### 需要进一步运行时验证

- 人为触发未知同步异常，观察是否持续打印相同 TrapFrame。
- 人为触发未知中断，观察中断源是否保持 pending。
- 修复 offset 0 后确认 `print_regs()` 输出 `zero 0x00000000`。
- 验证 16 位和 32 位断点/非法指令的 EPC 增量。
- 后续进程功能完成后验证 `forkrets` 首次运行路径。

当前无法直接启动完整 QEMU 测试，因为 Lab4 后续的 `proc_init`、`cpu_idle`、`page_insert`、`page_remove` 仍未实现。

建议修改只有一条：

```diff
     addi sp, sp, -36 * REGBYTES
+    STORE x0, 0*REGBYTES(sp)
     # save gprs
     STORE  x1,1*REGBYTES(sp)
```

是否执行这一处最小修复，等待人工确认。

> 好的，请执行这一处最小修复

<details><summary>2 previous messages</summary>

> 我只在 `SAVE_ALL` 中补一条 `STORE x0, 0*REGBYTES(sp)`，不修改其他寄存器保存、handler 或 Lab2/Lab4 TODO；随后做 diff 检查、TrapFrame 编译检查和单文件汇编构建。
>
> <details><summary>Edited a file</summary>
>
> - Wrote `./code/kern/trap/trapentry.S` (+1 -0)
>
> </details>
</details>

已完成最小修复，在 [trapentry.S](./code/kern/trap/trapentry.S:9) 增加：

```asm
STORE x0, 0*REGBYTES(sp)
```

验证结果：

- TrapFrame offset 0–35 现在全部有定义。
- 反汇编确认生成了 `sd zero,0(sp)`。
- x0 仍不参与 `RESTORE_ALL`，符合 RISC-V x0 只读恒零的语义。
- `trapentry.S` 通过 RISC-V 交叉编译器 `-Werror` 编译。
- `git diff --check` 通过。
- 未修改其他文件，未 commit、未 push。

当前状态：

```text
## lab4...origin/lab4 [ahead 1]
 M code/kern/trap/trap.c
 M code/kern/trap/trap.h
 M code/kern/trap/trapentry.S
```

`RESTORE_ALL` 仍跳过 offset 0、34、35，分别对应 x0、`badvaddr` 和 `cause`，这是预期行为。

> 好的，依旧不改变仓库的任何代码：
> 我此次要实现的功能是虚拟内存页操作功能。然后主要实现的函数是int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);(kern/mm/pmm.c) 和void page_remove(pde_t *pgdir, uintptr_t la);(kern/mm/pmm.c)。
> 我需要你给我解释一下虚拟内存页操作在操作系统中发挥什么样的作用，然后告诉我我需要实现的这两个函数在哪个文件中，同时又发挥着什么样的作用。然后是在“虚拟内存页操作”这个功能这里，我们现有的项目已经实现了多少

<details><summary>2 previous messages</summary>

> 我会以只读方式检查 `pmm.c`、`pmm.h`、`vmm.c` 以及 Lab4 测试注入代码，确认 `page_insert/page_remove` 的调用关系、现有页表基础设施和尚未实现的范围，不改动任何文件。
>
> 现有页表基础比两个目标函数多：`get_pte/get_page`、页表创建、权限 PTE 构造和 TLB 局部失效都已实现；但映射建立与拆除仍为空，因此启动中的 `check_pgdir()` 和 Lab4 PMM 测试目前会直接引用未定义符号。接下来我确认文件尾部的页表打印辅助和测试期望，再给出完整完成度判断。
</details>

当前仓库没有代码改动。以下结论基于当前 `lab4` 工作区。

## 虚拟内存页操作的作用

操作系统不能直接把物理内存地址暴露给进程。它给进程提供虚拟地址，并维护“虚拟页 → 物理页框”的映射：

```text
虚拟地址
  → 页表遍历
  → 虚拟页号 VPN 对应的物理页号 PPN
  → 物理地址 + 页内偏移
  → 实际访问物理内存
```

页操作处在 VMM 和 PMM 之间：

- PMM 负责分配、回收物理页。
- VMM 负责描述进程的虚拟地址区域。
- 页操作负责把具体的物理页真正写入或移出页表。

它支撑了 OS 的多项核心能力：

- 地址转换：让程序使用连续的虚拟地址，即使数据在物理内存中不连续。
- 进程隔离：不同进程使用不同页表，同一虚拟地址可以映射到不同物理页。
- 内存保护：通过 PTE 的 `PTE_R/PTE_W/PTE_X/PTE_U` 控制读写、执行和用户态访问。
- 页面共享：多个虚拟地址可以映射到同一个物理页，通过引用计数管理生命周期。
- 按需分页：缺页时再分配物理页并建立映射。
- 虚拟地址空间管理：支持代码段、数据段、栈、堆和 mmap 等不同区域。
- 内核地址映射：将内核虚拟地址映射到对应物理内存。

只有页表、物理页和页操作三者配合，虚拟内存才能实际工作。

## 需要实现的两个函数

两个函数都应位于：

[pmm.c](./code/kern/mm/pmm.c:332)

当前文件中只有占位：

```c
//LAB4 function1 YOUR CODE
```

对应的接口声明已经存在于：

[pmm.h](./code/kern/mm/pmm.h:44)

```c
void page_remove(pde_t *pgdir, uintptr_t la);
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
```

### page_insert()

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
```

作用：在页目录 `pgdir` 中，把虚拟地址 `la` 所在页面映射到 `page` 描述的物理页框，并设置权限 `perm`。

它的核心语义包括：

- 确保虚拟地址对应的页表项存在，必要时创建中间页表。
- 如果该虚拟页已经有旧映射，需要减少旧物理页的引用计数。
- 如果新旧映射是同一个物理页，不能错误地将其释放。
- 增加新物理页的引用计数。
- 将物理页号及 `PTE_V/PTE_U/PTE_W` 等权限写入 PTE。
- 修改当前正在使用的页表后，需要使相关 TLB 项失效。
- 内存不足时返回错误，成功时返回 0。

同一个物理页可以同时出现在多个虚拟地址中，所以引用计数表示“这个物理页当前被多少个映射引用”。

### page_remove()

```c
void page_remove(pde_t *pgdir, uintptr_t la);
```

作用：撤销 `la` 所在虚拟页在 `pgdir` 中的映射。

它的核心语义包括：

- 查找对应的 PTE，但不创建新页表。
- 如果该地址本来没有映射，应当安全地什么都不做。
- 如果有有效映射，清除 PTE。
- 减少原物理页的引用计数。
- 引用计数降为零时，将物理页归还 PMM。
- 使相关 TLB 项失效。
- 同一地址重复调用 `page_remove()` 不应产生错误。

## 项目当前已实现的部分

### 已经完成

| 功能 | 当前状态 |
|---|---|
| 物理页探测和 Page 描述符初始化 | 已完成 |
| Best-Fit 物理页分配和释放 | 已完成 |
| `alloc_page/free_page` | 已完成 |
| 物理地址与虚拟地址转换 | 已完成 |
| 启动阶段内核映射 | 已完成 |
| 页表遍历与中间页表分配 | 已完成 |
| 查询虚拟地址对应页面 | 已完成 |
| PTE 权限和物理页号转换 | 已完成 |
| Page 引用计数接口 | 已完成 |
| 局部 TLB 失效 | 已完成 |
| 页目录自检代码 | 已完成 |

具体包括：

- [get_pte()](./code/kern/mm/pmm.c:286)：遍历页表；页表不存在且 `create=true` 时，分配并初始化中间页表。
- [get_page()](./code/kern/mm/pmm.c:318)：根据虚拟地址查询当前映射的 `struct Page`。
- [boot_map_segment()](./code/kern/mm/pmm.c:212)：启动期间直接建立内核地址映射。
- [tlb_invalidate()](./code/kern/mm/pmm.c:342)：执行 `sfence.vma`。
- `page2pa`、`pa2page`、`pte2page`、`pte_create` 等地址和 PTE 辅助函数已经存在。
- [check_pgdir()](./code/kern/mm/pmm.c:355) 和 [check_boot_pgdir()](./code/kern/mm/pmm.c:417) 已经写好了映射、覆盖、权限和引用计数的检查逻辑。

VMM 结构层也完成了一部分：

- `mm_create()`
- `vma_create()`
- `find_vma()`
- `insert_vma_struct()`
- `mm_destroy()`
- VMA 插入、查找和非重叠检查

这些位于 [vmm.c](./code/kern/mm/vmm.c:64)。

### 尚未完成

| 功能 | 当前状态 |
|---|---|
| `page_insert()` | 未实现 |
| `page_remove()` | 未实现 |
| `pgdir_alloc_page()` | 仅声明，无定义 |
| `print_pgdir()` | 仅声明，无定义 |
| `do_pgfault()` | 仅声明，无定义 |
| 用户进程独立页目录 | 尚未接入 |
| 缺页分配和按需分页 | `check_pgfault()` 仍被注释 |

因此可以说：项目已经完成了“页表遍历、物理页管理、PTE 操作和检查框架”，但最关键的“建立和拆除实际页面映射”仍是空的。

## 当前测试要求

[test_lab4.c](./code/tools/test_lab4.c:13) 对两个函数有明确契约：

- 新页面插入后 PTE 有效，引用计数为 1。
- 替换旧映射时，旧页面引用计数减少。
- 插入相同物理页时，不能发生错误的重复引用。
- 对同一地址重复插入时，空闲页数量必须保持不变。
- 删除映射应将引用计数降为 0。
- 同一地址重复删除必须安全。
- PTE 中的权限必须与传入的 `perm` 一致。

此外，普通启动路径中的 `check_pgdir()` 和 `check_boot_pgdir()` 已经调用这两个函数。尽管测试注入尚未发生，当前内核链接也会因为这两个符号缺失而失败。

## 完成度判断

可以概括为：

```text
虚拟内存页操作基础设施：大部分已完成
页表查询与创建：       已完成
PTE/引用计数辅助：      已完成
TLB 失效接口：          已完成
page_insert/page_remove：0%，当前核心缺口
上层缺页处理：          未完成
```

这两个函数完成后，虚拟地址和物理页之间才第一次具备完整的动态绑定与解绑能力，后续的进程地址空间、缺页处理和用户态程序加载才能继续实现。

> 我们最后是如何审查代码是否完全完成呢？还是make grade吗？

最后审查时，`make grade` 是主要的自动验收手段，但不是唯一标准。它适合作为最终门禁，不能单独证明仓库完全干净。

## 当前阶段能否直接 make grade

现在还不能。

`make grade` 会注入并检查整个 Lab4，包括：

- `[Test PMM] PASS`
- `[Test Process Create] PASS`
- `[Test Schedule] PASS`
- init 线程输出
- 最终 `30/30`

对应测试位于 [test_lab4.c](./code/tools/test_lab4.c:13)。因此，即使 `page_insert/page_remove` 完全正确，只要 `proc_init`、`cpu_idle`、`kernel_thread`、`schedule`、`switch_to` 尚未实现，全树仍会链接失败，`make grade` 也不能通过。

在只实现页操作的阶段：

- 可以用 `make obj/kern/mm/pmm.o` 做编译检查。
- 内核启动时的 `check_pgdir()` 和 `check_boot_pgdir()` 会实际检查映射、权限和引用计数。
- 由于当前内核还不能完整链接，这两个运行时检查暂时无法启动。
- `make grade` 失败不能直接归因于页操作。

## 最终推荐验收流程

| 检查项 | 命令或方法 | 目的 |
|---|---|---|
| 分支与工作区 | `git status --short --branch` | 确认在 `lab4`，看清楚哪些文件有改动 |
| 空白与冲突 | `git diff --check` | 检查空白错误、冲突标记 |
| Diff 审查 | `git diff origin/lab4 -- code/` | 确认只修改了预期源码 |
| 测试完整性 | `git diff origin/lab4 -- code/tools/test_lab4.*` | 测试文件应当没有改动 |
| 残留检查 | `rg "YOUR CODE|请补充|TODO|Not Implemented" code` | 确认需要实现的占位已清空，保留项有明确理由 |
| 全树构建 | `cd code && make -k` | 确认所有源码编译和最终链接成功 |
| 自动评分 | `cd code && make grade` | 最终功能验收 |
| 运行时复查 | `make qemu` | 手工观察启动、panic 和测试输出 |
| 调试记录 | GDB/backtrace | 用于实验报告，但不能代替自动评分 |

`make grade` 会先把工程复制到 `/tmp/lab4_grade`，注入测试，再构建并在 QEMU 中运行。因此它通常不会污染仓库，但也会调用 `make clean`。

## make grade 的局限

最终还需要人工确认：

- 没有修改 `test_lab4.c`、`test_lab4.h` 或评分脚本绕过测试。
- 没有临时 stub、空函数或调试输出残留。
- Lab2 的 PMM 没有被误改。
- 进程、调度、页表功能没有互相覆盖。
- `git status` 只包含本节应有的代码修改。
- 报告和 GDB 截图符合实验要求。

所以最终判断应当采用：

```text
源码审查通过
+ 全树构建通过
+ make grade 满分
+ 工作区改动范围正确
= 功能验收完成
```

最简单的结论是：最终确实应以 `make grade` 为准，但必须同时做 diff、工作区和测试文件完整性审查；而在当前只做到页操作的阶段，`make grade` 还不能作为有效门禁。

> # [PROMPT]
>
> 现在开始实现操作系统 Lab4 的第一个功能模块：虚拟内存页操作。
>
> 本次只实现 `page_remove()` 函数。
>
> 目标文件：
>
> `code/kern/mm/pmm.c`
>
> 函数原型：
>
> ```c
> void page_remove(pde_t *pgdir, uintptr_t la);
> ```
>
> 本函数负责删除指定页目录中，线性地址 `la` 对应的有效物理页映射，并正确维护物理页引用计数和 TLB 状态。
>
> 具体需要完成：
>
> 1. 查找线性地址对应的页表项。
> 2. 判断该页表项是否存在以及是否有效。
> 3. 如果存在有效映射，获取其对应的物理页描述符。
> 4. 清除对应页表项。
> 5. 对相应地址执行 TLB 失效。
> 6. 减少被删除映射所对应物理页的引用计数。
> 7. 当物理页引用计数降为零时，调用物理页释放接口归还物理页。
> 8. 如果目标地址不存在有效映射，则安静返回，不产生错误。
>
> 本函数必须具备幂等性：对同一个地址重复调用 `page_remove()`，不能造成重复释放、引用计数下溢或者其他副作用。
>
> 本次不需要处理页面替换逻辑，也不需要实现 `page_insert()`。
>
> # [RELY]
>
> 本次实现可以依赖当前项目中已经存在的代码和接口，但必须先阅读实际源码，不能仅凭函数名称推断接口行为。
>
> 重点检查以下文件。
>
> ## 1. code/kern/mm/pmm.c
>
> 重点阅读：
>
> - `get_pte()`：根据页目录和线性地址获取对应页表项。
> - `get_page()`：查询指定线性地址对应的物理页。
> - `tlb_invalidate()`：使指定地址对应的 TLB 项失效。
> - `flush_tlb()`：检查当前项目是否提供全局 TLB 刷新接口。
> - `page_remove()`：本次需要填写的函数位置。
> - `check_pgdir()`：了解页映射删除的测试要求。
> - `check_boot_pgdir()`：了解现有内核映射的检查方式。
>
> ## 2. code/kern/mm/pmm.h
>
> 重点阅读：
>
> - `struct Page`。
> - `page_ref_inc()`。
> - `page_ref_dec()`。
> - `page_ref()` 或项目实际使用的引用计数读取接口。
> - `set_page_ref()`。
> - `free_page()`。
> - `free_pages()`。
> - `pte2page()`。
> - `get_pte()` 和 `page_remove()` 的声明。
>
> 必须确认引用计数接口的实际返回值、参数和语义。
>
> 特别检查 `page_ref_dec()` 是返回递减后的引用计数，还是仅仅修改引用计数。如果它不返回引用计数，应使用项目已有的读取接口判断是否为零。
>
> ## 3. code/kern/mm/mmu.h
>
> 重点检查：
>
> - `pde_t`、`pte_t`。
> - `PTE_V`。
> - 页表项的物理页号组织方式。
> - `pte2page()` 所依赖的地址转换关系。
>
> ## 4. code/kern/mm/default_pmm.c
>
> 重点检查：
>
> - 物理页释放接口最终如何调用 Best-Fit 分配器。
> - `free_pages()` 对待释放页面有什么约束。
> - 页面释放时是否要求引用计数为零。
> - 页面释放时是否要求页面不具有 `PG_reserved` 或 `PG_property` 等状态。
>
> 本项目已经恢复了 Lab2 的 Best-Fit 物理内存管理，因此必须遵循现有分配器的约束，不要修改分配器。
>
> ## 5. code/tools/test_lab4.c
>
> 检查实验测试代码中关于页面删除、引用计数和重复删除的测试要求。
>
> # [GUARANTEE]
>
> 本次修改必须严格遵守以下约束：
>
> 1. 只修改 `code/kern/mm/pmm.c` 中 `page_remove()` 的实现位置。
> 2. 不修改 `page_insert()`，`get_pte()`， `tlb_invalidate()`，`flush_tlb()`，`check_pgdir()` 和 `check_boot_pgdir()`，不修改 `pmm.h`、`mmu.h` 和 `default_pmm.c`。
> 3. 不修改 Lab2 已恢复的 Best-Fit 分配器。
> 4. 不修改 Lab3 已恢复的 Trap 相关代码。
> 5. 不修改其他成员负责的进程创建、初始化和调度代码。
> 6. 不提前实现 `pgdir_alloc_page()`、`print_pgdir()` 或其他 Lab4 函数。
> 7. 不修改实验测试代码。
> 8. 不进行无关重构和格式化。
> 9. 不执行 Git commit、push、merge 或分支切换。
>
> 特别注意：
>
> - 必须使用 `get_pte(pgdir, la, 0)` 查找页表项，不能传入 `1`，因为删除映射不应该创建新的中间页表。
> - 只有确认 PTE 有效后，才能调用 `pte2page()` 获取物理页。
> - 不得根据无效 PTE 中残留的物理页号执行引用计数操作。
> - 不能在物理页仍然被其他映射引用时释放它。
> - 不得重复减少引用计数。
> - 必须检查 PTE 更新和 TLB 失效的先后关系，避免释放仍可能被旧 TLB 项访问的物理页。
> - 不要擅自使用 `set_page_ref(page, 0)` 替代正常的引用计数递减。
> - 不要在没有检查引用计数的情况下直接调用 `free_page()`。
>
> 如果发现现有接口不足以安全实现，请先报告原因，不要擅自修改其他文件。
>
> # [SPECIFICATION]
>
> 实现：
>
> ```c
> void page_remove(pde_t *pgdir, uintptr_t la);
> ```
>
> 输入参数：
>
> | 参数      | 含义          |
> | ------- | ----------- |
> | `pgdir` | 需要操作的页目录    |
> | `la`    | 需要删除映射的线性地址 |
>
> 返回值：
>
> 无。
>
> 函数执行后应保证目标线性地址不再具有原来的有效物理页映射。
>
> ## #### Pre-Condition
>
> 1. `pgdir` 指向有效的页目录。
> 2. `la` 是当前页表框架允许查询的线性地址。
> 3. 如果目标地址存在有效映射，则该 PTE 对应一个有效的物理页描述符。
> 4. 有效映射对应的物理页具有正确的引用计数。
> 5. 物理页的引用计数能够反映当前有效映射对该物理页的引用情况。
>
> ## #### Post-Condition
>
> ### 情况一：目标页表项不存在
>
> 当：
>
> ```c
> get_pte(pgdir, la, 0) == NULL
> ```
>
> 操作：
>
> 直接返回。
>
> 结果：
>
> - 不分配新的页表页。
> - 不修改任何物理页引用计数。
> - 不释放任何物理页。
> - 不执行不必要的 TLB 失效。
>
> ### 情况二：目标页表项存在，但 PTE 无效
>
> 当页表项存在，但不包含有效映射时：
>
> 操作：
>
> 按照当前项目对无效 PTE 的处理约定，安全返回。
>
> 结果：
>
> - 不调用 `pte2page()`。
> - 不减少任何物理页引用计数。
> - 不释放物理页。
> - 不执行不必要的 TLB 失效。
> - 不得将无效 PTE 中残留的物理页号当成有效映射。
>
> ### 情况三：目标地址存在有效映射，物理页引用计数大于 1
>
> 假设：
>
> `la → page_A`
>
> 且：
>
> `page_ref(page_A) > 1`
>
> 操作：
>
> 1. 通过当前有效 PTE 获取 `page_A`。
> 2. 清除目标 PTE，使映射失效。
> 3. 对目标地址执行适当的 TLB 失效。
> 4. 将 `page_A` 的引用计数减少一次。
> 5. 不释放 `page_A`。
>
> 结果：
>
> - 目标线性地址不再映射到 `page_A`。
> - `page_A` 的引用计数减少 1。
> - 因为仍有其他引用，物理页继续保留。
> - 不影响其他虚拟地址对 `page_A` 的映射。
>
> ### 情况四：目标地址存在有效映射，物理页引用计数等于 1
>
> 假设：
>
> `la → page_A`
>
> 且：
>
> `page_ref(page_A) == 1`
>
> 操作：
>
> 1. 获取原物理页描述符。
> 2. 清除目标 PTE。
> 3. 执行对应的 TLB 失效。
> 4. 将物理页引用计数减少一次。
> 5. 确认引用计数变为零。
> 6. 调用 `free_page(page_A)` 将物理页归还物理页分配器。
>
> 结果：
>
> - 目标线性地址不再映射到 `page_A`。
> - `page_A` 的引用计数变为零。
> - `page_A` 被安全归还物理页分配器。
> - 物理页可以重新分配给其他用途。
>
> ### 情况五：重复删除同一个地址
>
> 例如：
>
> ```c
> page_remove(pgdir, la);
> page_remove(pgdir, la);
> page_remove(pgdir, la);
> ```
>
> 第一次调用成功删除映射后，后续调用应按照目标 PTE 已经无效的情况处理。
>
> 结果：
>
> - 不会重复减少引用计数。
> - 不会重复释放物理页。
> - 不会出现引用计数下溢。
> - 不会产生异常。
> - 函数保持幂等性。
>
> # [IMPLEMENTATION AND VERIFICATION]
>
> 请按照以下步骤完成。
>
> ## 第一步：检查现有代码
>
> 先确认：
>
> - 当前 Git 分支为 `lab4`。
> - 工作区状态。
> - `page_remove()` 当前所在位置。
> - 相关辅助函数的实际定义和接口。
>
> 不要立即修改代码。
>
> ## 第二步：分析实现方案
>
> 先说明：
>
> 1. `get_pte(pgdir, la, 0)` 如何找到页表项。
> 2. 如何判断 PTE 是否有效。
> 3. 如何通过 `pte2page()` 找到对应物理页。
> 4. `page_ref_dec()` 如何维护引用计数。
> 5. `free_page()` 在什么情况下可以调用。
> 6. `tlb_invalidate()` 如何工作。
> 7. 为什么应该在释放物理页之前完成必要的 TLB 失效。
>
> 特别说明当前项目的 TLB 失效接口是否考虑了 `pgdir`，以及它对当前页表和非当前页表的实际行为。
>
> ## 第三步：实现函数
>
> 只填写 `page_remove()` 的实现位置。
>
> 保留其他代码原样。
>
> ## 第四步：进行静态检查
>
> 重点检查：
>
> - 未映射地址。
> - 无效 PTE。
> - 正常删除。
> - 引用计数大于 1。
> - 引用计数等于 1。
> - 重复删除。
> - 物理页是否被正确释放。
> - TLB 是否在正确时机失效。
>
> 对照 `check_pgdir()` 和 `tools/test_lab4.c` 检查接口要求。
>
> ## 第五步：编译检查
>
> 使用当前项目已有的 RISC-V 工具链和 Makefile。
>
> 至少完成：
>
> 1. `pmm.c` 单文件编译。
> 2. 必要的 `-Werror` 检查。
> 3. `git diff --check`。
> 4. 检查是否修改了其他文件。
> 5. 检查是否存在重复定义和警告。
>
> 由于 `page_insert()` 及其他 Lab4 功能尚未全部完成，全树链接失败不应通过提前实现其他函数来解决。
>
> ## 第六步：输出审查报告
>
> 请提供：
>
> 1. 实际修改的代码位置。
> 2. 完整的 `page_remove()` 实现。
> 3. 每种情况下的执行逻辑。
> 4. 引用计数变化分析。
> 5. TLB 失效时机分析。
> 6. `free_page()` 的调用条件。
> 7. 编译检查结果。
> 8. `git diff --stat`。
> 9. `git diff --check`。
> 10. `git status`。
> 11. 完整 diff。
>
> 不要 commit 或 push。
>
> 完成后停止，等待人工审查，不要继续实现 `page_insert()` 或其他 Lab4 功能。

<details><summary>5 previous messages</summary>

> 我会先按只读步骤核对当前 `lab4` 状态、`page_remove()` 占位位置、引用计数与 Best-Fit 释放约束，再只在该位置实现。重点会确认 PTE 清除、`sfence.vma`、引用计数递减和 `free_page()` 的先后顺序。
>
> 实现方案已确认：用 `get_pte(..., 0)` 获取叶子 PTE；仅在 `PTE_V` 有效时解析物理页。顺序采用“保存 page → 清零 PTE → 使该虚拟地址的 TLB 失效 → 引用计数递减 → 计数为零才释放”，这样不会在 TLB 仍可能缓存旧映射时归还物理页。现在只填写 `page_remove()`。
>
> 补丁上下文中的空行数量与文件实际内容不一致，因此没有发生任何修改。我会基于精确的相邻行重新应用同一函数实现。
>
> <details><summary>Edited a file</summary>
>
> - Wrote `./code/kern/mm/pmm.c` (+15 -4)
>
> </details>
>
> `page_remove()` 已通过显式 `-Werror` 单文件编译和顺序断言。现在用项目 Makefile 在 `/tmp` 副本中构建 `pmm.o`，再执行全树 `make -k`，确认剩余失败只来自尚未实现的 Lab4 函数，而不是本次代码。
</details>

`page_remove()` 已实现，仅修改了 [pmm.c](./code/kern/mm/pmm.c:333)，未实现 `page_insert()` 或其他 Lab4 功能，也未 commit 或 push。

## 1. 修改位置

```text
code/kern/mm/pmm.c:332-349
```

函数位于 `get_page()` 之后、原有 `//LAB4 function1 YOUR CODE` 标记下。

## 2. 完整实现

```c
void page_remove(pde_t *pgdir, uintptr_t la)
{
    pte_t *ptep = get_pte(pgdir, la, 0);
    if (ptep == NULL || !(*ptep & PTE_V))
    {
        return;
    }

    struct Page *page = pte2page(*ptep);
    *ptep = 0;
    tlb_invalidate(pgdir, la);

    if (page_ref_dec(page) == 0)
    {
        free_page(page);
    }
}
```

## 3. 各情况执行逻辑

### 页表项不存在

```c
get_pte(pgdir, la, 0) == NULL
```

直接返回：

- 不创建中间页表。
- 不修改引用计数。
- 不释放物理页。
- 不执行 TLB 失效。

### 页表项存在但无效

```c
!(*ptep & PTE_V)
```

直接返回：

- 不调用 `pte2page()`。
- 不会使用无效 PTE 中可能残留的物理页号。
- 不产生其他副作用。

这也是重复调用安全的保证。第一次删除后 PTE 被清零，后续调用会进入该分支。

### 引用计数大于 1

假设 `page_A` 仍被其他虚拟地址映射：

- PTE 清零。
- 执行 `tlb_invalidate()`。
- `page_ref_dec()` 返回递减后的计数。
- 结果大于零，因此不释放 `page_A`。
- 其他虚拟地址的映射不受影响。

### 引用计数等于 1

- PTE 清零。
- 执行 TLB 失效。
- `page_ref_dec()` 将计数降为零。
- `free_page()` 把物理页归还 Best-Fit 分配器。

### 重复删除

第一次调用后 PTE 为 0，后续调用看到 `PTE_V` 无效后直接返回，因此：

- 不会重复递减引用计数。
- 不会重复释放物理页。
- 不会发生引用计数下溢。
- 不会重复执行 TLB 失效。

## 4. 引用计数分析

项目中的接口是：

[pmm.h](./code/kern/mm/pmm.h:157)

```c
static inline int
page_ref_dec(struct Page *page)
{
    page->ref -= 1;
    return page->ref;
}
```

它会返回递减后的引用计数，因此可以直接判断是否为零：

```c
if (page_ref_dec(page) == 0)
{
    free_page(page);
}
```

没有使用 `set_page_ref(page, 0)` 绕过正常的引用计数规则。

Best-Fit 的 `best_free_pages()` 要求页面不能仍处于保留或空闲块头状态。当前实现只释放已从 PTE 移除、且引用计数刚降为零的已分配页面，符合这一约束。

## 5. TLB 顺序分析

采用的顺序是：

```text
读取有效 PTE 对应物理页
    ↓
PTE 清零
    ↓
sfence.vma la
    ↓
引用计数递减
    ↓
必要时 free_page()
```

先清除 PTE，再执行 TLB 失效，最后才可能释放物理页。这样可以避免物理页已经被重新分配，而处理器仍可能通过旧 TLB 项访问该页面。

当前项目的 [tlb_invalidate()](./code/kern/mm/pmm.c:353) 实际实现为：

```c
asm volatile("sfence.vma %0" : : "r"(la));
```

需要注意：

- 它只按虚拟地址 `la` 刷新当前 hart 的 TLB，没有使用 `pgdir`、ASID 或 hart mask。
- 因此参数 `pgdir` 当前实际上未参与失效判断。
- 对当前正在使用的页目录有效。
- 对当前未激活的页目录，这个 TLB 项通常不在当前地址空间中，但该接口不会针对其他 hart 或其他 ASID 做额外刷新。
- 若以后支持多进程地址空间切换、ASID 或多核，需要进一步审查 TLB 管理策略。

本次没有修改该接口。

## 6. free_page() 调用条件

`free_page(page)` 只在以下条件全部满足时调用：

1. `get_pte(pgdir, la, 0)` 返回非空。
2. PTE 设置了 `PTE_V`。
3. PTE 已被清零。
4. 已对 `la` 执行 TLB 失效。
5. `page_ref_dec(page)` 返回 0。

对应展开后的调用是 `free_pages(page, 1)`，最终进入当前 Best-Fit 管理器。

## 7. 静态检查

已检查：

```text
get_pte(..., 0) 而非 1: PASS
PTE_V 判断在 pte2page 之前: PASS
PTE 清零在 TLB 失效之前: PASS
TLB 失效在引用计数和释放之前: PASS
free_page 仅在引用计数为零时调用: PASS
重复删除不会进入持有引用计数递减的分支: PASS
只修改 pmm.c: PASS
```

`page_remove()` 对应测试要求也已核对：

- [check_pgdir()](./code/kern/mm/pmm.c:366) 检查共享页面删除后引用计数是否正确。
- [test_lab4.c](./code/tools/test_lab4.c:38) 包含对同一地址连续调用两次 `page_remove()` 的幂等性测试。

## 8. 编译检查

RISC-V 交叉编译器显式 `-Werror` 单文件编译：

```text
PMM_WERROR_BUILD_OK
```

使用项目 Makefile 单独构建：

```text
+ cc kern/mm/pmm.c
MAKEFILE_PMM_TARGET_OK
```

对象符号检查：

```text
T page_remove
T tlb_invalidate
U page_insert
```

`page_remove` 已正确定义，`page_insert` 仍是预期中的未实现符号。

全树 `make -k` 已完成所有源文件编译，链接仅剩：

```text
proc_init
cpu_idle
page_insert
```

没有链接错误涉及 `page_remove`。

`kmalloc.c` 的 `kva2page()` 整数转指针警告是仓库既有问题，本次未修改该文件。

## 9. Git 检查

```text
git diff --stat

 code/kern/mm/pmm.c | 19 +++++++++++++++----
 1 file changed, 15 insertions(+), 4 deletions(-)
```

```text
git diff --check
```

无输出，检查通过。

```text
git status --short --branch

## lab4...origin/lab4
 M code/kern/mm/pmm.c
```

未修改 Lab2、Lab3、测试文件、进程或调度代码。

## 10. 完整 diff

```diff
diff --git a/code/kern/mm/pmm.c b/code/kern/mm/pmm.c
index 8bd4729..e37bc46 100644
--- a/code/kern/mm/pmm.c
+++ b/code/kern/mm/pmm.c
@@ -330,12 +330,23 @@ struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
 }

 //LAB4 function1 YOUR CODE
+void page_remove(pde_t *pgdir, uintptr_t la)
+{
+    pte_t *ptep = get_pte(pgdir, la, 0);
+    if (ptep == NULL || !(*ptep & PTE_V))
+    {
+        return;
+    }

+    struct Page *page = pte2page(*ptep);
+    *ptep = 0;
+    tlb_invalidate(pgdir, la);

-
-
-
-
+    if (page_ref_dec(page) == 0)
+    {
+        free_page(page);
+    }
+}

 // invalidate a TLB entry, but only if the page tables being
 // edited are the ones currently in use by the processor.
```

当前修改等待人工审查，没有继续实现其他 Lab4 功能。

> # [PROMPT]
>
> 现在开始实现操作系统 Lab4 的另一个功能模块：虚拟内存页操作。
>
> 本次只实现 `page_insert()` 函数。
>
> 目标文件：
>
> `code/kern/mm/pmm.c`
>
> 函数原型：
>
> ```c
> int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
> ```
>
> 本函数负责在指定页目录中，将物理页 `page` 映射到线性地址 `la`，并设置对应的页表权限。
>
> 需要完成以下功能：
>
> 1. 根据页目录和线性地址，获取对应的页表项。
> 2. 如果中间级页表不存在，则尝试分配并创建。
> 3. 如果页表项不存在且无法分配所需页表页，返回 `-E_NO_MEM`。
> 4. 如果目标地址尚无有效映射，建立新的映射。
> 5. 如果目标地址已经映射到其他物理页，正确撤销旧映射，再建立新映射。
> 6. 如果目标地址已经映射到同一个物理页，则只更新权限，不应错误地增加或减少物理页引用计数。
> 7. 正确维护新旧物理页的引用计数。
> 8. 更新页表项时，确保 PTE 包含有效位和调用者指定的权限位。
> 9. 在页表映射更新后，执行适当的 TLB 失效操作。
> 10. 成功返回 `0`，失败返回 `-E_NO_MEM`。
>
> 特别注意：
>
> **同一个物理页可能被多个虚拟地址映射，同一个虚拟地址也可能被重复插入。因此必须正确处理物理页引用计数，不能简单地覆盖原有 PTE。**
>
> # [RELY]
>
> 本次实现可以依赖当前项目中已经存在的代码和接口，但必须先阅读实际源码，不能仅凭函数名称推断接口行为。
>
> 重点检查以下文件。
>
> ## 1. code/kern/mm/pmm.c
>
> 重点阅读：
>
> - `get_pte()`：根据页目录和线性地址获取或创建页表项。
> - `get_page()`：查询指定线性地址对应的物理页。
> - `page_remove()`：上一阶段已经实现的映射删除函数。
> - `tlb_invalidate()`：使指定地址对应的 TLB 项失效。
> - `flush_tlb()`：检查项目是否提供全局 TLB 刷新接口。
> - `page_insert()`：本次需要填写的函数位置。
> - `check_pgdir()`：了解页面映射、替换和引用计数的测试逻辑。
> - `check_boot_pgdir()`：了解内核页表的检查方式。
>
> 特别注意：
>
> 当前 `page_remove()` 已经实现，
>
> 可以调用该函数处理旧映射，但不允许修改它。
>
> ## 2. code/kern/mm/pmm.h
>
> 重点阅读：
>
> - `struct Page`。
> - `page_ref_inc()`。
> - `page_ref_dec()`。
> - `page_ref()` 或项目实际提供的引用计数读取接口。
> - `set_page_ref()`。
> - `alloc_page()`。
> - `free_page()`。
> - `free_pages()`。
> - `pte2page()`。
> - `get_pte()` 和 `page_insert()` 的声明。
>
> 必须确认：
>
> - `page_ref_inc()` 是否返回值。
> - `page_ref_dec()` 是否返回递减后的引用计数。
> - `pte2page()` 如何从有效 PTE 获取物理页描述符。
> - `free_page()` 最终如何归还物理页。
>
> ## 3. code/kern/mm/mmu.h
>
> 重点检查：
>
> - `pde_t`、`pte_t`。
> - `PTE_V`。
> - `PTE_U`。
> - `PTE_R`。
> - `PTE_W`。
> - `PTE_X`。
> - `pte_create()`。
> - `page2pa()`、`pa2page()` 等地址转换辅助函数。
>
> 必须确认 `pte_create()` 是否自动设置有效位。
>
> 最终生成的叶子 PTE 必须包含 `PTE_V`，同时保留调用者通过 `perm` 指定的权限。
>
> ## 4. code/kern/mm/default_pmm.c
>
> 重点检查：
>
> - 当前 Best-Fit 物理页分配器。
> - `free_pages()` 的实际调用路径。
> - 页面释放时的状态约束。
>
> 本项目已经恢复了 Lab2 的 Best-Fit 物理内存管理，本次不得修改分配器。
>
> ## 5. code/tools/test_lab4.c
>
> 重点阅读测试代码，特别关注：
>
> - 新页面插入。
> - 替换已有映射。
> - 同一个物理页重复插入。
> - 同一个物理页映射到不同虚拟地址。
> - 引用计数变化。
> - 空闲物理页数量变化。
> - PTE 权限检查。
>
> 实现必须符合现有测试代码的预期。
>
> # [GUARANTEE]
>
> 本次修改必须严格遵守以下约束：
>
> 1. 只修改 `code/kern/mm/pmm.c` 中 `page_insert()` 的实现位置。
> 2. 不修改 `page_remove()`。
> 3. 不修改 `get_pte()`、`get_page()`、`tlb_invalidate()`、`flush_tlb()`。
> 4. 不修改 `check_pgdir()` 和 `check_boot_pgdir()`。
> 5. 不修改 `pmm.h`、`mmu.h` 和 `default_pmm.c`。
> 6. 不修改 Lab2 已恢复的 Best-Fit 分配器。
> 7. 不修改 Lab3 已恢复的 Trap 相关代码。
> 8. 不修改其他成员负责的进程创建、初始化和调度代码。
> 9. 不提前实现 `pgdir_alloc_page()`、`print_pgdir()` 或其他 Lab4 函数。
> 10. 不修改实验测试代码。
> 11. 不进行无关重构和格式化。
> 12. 不执行 Git commit、push、merge 或分支切换。
>
> 特别注意以下事项：
>
> **一、同一物理页的特殊处理**
>
> 如果目标地址原本已经映射到传入的同一个 `struct Page`：
>
> - 不能先调用 `page_remove()` 将其删除。
> - 不能让引用计数先降到零。
> - 不能让该物理页被释放。
> - 应当只更新对应 PTE 的权限。
> - 引用计数应保持不变。
> - 必须考虑 TLB 失效。
>
> **二、不同物理页的替换**
>
> 如果目标地址原本映射到其他物理页：
>
> - 必须正确撤销原有映射。
> - 原物理页引用计数减少一次。
> - 原物理页引用计数归零时，必须正确释放。
> - 新物理页引用计数增加一次。
> - 建立新的有效 PTE。
> - 对相应虚拟地址执行必要的 TLB 失效。
>
> 可以复用已经实现的 `page_remove()`，避免重复编写物理页释放和引用计数递减逻辑。
>
> **三、引用计数**
>
> 必须保证：
>
> - 新增一个有效映射，新物理页引用计数增加一次。
> - 删除旧映射，旧物理页引用计数减少一次。
> - 同一物理页的原映射被更新权限时，引用计数不变。
> - 同一物理页被不同虚拟地址映射时，每个映射都应有对应的引用计数。
> - 不允许出现重复递增、重复递减和引用计数下溢。
>
> **四、内存分配失败**
>
> `get_pte(pgdir, la, 1)` 可能需要分配中间级页表页。
>
> 如果返回 `NULL`：
>
> - 返回 `-E_NO_MEM`。
> - 不修改目标叶子 PTE。
> - 不改变传入物理页的引用计数。
> - 不撤销原有有效映射。
> - 不产生半完成的叶子映射。
>
> 必须阅读 `get_pte()` 的实际实现，检查它在中间级页表分配失败时是否可能留下已经分配的空页表页。
>
> 本次不允许修改 `get_pte()`。如果存在这种情况，请在最终报告中明确说明，不能声称没有任何中间页表资源残留。
>
> **五、TLB 失效**
>
> - 建立新映射后必须考虑 TLB 失效。
> - 更新已有映射权限后也必须考虑 TLB 失效。
> - 替换旧映射时，必须保证旧 TLB 项不会继续访问已经释放的物理页。
> - 必须结合当前项目 `tlb_invalidate()` 的实际实现判断失效范围。
> - 不要擅自引入其他 TLB 管理机制。
>
> # [SPECIFICATION]
>
> 实现：
>
> ```c
> int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm);
> ```
>
> 输入参数：
>
> | 参数      | 含义          |
> | ------- | ----------- |
> | `pgdir` | 需要操作的页目录    |
> | `page`  | 需要映射的物理页描述符 |
> | `la`    | 需要映射的线性地址   |
> | `perm`  | 需要设置的页表权限   |
>
> 返回值：
>
> | 返回值         | 含义               |
> | ----------- | ---------------- |
> | `0`         | 映射建立或更新成功        |
> | `-E_NO_MEM` | 创建所需中间级页表时物理内存不足 |
>
> 函数执行后，应保证目标线性地址映射到传入的物理页，并具有正确的权限和引用计数。
>
> ## #### Pre-Condition
>
> 1. `pgdir` 指向有效的页目录。
> 2. `page` 指向有效的物理页描述符。
> 3. `la` 是当前页表框架允许映射的线性地址。
> 4. `perm` 是符合当前 RISC-V 页表规范的合法权限组合。
> 5. 如果目标地址已经存在有效映射，其物理页描述符和引用计数均处于一致状态。
> 6. 当前实验环境不要求处理多个 hart 同时并发修改同一个页表的情况。
>
> ## #### Post-Condition
>
> ### 情况一：目标地址尚未建立有效映射
>
> 例如：
>
> ```c
> *ptep == 0
> ```
>
> 操作：
>
> 1. 获取或创建目标地址对应的页表项。
> 2. 增加新物理页的引用计数。
> 3. 使用 `pte_create()` 建立新 PTE。
> 4. 确保 PTE 包含 `PTE_V` 和指定的权限。
> 5. 执行适当的 TLB 失效。
>
> 结果：
>
> - `la` 成功映射到 `page`。
> - 新物理页引用计数增加一次。
> - 返回 `0`。
>
> ### 情况二：目标地址已经映射到同一个物理页
>
> 例如：
>
> ```text
> la → page_A
> ```
>
> 本次传入的仍然是：
>
> ```text
> page = page_A
> ```
>
> 操作：
>
> 1. 获取原 PTE 对应的物理页描述符。
> 2. 判断其是否与传入的 `page` 相同。
> 3. 不调用 `page_remove()`。
> 4. 不增加或减少该物理页的引用计数。
> 5. 根据新的 `perm` 更新 PTE。
> 6. 执行 TLB 失效。
>
> 结果：
>
> - `la` 仍然映射到 `page_A`。
> - PTE 权限更新为本次指定的权限。
> - `page_A` 的引用计数保持不变。
> - 返回 `0`。
>
> 特别注意：不能因为重复插入同一个物理页而重复增加引用计数。
>
> ### 情况三：目标地址已经映射到其他物理页
>
> 例如：
>
> 原映射：
>
> ```text
> la → page_A
> ```
>
> 本次需要建立：
>
> ```text
> la → page_B
> ```
>
> 其中：
>
> ```text
> page_A != page_B
> ```
>
> 操作：
>
> 1. 获取原映射对应的 `page_A`。
> 2. 正确撤销 `la → page_A`。
> 3. 将 `page_A` 的引用计数减少一次。
> 4. 如果 `page_A` 的引用计数变成零，释放 `page_A`。
> 5. 将 `page_B` 的引用计数增加一次。
> 6. 建立新的 PTE，使 `la → page_B`。
> 7. 执行必要的 TLB 失效。
>
> 结果：
>
> - `la` 映射到 `page_B`。
> - `page_A` 的引用计数正确减少。
> - `page_B` 的引用计数正确增加。
> - 返回 `0`。
>
> 优先考虑复用已经实现的 `page_remove()`，避免重复实现旧映射删除逻辑。
>
> ### 情况四：同一个物理页被映射到不同虚拟地址
>
> 例如：
>
> ```text
> la_1 → page_A
> ```
>
> 现在需要建立：
>
> ```text
> la_2 → page_A
> ```
>
> 其中：
>
> ```text
> la_1 != la_2
> ```
>
> 操作：
>
> 1. 获取 `la_2` 对应的页表项。
> 2. 增加 `page_A` 的引用计数。
> 3. 建立 `la_2 → page_A` 的映射。
> 4. 执行 TLB 失效。
>
> 结果：
>
> - 两个虚拟地址都映射到 `page_A`。
> - `page_A` 的引用计数增加一次。
> - 返回 `0`。
>
> ### 情况五：目标 PTE 存在，但当前无效
>
> 如果目标 PTE 存在，但没有设置 `PTE_V`：
>
> - 不得调用 `pte2page()` 将其解释为有效物理页。
> - 不得对其中残留的物理页号执行引用计数操作。
> - 按照尚无有效映射的情况建立新映射。
> - 正确增加新物理页的引用计数。
> - 返回 `0`。
>
> ### 情况六：中间级页表分配失败
>
> 当：
>
> ```c
> get_pte(pgdir, la, 1) == NULL
> ```
>
> 操作：
>
> 直接返回：
>
> ```c
> -E_NO_MEM
> ```
>
> 结果：
>
> - 不建立新的叶子映射。
> - 不修改传入物理页的引用计数。
> - 不撤销目标地址已有的有效映射。
> - 不释放调用者传入的物理页。
>
> 如果 `get_pte()` 在失败前可能已经创建部分中间级页表，请在报告中说明实际情况。
>
> # [IMPLEMENTATION AND VERIFICATION]
>
> 请按照以下步骤完成。
>
> ## 第一步：检查当前代码
>
> 先检查：
>
> ```bash
> git status --short --branch
> ```
>
> 确认：
>
> - 当前分支是 `lab4`。
> - 已经存在上一阶段的 `page_remove()` 实现。
> - `page_insert()` 仍然是未实现状态。
> - 没有其他成员正在进行的修改被意外覆盖。
>
> 同时阅读相关函数的实际源码。
>
> 不要立即修改代码。
>
> ## 第二步：分析实现方案
>
> 先解释：
>
> 1. `get_pte(pgdir, la, 1)` 如何获取或创建目标页表项。
> 2. 为什么页表项创建可能失败。
> 3. 如何判断目标 PTE 是否有效。
> 4. 如何通过 `pte2page()` 获取旧映射对应的物理页。
> 5. 如何处理新旧映射指向同一个物理页的情况。
> 6. 如何处理新旧映射指向不同物理页的情况。
> 7. 为什么同一个物理页可以对应多个虚拟地址。
> 8. 如何正确维护物理页引用计数。
> 9. 如何复用 `page_remove()`。
> 10. `tlb_invalidate()` 的实际工作方式以及适用范围。
>
> 特别要求：
>
> 请明确比较以下两种情况：
>
> ```text
> 情况 A：
> 原映射 la → page_A
> 新映射 la → page_A
>
> 情况 B：
> 原映射 la → page_A
> 新映射 la → page_B
> ```
>
> 说明两种情况下引用计数为什么必须采用不同的处理方式。
>
> ## 第三步：实现函数
>
> 只填写 `page_insert()` 的实现位置。
>
> 优先复用现有辅助函数，不重复编写已经实现的逻辑。
>
> 保留 `page_remove()` 和其他函数原样。
>
> ## 第四步：静态检查
>
> 重点检查：
>
> - 新地址建立映射。
> - 同一地址重复插入同一物理页。
> - 同一地址替换成不同物理页。
> - 同一个物理页映射到不同虚拟地址。
> - 更新已有映射的权限。
> - 无效 PTE 的处理。
> - 中间页表分配失败。
> - 新旧物理页引用计数。
> - 物理页释放条件。
> - TLB 失效顺序。
>
> 对照 `check_pgdir()` 和 `tools/test_lab4.c` 检查实现是否满足测试契约。
>
> ## 第五步：编译检查
>
> 使用当前项目已有的 RISC-V 工具链和 Makefile。
>
> 至少完成：
>
> 1. `pmm.c` 单文件编译。
> 2. 必要的 `-Werror` 检查。
> 3. `git diff --check`。
> 4. 检查是否修改了其他文件。
> 5. 检查是否存在重复定义、未声明函数和新增编译警告。
>
> 如果全树链接仍然因为其他成员负责的 Lab4 功能未完成而失败，不要提前实现这些函数来解决问题。
>
> ## 第六步：输出审查报告
>
> 请提供：
>
> 1. 实际修改的代码位置。
> 2. 完整的 `page_insert()` 实现。
> 3. 每种映射情况下的执行逻辑。
> 4. 同一物理页重复映射时的引用计数分析。
> 5. 替换不同物理页时的引用计数分析。
> 6. TLB 失效时机分析。
> 7. `get_pte()` 内存分配失败路径分析。
> 8. 编译检查结果。
> 9. `git diff --stat`。
> 10. `git diff --check`。
> 11. `git status`。
> 12. 完整 diff。
>
> 不要 commit 或 push。
>
> 完成后停止，等待人工审查，不要继续实现其他 Lab4 功能。

<details><summary>4 previous messages</summary>

> 我会先确认上一阶段的 `page_remove()` 仍是当前工作区内容、`page_insert()` 仍为占位，并核对 `get_pte()` 的失败路径和 PTE 权限构造。实现时复用 `page_remove()` 删除不同物理页的旧映射，但同一物理页只更新 PTE 和 TLB，不触碰引用计数。
>
> 方案已确认：`get_pte(..., 1)` 成功后先检查叶子 PTE。若映射同一物理页，直接重写权限并失效 TLB；若映射其他物理页，调用现有 `page_remove()` 完成旧页解绑，再递增新页引用并写入新 PTE；若 PTE 无效，则直接建立新映射。现在只新增 `page_insert()`。
>
> <details><summary>Edited a file</summary>
>
> - Wrote `./code/kern/mm/pmm.c` (+25 -0)
>
> </details>
>
> `page_insert()` 已通过 `-Werror` 编译和静态路径检查。接下来用项目 Makefile 构建 `pmm.o`，再在全树副本中确认 `page_insert/page_remove` 均已解析，剩余链接失败只属于尚未实现的进程与调度部分。
</details>

`page_insert()` 已实现，仅修改了 [pmm.c](./code/kern/mm/pmm.c:351)，没有修改 `page_remove()`、Lab2、Lab3、进程管理、调度或测试代码，也没有 commit 或 push。

## 1. 修改位置

```text
code/kern/mm/pmm.c:351-374
```

函数位于已实现的 `page_remove()` 之后。

## 2. 完整实现

```c
int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
{
    pte_t *ptep = get_pte(pgdir, la, 1);
    if (ptep == NULL)
    {
        return -E_NO_MEM;
    }

    if (*ptep & PTE_V)
    {
        if (pte2page(*ptep) == page)
        {
            *ptep = pte_create(page2ppn(page), perm);
            tlb_invalidate(pgdir, la);
            return 0;
        }
        page_remove(pgdir, la);
    }

    page_ref_inc(page);
    *ptep = pte_create(page2ppn(page), perm);
    tlb_invalidate(pgdir, la);
    return 0;
}
```

## 3. 各映射情况的执行逻辑

### 情况一：目标地址没有有效映射

包括：

- 叶子 PTE 为 0。
- 叶子 PTE 存在但未设置 `PTE_V`。

此时：

1. 不调用 `pte2page()`。
2. `page_ref_inc(page)` 增加一次引用。
3. 使用 `pte_create()` 建立 PTE。
4. 执行 TLB 失效。
5. 返回 0。

`pte_create()` 的定义是：

```c
return (ppn << PTE_PPN_SHIFT) | PTE_V | type;
```

因此生成的 PTE 自动包含 `PTE_V`，同时保留 `perm` 中的 `PTE_U/PTE_R/PTE_W/PTE_X` 等权限。

### 情况二：目标地址已映射到同一物理页

```c
pte2page(*ptep) == page
```

此时：

1. 不调用 `page_remove()`。
2. 不增加引用计数。
3. 不减少引用计数。
4. 直接重写 PTE，只更新权限。
5. 执行 TLB 失效。
6. 返回 0。

这满足测试中“向同一地址重新插入同一页面时，空闲页数量不变”的要求。

### 情况三：目标地址已映射到其他物理页

此时：

1. 调用 `page_remove(pgdir, la)`。
2. `page_remove()` 清除旧 PTE。
3. `page_remove()` 对 `la` 执行 TLB 失效。
4. 旧物理页引用计数减一，归零时释放。
5. 新物理页引用计数加一。
6. 写入指向新物理页的 PTE。
7. 再次执行 TLB 失效，确保新映射立即可见。
8. 返回 0。

复用了上一阶段已经实现的 `page_remove()`，没有重复编写旧页面释放逻辑。

### 情况四：同一物理页映射到不同虚拟地址

目标 `la` 尚无有效映射，因此正常执行：

```text
page_ref_inc(page)
la → page
```

每增加一个有效虚拟地址映射，就增加一次引用计数。

### 情况五：中间级页表分配失败

如果：

```c
get_pte(pgdir, la, 1) == NULL
```

立即返回：

```c
-E_NO_MEM
```

此时：

- 不修改叶子 PTE。
- 不修改传入页面的引用计数。
- 不撤销任何已有叶子映射。
- 不释放传入页面。

## 4. 同一物理页重复映射的引用计数

假设：

```text
la → page_A，ref(page_A) = 1
```

再次执行：

```c
page_insert(pgdir, page_A, la, new_perm);
```

必须保持：

```text
ref(page_A) = 1
```

因为这是同一个虚拟地址的权限更新，不是新的独立映射。

如果错误地先 `page_remove()` 再 `page_insert()` 式地增加引用，可能会出现：

- 页面先被错误释放。
- 引用计数短暂变为零。
- 页面被错误加入空闲链表。
- 相同映射被重复计入两次。

当前实现通过先比较 `pte2page(*ptep) == page` 避免了该问题。

## 5. 替换不同物理页的引用计数

假设：

```text
la → page_A，ref(page_A) = 1
ref(page_B) = 0
```

执行：

```c
page_insert(pgdir, page_B, la, perm);
```

变化为：

```text
page_remove:
    page_A.ref: 1 → 0
    page_A 被释放

page_insert:
    page_B.ref: 0 → 1
    la → page_B
```

如果 `page_A` 还被其他虚拟地址引用：

```text
page_A.ref: 2 → 1
```

此时不会释放 `page_A`。

## 6. TLB 失效时机

当前 [tlb_invalidate()](./code/kern/mm/pmm.c:378) 实际执行：

```c
asm volatile("sfence.vma %0" : : "r"(la));
```

行为是：

- 按虚拟地址 `la` 失效当前 hart 的相关 TLB 项。
- 不使用 `pgdir` 参数。
- 不涉及 ASID。
- 不向其他 hart 广播。

各路径的失效时机：

| 路径 | TLB 失效 |
|---|---|
| 新映射 | 写入新 PTE 后失效 |
| 同一页面更新权限 | 更新权限 PTE 后失效 |
| 替换不同页面 | `page_remove()` 清除旧 PTE 后失效一次，新 PTE 写入后再失效一次 |

替换路径执行两次失效是有意的：

1. 第一次确保旧物理页在被释放前没有旧 TLB 映射。
2. 第二次确保新建立的映射立即可见。

## 7. get_pte() 内存分配失败分析

当前 `get_pte()` 最多分配两级中间页表。

如果最高级页表不存在：

1. 分配第一张中间页表。
2. 初始化并写入 `pgdir[PDX1(la)]`。
3. 再检查下一级页表。

如果下一级分配失败，会直接返回 `NULL`，但已经完成的第一张中间页表不会回滚。

因此存在以下实际行为：

```text
page_insert 返回 -E_NO_MEM
但可能残留一张已经分配、已挂入页目录的空中间页表
```

后续再次调用时，`get_pte()` 可以复用这张页表，因此不是持续无限泄漏；但单次失败不是完全原子的。

叶子 PTE 不会被修改，传入页面的引用计数不会改变。本次按要求没有修改 `get_pte()`。

## 8. 测试契约核对

已对照 [check_pgdir()](./code/kern/mm/pmm.c:391) 和 [test_lab4.c](./code/tools/test_lab4.c:13)：

- 新映射后引用计数为 1：满足。
- 同地址替换到另一物理页：旧页引用减少，新页引用增加。
- 同一页面映射到不同虚拟地址：引用增加。
- 同地址重复插入同一页面：引用保持不变，空闲页数量不变。
- 权限字段由 `perm` 正确写入。
- 无效 PTE 不会被 `pte2page()` 解释。
- 中间页表分配失败返回 `-E_NO_MEM`。

## 9. 编译检查

RISC-V 交叉编译器显式 `-Werror`：

```text
PMM_WERROR_BUILD_OK
```

项目 Makefile 单目标构建：

```text
+ cc kern/mm/pmm.c
MAKEFILE_PMM_TARGET_OK
```

符号检查：

```text
T page_insert
T page_remove
T tlb_invalidate
```

`pmm.o` 已没有对 `page_insert/page_remove` 的未解析引用。

全树 `make -k` 已通过所有源文件编译，链接仅剩：

```text
proc_init
cpu_idle
```

这两个符号属于尚未实现的 Lab4 进程和调度模块，本次没有补齐。

`kmalloc.c` 中存在仓库既有的 `kva2page()` 整数转指针警告，本次没有修改。

## 10. git diff --stat

```text
 code/kern/mm/pmm.c | 38 +++++++++++++++++++++++++++++++++++++-
 1 file changed, 37 insertions(+), 1 deletion(-)
```

该 diff 同时包含上一阶段尚未提交的 `page_remove()` 和本次新增的 `page_insert()`。

## 11. git diff --check

无输出，检查通过。

## 12. git status

```text
## lab4...origin/lab4
 M code/kern/mm/pmm.c
```

只修改了 `pmm.c`，没有修改测试、Lab2、Lab3、进程或调度代码。

## 13. 完整 diff

```diff
diff --git a/code/kern/mm/pmm.c b/code/kern/mm/pmm.c
index 8bd4729..cbcbdd3 100644
--- a/code/kern/mm/pmm.c
+++ b/code/kern/mm/pmm.c
@@ -330,12 +330,48 @@ struct Page *get_page(pde_t *pgdir, uintptr_t la, pte_t **ptep_store)
 }

 //LAB4 function1 YOUR CODE
+void page_remove(pde_t *pgdir, uintptr_t la)
+{
+    pte_t *ptep = get_pte(pgdir, la, 0);
+    if (ptep == NULL || !(*ptep & PTE_V))
+    {
+        return;
+    }

+    struct Page *page = pte2page(*ptep);
+    *ptep = 0;
+    tlb_invalidate(pgdir, la);

+    if (page_ref_dec(page) == 0)
+    {
+        free_page(page);
+    }
+}

+int page_insert(pde_t *pgdir, struct Page *page, uintptr_t la, uint32_t perm)
+{
+    pte_t *ptep = get_pte(pgdir, la, 1);
+    if (ptep == NULL)
+    {
+        return -E_NO_MEM;
+    }

+    if (*ptep & PTE_V)
+    {
+        if (pte2page(*ptep) == page)
+        {
+            *ptep = pte_create(page2ppn(page), perm);
+            tlb_invalidate(pgdir, la);
+            return 0;
+        }
+        page_remove(pgdir, la);
+    }

-
+    page_ref_inc(page);
+    *ptep = pte_create(page2ppn(page), perm);
+    tlb_invalidate(pgdir, la);
+    return 0;
+}

 // invalidate a TLB entry, but only if the page tables being
 // edited are the ones currently in use by the processor.
```

当前修改等待人工审查，没有继续实现 `pgdir_alloc_page()`、`print_pgdir()` 或进程管理功能。


# Lab4 prompt 成员2：张祎然 2412419
## 功能模块二：进程创建及初始化

**成员：** 张祎然（2412419，2号）

以下按本次实际对话的先后顺序，配对整理本人提示词与 AI 回复摘要，保留任务、依赖、约束、实现建议和检查结论。

**AI 工具：** Codex（桌面端）；GPT-6。

实际协作方式为：先阅读当前仓库，再逐个分析函数；本人在 VS Code 中手动编写，保存后交由 AI 只读检查；编译、GDB 和评分结果以实际执行输出为准。

### 1. 只读分析整个模块

**本人提示词：**

```text
[PROMPT]
我正在完成 uCore OS Lab4：进程管理实验，
负责功能模块二“进程创建及初始化”。
本次先阅读项目源码，分析已有代码和需要实现的部分，暂时不要修改代码。

我负责 proc.c 中的六个函数：
alloc_proc()、proc_init()、kernel_thread()、do_fork()、
copy_thread()、copy_mm()。

[RELY]
请阅读当前项目的 proc.c、proc.h，以及必要的内存管理、
trapframe、上下文切换和调度相关代码，理解已有数据结构及辅助函数。
以当前 Lab4 项目的实际代码为准，不直接套用其他版本的 uCore 实现。

[GUARANTEE]
1. 本轮只分析，不修改代码。
2. 不修改已有 Lab2、Lab3 功能以及其他组员的页操作、调度函数。
3. 不修改函数签名、结构体和已有接口。
4. 不直接生成六个函数的完整实现。
5. 后续按一个函数一个函数的方式讨论和实现。

[SPECIFICATION]
检查六个函数当前的实现状态，解释各自作用、依赖与调用关系；
梳理从 proc_init() 到 initproc 创建完成的流程；
推荐实现顺序，指出 PCB、PID、内核栈、trapframe 和 context 的注意事项。
完成分析后停止，等待我确认，再开始第一个函数。
```

据此确定实现顺序为 `alloc_proc → copy_mm → copy_thread → do_fork → kernel_thread → proc_init`。

**AI 回复摘要：**

AI 先根据当前仓库梳理了创建流程：`proc_init()` 建立 `idleproc`，再通过 `kernel_thread()` 和 `do_fork()` 创建 `initproc`。`do_fork()` 依赖 PCB 初始化、地址空间处理和首次运行现场构造，因此建议按 `alloc_proc → copy_mm → copy_thread → do_fork → kernel_thread → proc_init` 的顺序实现。

同时明确本模块的边界：只实现内核线程创建和初始化；页映射由模块一负责，选择线程及切换 CPU 现场由模块三负责。本阶段 `mm == NULL`，但线程仍使用已有的内核页表。阅读 `switch_to()` 和 Trap 恢复汇编，是为了保证创建时准备的字段能被后续正确使用。

### 2. alloc_proc：建议实现与逐字段检查

**本人提示词：**

```text
[PROMPT]
现在开始功能模块二的第一个函数 alloc_proc()。
请结合当前项目源码分析实现方案，并给出建议代码，暂时不要修改文件。

[RELY]
重点阅读 proc.c、proc.h、struct proc_struct 的实际定义，
以及已有 kmalloc()、memset()、list_init() 等接口。

[GUARANTEE]
只讨论 alloc_proc()，不实现其他五个函数；
不修改其他组员代码、结构体、函数签名和辅助接口；
只分析并提供代码，不直接写入文件。

[SPECIFICATION]
1. 解释函数的作用及其在进程创建中的位置。
2. 逐个说明 proc_struct 成员及初始值。
3. 解释为什么使用 PROC_UNINIT 和 pid = -1。
4. 说明内存分配失败如何处理。
5. 给出符合当前项目的完整函数实现并解释关键语句。
6. 指出代码在 proc.c 中的插入位置。

alloc_proc() 只负责 PCB 分配与初始化，
不负责分配内核栈、正式 PID 或创建可运行线程。
完成后停止，等待我确认。
```

手动完成后，检查请求集中在全部字段、分配失败处理、两个链表节点、与后续调用者的衔接及 Git diff，并尝试编译。没有让 AI 顺带实现其他函数。

**AI 回复摘要：**

建议先调用 `kmalloc(sizeof(struct proc_struct))`，失败立即返回 `NULL`，成功后用 `memset()` 清零整个 PCB，再覆盖有特殊初值的成员：

| 成员 | AI 建议的初值及原因 |
|---|---|
| `state` | `PROC_UNINIT`，表示资源尚未准备完成 |
| `pid` | `-1`，表示正式 PID 尚未分配 |
| `pgdir` | `boot_pgdir_pa`，使用内核页表根的物理地址 |
| `list_link`、`hash_link` | 分别调用 `list_init()`，使前后指针指向节点自身 |
| `runs`、`kstack`、`need_resched`、`flags` | 由整体清零得到 0 |
| `parent`、`mm`、`tf` | 由整体清零得到空指针 |
| `context`、`name` | 整体清零，等待后续填写 |

AI 强调不要在这里分配内核栈、取得正式 PID 或登记到全局链表。本人手动完成后，复核了全部 14 个成员、分配失败路径和链表初始化，并检查与 `proc_init()`、`do_fork()` 的衔接；本函数通过源码与单文件编译检查。

### 3. copy_mm：限定本实验的地址空间语义

**本人提示词：**

```text
[PROMPT]
现在开始第二个函数 copy_mm()，alloc_proc() 已完成并通过检查。
请阅读 proc.c、proc.h 及相关内存管理代码，分析该函数的实现。

[GUARANTEE]
本轮只讨论和提供代码，不修改文件，不实现其他函数。
我会自己手动编写，完成后再让你检查。

[SPECIFICATION]
1. 解释 copy_mm() 的作用以及与 do_fork() 的关系。
2. 根据实际源码解释 mm、pgdir 和 CLONE_VM。
3. 本实验主要创建内核线程，分析是否需要复制地址空间，
   以及如何处理 mm == NULL。
4. 说明参数、返回值和边界情况。
5. 给出符合当前项目的完整 copy_mm() 代码，不添加代码注释。
6. 告诉我代码应插入 proc.c 的哪个位置。
```

完成后的检查要求为：核对签名、断言和返回值；确认父子 `mm == NULL` 的处理及 `alloc_proc()` 设置的页表地址；检查 Git diff 并进行单文件编译。特别要求不更换 QEMU 环境。

**AI 回复摘要：**

`mm` 描述用户虚拟地址空间，`pgdir` 保存页表根的物理地址，`CLONE_VM` 表达共享地址空间的创建语义。本次父子都是内核线程，双方 `mm` 均为空，`alloc_proc()` 已设置共享的内核页表，因此无需分配 `mm_struct`、复制 VMA 或新建页表。

当前实现保留 `clone_flags` 参数并用 `(void)clone_flags` 显式标记未使用；断言 `current` 和 `proc` 非空、父子 `mm == NULL`，然后返回 0。`do_fork()` 根据返回值继续创建流程或进入失败清理。非空 `mm` 超出了本实验的实现范围，不能把直接返回 0 解释为已经支持用户地址空间复制。

本人完成后，AI 核对了函数签名、三条断言及成功返回值，确认与 `alloc_proc()` 的初始化一致，并通过单文件编译检查。

### 4. copy_thread：使用 TODO 框架学习现场构造

**本人提示词：**

```text
[PROMPT]
现在开始第三个函数 copy_thread()，前两个函数已完成并通过检查。
请阅读当前 Lab4 源码，帮助我理解并完成该函数。

[GUARANTEE]
只提供分析和代码框架，不修改任何文件。
我会自己补全代码，再交给你检查。

[SPECIFICATION]
1. 解释函数作用，以及 trapframe 和 context 的区别。
2. 根据当前源码说明内核栈、寄存器和首次运行上下文如何设置。
3. 给出完整函数框架，保留函数签名和必要控制结构。
4. 需要我实现的关键语句用 TODO 标记，并说明每个 TODO 的任务，
   不直接给出全部答案。
5. 结合 switch_to()、forkret() 和 entry.S 解释首次运行流程。
6. 框架必须符合当前项目的结构体成员和已有接口，不套用其他版本。
```

补全后的专项检查请求为：

```text
请只检查已经手动完成的 copy_thread()：
1. trapframe 的存放位置和复制方式是否正确；
2. gpr.a0、gpr.sp 是否符合本项目约定；
3. esp == 0 时用 proc->tf 作为栈指针是否正确；
4. context.ra、context.sp 能否衔接 switch_to、forkret 和 __trapret；
5. 是否有栈对齐、栈越界和寄存器恢复问题；
6. 检查 Git diff 并尝试单文件编译。
不要修改代码，不补全其他函数，发现问题说明原因和修改方法。
```

**AI 回复摘要：**

AI 先解释两种现场的用途：`context` 保存 `switch_to()` 需要恢复的 `ra`、`sp` 和被调用者保存寄存器；`trapframe` 保存通用寄存器、状态和入口地址，供 `__trapret` 恢复。随后按下面的任务拆分 TODO，由本人补全语句：

| TODO 任务 | 实现思路 |
|---|---|
| 在新线程栈内放置中断帧 | 使用内核栈上界减去 `sizeof(struct trapframe)`，不能把临时参数地址直接留在 PCB 中 |
| 复制中断帧 | 按结构体内容复制，使新线程拥有自己的现场 |
| 设置返回值和栈指针 | 子线程 `a0` 为 0；`esp` 非零时采用给定栈，零时按当前项目约定使用 `proc->tf` |
| 构造首次切换上下文 | `context.ra` 指向 `forkret`，`context.sp` 指向新线程的中断帧 |

补全后的审查确认：当前 RV64 中断帧为 288 字节，位于 8192 字节内核栈的顶部区域，起始地址保持 16 字节对齐；帧内容已复制，未保留调用者局部变量的地址。`context` 能衔接 `switch_to → forkret → forkrets → __trapret`，最终由 `sret` 进入中断帧指定的入口。本函数通过单文件编译检查。

### 5. do_fork：资源准备与全局登记分开审查

**本人提示词：**

```text
[PROMPT]
现在开始第四个函数 do_fork()，前三个辅助函数均已完成并通过检查。
请结合当前源码分析实现，并提供代码框架。

[RELY]
阅读已有 setup_kstack()、put_kstack()、get_pid()、
hash_proc()、wakeup_proc() 等辅助函数。

[GUARANTEE]
只分析和提供框架，不修改文件，不实现其他函数，
不修改其他组员代码。我会自己补全 TODO。

[SPECIFICATION]
1. 解释作用、参数、返回值及与前三个函数的调用关系。
2. 按实际顺序分析 PCB 分配、内核栈分配、地址空间处理、
   上下文复制、PID 分配、链表登记和唤醒。
3. 重点解释中断保护范围及分配失败时的资源释放。
4. 给出完整函数框架，保留签名和控制结构，关键位置标记 TODO，
   不直接给出完整答案。
5. 逐个解释 TODO 和易错点，指出代码的插入位置。
```

先完成资源准备部分后，提出分阶段检查：

```text
我已完成 do_fork() 的 TODO 1～6，TODO 7～10 尚未完成。
只检查进程上限判断、alloc_proc/setup_kstack/copy_mm/copy_thread
的调用顺序和参数、父进程设置、错误码及失败跳转目标。
不要求整个函数现在能编译运行，不修改代码，不补全 TODO 7～10。
检查后再指导剩余部分的框架与思路。
```

完成 TODO 7～10 后，再审查完整执行顺序、PID 唯一性、哈希表和进程链表登记、进程计数、中断保存与恢复、唤醒时机和所有清理标签，并进行单文件编译。期间发现磁盘文件仍是旧的占位版本，保存编辑器中的文件后重新读取核对。

**AI 回复摘要：**

AI 将实现分成资源准备和全局登记两个阶段。前者依次检查数量上限、分配 PCB、设置父进程、分配内核栈、处理地址空间并复制上下文；后者在保存并关闭本地中断后，完成 PID 分配、哈希表登记、进程链表插入、计数增加和唤醒，最后恢复原中断状态。

| 失败点 | 返回值与清理思路 |
|---|---|
| 达到 `MAX_PROCESS` | 返回 `-E_NO_FREE_PROC`，不释放尚未取得的资源 |
| PCB 分配失败 | 返回 `-E_NO_MEM` |
| 内核栈分配失败 | 保留错误码，只释放已分配的 PCB |
| 地址空间处理失败 | 保留错误码，先释放内核栈，再释放 PCB |

AI 提醒：不能先唤醒再补齐资源；不能把尚未登记的进程计入总数；各失败标签只释放已经成功取得的资源。关闭本地中断保护的是本实验单核下的全局结构更新，不能作为多核互斥锁使用。

第一次读取时，磁盘上的 `do_fork()` 仍为占位内容，因此没有直接判定 TODO 1～6 正确，而是要求先保存再核对。本人保存并完成剩余 TODO 后，复核确认 `get_pid → hash_proc → list_add_before → nr_process++ → wakeup_proc` 的顺序正确，成功返回 PID，失败清理没有重复释放，本函数通过单文件编译检查。

### 6. kernel_thread：解释寄存器与状态位

**本人提示词：**

```text
[PROMPT]
现在开始第五个函数 kernel_thread()，前面四个函数均已完成并通过检查。
请结合当前项目分析实现，并提供代码框架。

[RELY]
阅读 proc.c、proc.h、trap.h、entry.S 及相关汇编代码。

[GUARANTEE]
只分析和提供框架，不修改文件，不实现其他函数。
我会自己补全 TODO，再交给你检查。

[SPECIFICATION]
1. 解释函数作用、参数、返回值及与 do_fork() 的关系。
2. 说明为什么构造临时 trapframe，以及如何初始化。
3. 重点解释 gpr.s0、gpr.s1、status、epc 的设置，
   以及 SPP、SPIE、SIE 的含义。
4. 解释如何调用 do_fork()，以及为什么使用 CLONE_VM。
5. 给出保留函数签名的完整框架，关键语句用 TODO 标记，
   逐个说明填写方法，不直接给出完整答案。
6. 指出代码在 proc.c 中的位置。
```

补全后的检查重点为：中断帧是否整体清零、`s0/s1` 是否对应 `entry.S`、`SPP/SPIE/SIE` 是否为预期状态、`epc` 是否指向正确入口、`do_fork()` 的参数与返回值是否正确，以及局部 `tf` 是否被及时复制。随后核对 Git diff 并进行单文件编译。

**AI 回复摘要：**

临时 `trapframe` 是新线程第一次执行的恢复模板。AI 要求先整体清零，再按 `entry.S` 的约定逐项填写：

| 字段 | 设置思路与原因 |
|---|---|
| `gpr.s0` | 保存线程函数 `fn`；入口汇编通过 `jalr s0` 调用它 |
| `gpr.s1` | 保存参数 `arg`；入口汇编先执行 `move a0, s1` |
| `status.SPP` | 置 1，使 `sret` 返回 S 态 |
| `status.SPIE` | 置 1，使 `sret` 后的 S 态中断使能位为 1 |
| `status.SIE` | 清 0，在恢复现场期间关闭 S 态中断；保留其他状态位 |
| `epc` | 指向 `kernel_thread_entry`，由该入口调用实际线程函数 |

随后将 `clone_flags | CLONE_VM`、栈参数 0 和临时帧地址传给 `do_fork()`，直接返回其 PID 或负错误码。这里使用共享地址空间标志表达内核线程的语义；当前 `copy_mm()` 仍只处理空 `mm`。

本人补全后，AI 核对确认各寄存器和状态位正确，且 `copy_thread()` 在创建流程中已把局部 `tf` 复制到新内核栈，不存在函数返回后引用失效的问题。源码检查、Git diff 检查和单文件编译通过；当时尚未据此宣称完整内核运行通过。

### 7. proc_init：以测试断言约束初始化结果

**本人提示词：**

```text
[PROMPT]
现在开始最后一个函数 proc_init()，前五个函数已完成并通过单文件编译。
请结合当前项目分析实现，并提供代码框架。

[RELY]
阅读 proc.c、proc.h、init.c 和相关测试代码，确认初始化要求。

[GUARANTEE]
只分析和提供框架，不修改文件，不修改其他组员代码。
我会自己补全 TODO，之后再交给你检查。

[SPECIFICATION]
1. 解释函数作用和整体执行流程。
2. 说明 proc_list、hash_list、nr_process 如何初始化。
3. 解释如何通过 alloc_proc() 创建 idleproc，
   并设置 PID、状态、内核栈、名称和 current。
4. 解释如何通过 kernel_thread() 创建 initproc，
   如何查找 PCB、设置名称并检查创建结果。
5. 说明 idleproc、initproc 的区别及其与后续调度的关系。
6. 给出保留签名的完整 TODO 框架，逐个解释填写方法，
   不直接给出完整答案，并指出插入位置。
```

六函数完成后，要求对照 `tools/test_lab4.c` 和 `grade.sh` 检查整体衔接，并明确区分单文件编译、完整链接和 QEMU 运行结果。若失败，必须指出具体位置、原因和所属模块，不擅自修复其他组员代码。

**AI 回复摘要：**

AI 给出的框架以初始化顺序为重点：

1. 初始化 `proc_list` 和每个 `hash_list` 桶，令 `nr_process = 0`。
2. 调用 `alloc_proc()` 创建 `idleproc`，分配失败立即 `panic`。
3. 设置 idle 的 PID 为 0、状态为 `PROC_RUNNABLE`、内核栈为已有 `bootstack`、调度标志为 1，并命名为 `idle`。
4. 先设置 `current = idleproc` 并计入进程总数，再创建 init，保证新线程的父进程正确。
5. 调用 `kernel_thread(init_main, "Hello world!!", 0)`，检查返回的 PID；用 `find_proc()` 找到 PCB，检查非空后命名为 `init`。
6. 用断言核对当前线程、两个 PID、父子关系、init 状态及进程总数。

AI 解释了 idle 直接接管启动执行流，复用启动栈，不进入普通进程链表和 PID 哈希表，但计入总数；init 通过完整创建流程取得自己的内核栈，登记后等待调度。对于当前固定长度复制名称的 `set_proc_name()`，最终实现使用 `PROC_NAME_LEN + 1` 的名称数组。

本人完成后，复核确认六个函数之间的调用和初始化衔接正确。单文件编译通过，但最初完整构建遇到其他已有文件的类型诊断，因此 AI 将“模块二检查通过”与“完整构建尚未通过”分别报告。

### 8. 只读审查的共同要求

以下是上述各轮检查要求的归纳，不是一次性替代所有专项检查的原始提示词：

```text
我已手动完成本轮函数，请读取保存后的当前仓库源码检查。
核对函数签名、前置条件、返回值、初始化、调用顺序、
资源生命周期，以及与已完成函数的衔接。
检查 Git diff，确认没有误改其他函数，并尝试适当的编译验证。
只检查，不修改文件，不补全其他函数，不更换实验环境。
发现问题请指出位置、原因和修改方法；
没有实际运行的测试不要声称通过，最后给出简洁结论。
```

**AI 回复摘要：**

AI 的复核按“结论、具体依据、验证范围”组织：先核对磁盘上的实际实现，再结合辅助函数和汇编检查调用关系，最后检查 Git diff 并给出编译结果。仅当实际执行过对应验证时，才记录“通过”。

单文件编译用于检查当前源文件能否生成目标文件；完整编译与链接用于验证模块之间的符号和接口；QEMU 与 GDB 用于观察实际执行状态。对仅靠阅读分析的失败分支，保留“源码检查”的表述，不将其写成已经执行过的运行测试。

### 9. 构建问题排查与测试边界

完整构建被 `kmalloc.c` 和 `test_lab4.c` 的类型诊断阻断后，先要求核实错误是否与 GCC 15.1.0 的检查有关，并核对 Makefile 的构建与测试入口。早期曾允许在外部临时副本做最小修复诊断，但后续明确要求实验回到原仓库、源码由本人手动编辑、不得主动创建项目副本，不把那些诊断副本作为最终提交。

最终采用的原仓库约束整理如下：

```text
接下来所有 Lab4 实验直接基于原仓库：
D:/HuaweiMoveData/Users/zhangyiran/Desktop/lab4/OS-Lab-lab4。
源码修改由我在 VS Code 中完成，未经同意不得修改源码或其他组员代码。
Codex 负责分析、检查、编译和测试；测试临时文件尽量放在仓库外。
教师原有的 kmalloc.c 和 test_lab4.c 保持不变。
本机已有 QEMU 4.1.1，使用该版本，由我手动输入命令调试。
```

实际使用命令行的 `-Wno-error=int-conversion` 和 `-Wno-error=incompatible-pointer-types` 降低两类已有类型诊断的错误级别，保留警告。没有把这种兼容处理记录成修复了源代码中的类型问题。

手动调试按三个阶段进行：先在 `proc_init()` 返回后检查两个 PCB，再在 `init_main()` 入口检查当前线程和参数，最后在 `do_exit()` 入口及 QEMU 控制台核对返回值和输出。具体命令和原始截图见 [`report.md` 的 5.2 节](report.md#52-2号成员测试与验证)。

**AI 回复摘要：**

AI 将构建问题定位到两处已有类型不匹配：`kmalloc.c` 将整数形式的内核地址传给接受 `void *` 的 `kva2page()`；`test_lab4.c` 的测试线程返回 `void`，而 `kernel_thread()` 接受返回 `int` 的函数。GCC 15.1.0 默认将相应诊断作为错误。采用命令行降低这两类诊断级别后完成验证，同时明确这不等于修复类型问题。

手动调试时，AI 根据截图逐步分析：

| 调试阶段 | 观察到的结果与反馈 |
|---|---|
| 命中 `proc_init()` | 断点显示在 `libs/list.h:42`，与优化后内联的 `list_init()` 对应 |
| `proc_init()` 返回后 | `current == idleproc`，进程数为 2；idle/init 的 PID 为 0/1，状态、父进程、名称及空 `mm` 符合预期 |
| 进入 `init_main()` | `current == initproc`、PID 为 1、运行次数为 1，参数为 `Hello world!!`，说明首次运行和参数传递成功 |
| 进入 `do_exit()` | `error_code == 0`，说明入口函数正常返回；当前 `panic` 是退出接口的占位行为，不等于实现了完整资源回收 |

这些反馈帮助本人把“PCB 创建完成”“调度后开始运行”和“线程函数返回”三个阶段分别验证。

### 10. 全组自动评分的最终授权与实际结果

在模块二手动调试通过后，本人提出对整个小组运行测试。确认原 Makefile 的 `make grade` 会自动在 `/tmp/lab4_grade` 创建评分副本、注入测试，并在成功后删除后，最终明确授权：

```text
直接从原仓库运行 make grade。
```

这次授权只用于仓库原有评分流程，执行入口仍为原仓库 `code` 目录，原源码未修改。本人手动运行指定 GCC 参数和 QEMU 4.1.1 的命令，三项测试均通过，总分为 **30/30**，评分退出码为 **0**。

实际评分截图保存在 [`images/m2-07-grade-30-of-30.png`](images/m2-07-grade-30-of-30.png)。评分后确认临时目录已删除、原 `proc.c` 校验值未变。该结论仅表示通过当前仓库的测试集合，不包含 Challenge 文字题或未执行的故障注入、压力测试。

**AI 回复摘要：**

AI 核对截图后确认，页操作、进程创建和调度三项均为 `OK`，总分为 30/30，评分退出码为 0。结合前面的断点与字段检查，可以记录模块二已通过现有的创建、首次运行及集成测试。

评分结论以当前测试集合为限，不代表完成了尚未实现的用户地址空间复制、退出回收或其他扩展功能。Challenge 是另外的文字分析任务，不由这次自动评分验证；按小组分工，2号成员负责六个创建初始化函数和对应报告，没有额外分配 Challenge 题目。


---

# Lab4 prompt 成员3：张旗庭 2413788

**负责模块：** 进程调度（`cpu_idle()`、`schedule()`、`proc_run()`、`switch_to()`）

**AI 工具：** ChatGPT（GPT-6）+ Codex（VS Code）

本次实验主要使用 ChatGPT 辅助理解实验要求、分析源码和解决实现过程中的问题，再将整理后的要求交给 Codex 辅助编写代码。本人负责检查 AI 提供的实现，结合项目源码理解关键逻辑，并通过编译和运行进行验证。

注：三轮具体实现提示词已经在实验报告 4.3.4 节中记录，本文件主要整理与 ChatGPT 的关键讨论及问题分析过程，不再重复提示词正文。

---

## 一、理解项目与明确任务

### 1. Lab4 整体功能与各模块之间的关系

**本人提出的问题：**

开始编写报告时，我对整个项目的理解还不够完整，不清楚三个成员负责的工作之间有什么联系，也不清楚自己实现的进程调度功能在整个操作系统中发挥什么作用。希望结合实验指导书、项目源码和已有报告，梳理各模块之间的关系。

**ChatGPT 协助分析：**

首先从整个 Lab4 的目标出发，将实验划分为三个相互衔接的功能模块：

- 虚拟内存页操作：负责建立和删除虚拟地址与物理页面之间的映射，为内核内存管理提供基础。
- 进程创建及初始化：负责创建进程控制块、分配内核栈、构造执行上下文，并创建 `idleproc` 和 `initproc`。
- 进程调度：在已经创建好的线程中选择可运行线程，并完成 CPU 执行现场的切换。

进一步梳理出整体执行流程：

```text
内存管理初始化
      |
      v
进程初始化 proc_init()
      |
      +----------+
      |          |
      v          v
   idleproc   initproc
      |
      v
   cpu_idle()
      |
      v
   schedule()
      |
      v
   proc_run()
      |
      v
   switch_to()
      |
      v
目标线程获得 CPU 执行权
```

由此明确，进程创建只是为线程准备好运行所需的资源和执行现场，而调度模块负责让这些线程真正获得 CPU 执行机会。

### 2. 四个核心函数之间的关系

**本人提出的问题：**

我进一步询问了四个函数分别负责什么，它们之间是怎样连接的，以及为什么需要分别在不同文件中实现。

**ChatGPT 协助分析：**

将四个函数按照执行过程划分为不同层次：

- `cpu_idle()`：空闲线程的执行入口，持续检查是否需要重新调度。
- `schedule()`：根据进程状态和链表顺序，选择下一个可运行线程。
- `proc_run()`：更新当前进程，完成页表切换，并调用底层上下文切换函数。
- `switch_to()`：使用 RISC-V 汇编保存原线程的寄存器现场，并恢复目标线程的寄存器现场。

四个函数并不是相互独立的，而是形成了从调度请求到执行权转移的完整过程。

### 3. 实现所依据的源码、数据结构和接口

**本人提出的问题：**

由于对项目源码不熟悉，我希望明确实现这些函数时应该依据哪些已有接口和数据结构，避免自行设计与框架不一致的实现。

**ChatGPT 协助分析：**

结合项目目录，确定了需要重点阅读的文件：

| 文件 | 主要内容 |
|---|---|
| `kern/process/proc.c` | `cpu_idle()`、`proc_run()` 等进程管理函数 |
| `kern/process/proc.h` | `proc_struct`、`context` 等数据结构 |
| `kern/schedule/sched.c` | `schedule()` 的实现 |
| `kern/schedule/sched.h` | 调度模块相关接口 |
| `kern/process/switch.S` | 底层汇编函数 `switch_to()` |
| `libs/list.h` | 双向链表操作接口 |
| `kern/sync/sync.h` | 中断状态保存与恢复宏 |
| `libs/riscv.h` | RISC-V CSR 寄存器操作接口 |

同时重点解释了调度过程中需要使用的几个成员：

- `current`：指向当前正在执行的线程。
- `idleproc`：空闲线程，在没有其他可运行线程时使用。
- `proc_list`：组织普通进程的全局链表。
- `state`：记录进程状态，调度器通过 `PROC_RUNNABLE` 判断线程是否可以执行。
- `need_resched`：标记是否需要重新调度。
- `runs`：记录线程被调度的次数。
- `pgdir`：保存页表根节点的物理地址。
- `context`：保存线程切换所需的寄存器现场。

通过这些分析，明确了本模块主要是在已有进程管理框架上实现调度功能，而不是重新设计进程管理机制。

---

## 二、具体实现与关键问题

### 1. 空闲线程与可运行线程的选择

**本人提出的问题：**

希望理解 `cpu_idle()` 和 `schedule()` 的具体实现逻辑，尤其是 `proc_list` 的遍历方式、`idleproc` 的特殊处理，以及如何判断一个线程能否参与调度。

**ChatGPT 协助分析：**

首先解释了 `cpu_idle()` 的作用：通过无限循环维持空闲线程运行，并根据 `current->need_resched` 决定是否调用 `schedule()`。

对于 `schedule()`，重点分析了链表遍历过程：

- 当前线程是 `idleproc` 时，从普通进程链表的头部开始寻找。
- 当前线程不是 `idleproc` 时，从当前节点的下一个节点开始寻找。
- 遍历时跳过链表头节点，只选择 `PROC_RUNNABLE` 状态的线程。
- 如果遍历一圈仍然没有找到其他可运行线程，则选择 `idleproc`。
- 选择完成后更新 `runs`，并在目标线程与当前线程不同时调用 `proc_run()`。

此外，还解释了 `need_resched` 为什么需要清零，以及中断保护在链表遍历和进程选择过程中的作用。

### 2. 当前线程切换与页表切换

**本人提出的问题：**

在实现 `proc_run()` 时，我对 `pgdir`、`satp` 和 TLB 的关系不太理解，尤其是不清楚为什么要对页表根地址进行移位，以及页表切换之后为什么还要刷新 TLB。

**ChatGPT 协助分析：**

结合项目中的 RISC-V Sv39 分页机制，解释了 `satp` 寄存器的组成：

- MODE：设置为 8，表示使用 Sv39 分页模式。
- ASID：本实验设置为 0。
- PPN：由页表根节点的物理地址右移 `RISCV_PGSHIFT` 得到。

因此，需要将页表根节点的物理页号与 MODE 等字段组合后写入 `satp`。

同时解释了 `flush_tlb()` 的作用：页表切换后，使缓存的地址转换信息失效，避免继续使用旧的地址转换结果。

另外，结合 `switch_to()` 的调用过程，解释了为什么 `proc_run()` 中的代码并不是执行到上下文切换后就立即返回，而是需要等原线程将来重新获得 CPU 执行权，才能继续执行后续代码。

### 3. 底层上下文切换与寄存器保存

**本人提出的问题：**

对于 `switch_to()` 中的汇编代码，我希望理解为什么只保存部分寄存器，以及这些寄存器对应的内存偏移是如何确定的。

**ChatGPT 协助分析：**

结合 `proc.h` 中 `struct context` 的定义和 RISC-V RV64 调用约定，解释了上下文切换需要保存的寄存器：

`ra`、`sp` 和 `s0~s11`。

由于 RV64 中每个寄存器占用 8 字节，因此可以按照结构体成员的排列顺序确定各寄存器的内存偏移。

进一步解释了汇编指令的作用：

- `sd`：将当前线程的寄存器值保存到原线程的 `context` 中。
- `ld`：从目标线程的 `context` 中恢复寄存器值。
- `ret`：根据恢复后的 `ra` 跳转到目标线程的执行位置。

同时区分了 `context` 和 `trapframe`：前者主要用于线程之间的普通上下文切换，后者用于中断现场的保存与恢复。新创建线程的首次运行还需要通过预先构造的执行现场进入相应的恢复流程。

### 4. 四个函数如何衔接新线程的首次运行

**本人提出的问题：**

我希望弄清楚进程创建模块已经构造好的上下文，最终是怎样被调度模块使用的，以及新线程第一次运行和已经运行过的线程有什么区别。

**ChatGPT 协助分析：**

结合前一模块的 `copy_thread()` 实现，解释了两种情况：

- 对于已经运行过的线程，`switch_to()` 恢复之前保存的 `ra`、`sp` 等寄存器，使线程从上次切换的位置继续执行。
- 对于新创建的线程，其 `context.ra` 预先设置为 `forkret`，第一次切换后进入该函数，再通过 `forkrets()` 和 `__trapret` 恢复中断帧，最终进入 `kernel_thread_entry` 执行指定的内核线程函数。

这样就把进程创建模块准备的执行现场与本模块实现的调度和切换过程联系起来。

---

## 三、Challenge：中断状态保存与恢复

### 1. `local_intr_save()` 和 `local_intr_restore()` 的实现原理

**本人提出的问题：**

对于 Challenge 1，我希望结合项目源码理解：

`local_intr_save(intr_flag); ... local_intr_restore(intr_flag);`

这两条语句究竟是如何实现关闭和恢复中断的，而不是只知道它们的作用。

**ChatGPT 协助分析：**

首先从 `kern/sync/sync.h` 中的宏定义入手，沿着调用关系分析到 `__intr_save()`、`intr_disable()` 和 `intr_enable()`。

重点解释了 RISC-V `sstatus` 寄存器中的 SIE 位：

- 通过 `read_csr(sstatus)` 读取当前中断状态。
- 如果 SIE 为 1，说明原来中断开启，需要关闭中断并返回 1。
- 如果 SIE 为 0，说明原来已经关闭，直接返回 0。
- 最后根据保存的标志决定是否重新开启中断。

### 2. 为什么需要保存原来的中断状态

进一步提出了嵌套调用的问题：如果 `schedule()` 已经关闭中断，随后调用 `proc_run()`，内层函数结束时是否会提前开启中断？

ChatGPT 结合两个函数的实际调用关系进行了分析：

1. 外层 `schedule()` 保存状态为 1，并关闭中断。
2. 内层 `proc_run()` 发现中断已经关闭，因此保存状态为 0。
3. 内层结束时恢复状态为 0，不会开启中断。
4. 外层结束时恢复状态为 1，才重新开启中断。

由此理解了这种设计的意义：恢复的是进入临界区之前的状态，而不是无条件开启中断。

同时明确了本机制主要用于保护当前 hart 上的临界区，不能直接替代多核环境中的锁机制。

---

## 四、代码检查与测试验证

### 1. AI 生成代码的检查与修改范围

在 Codex 完成指定函数后，我会结合项目源码检查实现，而不是直接接受生成结果。

ChatGPT 帮助我从函数接口、数据结构和调用关系出发检查代码，尤其关注：

- 是否修改了其他成员负责的文件和函数。
- 是否正确使用已有接口，而不是自行设计新的数据结构。
- 链表遍历是否存在遗漏或无法结束的情况。
- `satp` 的构造和 TLB 刷新是否正确。
- `switch_to()` 中寄存器保存顺序与 `struct context` 是否一致。
- 中断状态是否能够正确恢复。

### 2. 编译警告与构建结果分析

在执行编译和运行命令时，我还询问了不同警告信息的含义，以及它们是否属于本人负责的模块。

ChatGPT 帮助我区分了编译器警告和链接器输出的信息，并根据源文件路径判断问题涉及的模块。例如，`kern/mm/kmalloc.c` 中的类型警告不属于本次四个调度函数的直接实现范围，而链接器输出的部分 unused section 信息也不等同于编译失败。

同时提醒我不能仅凭警告信息就认定其一定来自实验框架，还需要结合文件修改记录和成员分工确认责任归属。

最终以完整编译、链接和实际运行结果判断本模块是否能够正常工作。

### 3. QEMU 运行结果分析

**本人提出的问题：**

在执行 `make qemu` 后，我希望确认应该截取哪些输出作为报告证据，以及如何理解启动过程中出现的警告和最后的 kernel panic。

**ChatGPT 协助分析：**

结合实际终端输出，梳理了启动过程：

1. OpenSBI 正常启动，识别 QEMU Virt Machine 平台。
2. uCore 完成设备树、物理内存和内存管理相关初始化。
3. 内核创建的 `initproc` 获得 CPU 执行机会。
4. 输出 `Hello world!!` 和 `Bye, Bye. :)`。
5. 最后在 `do_exit()` 处触发 kernel panic。

其中，`initproc` 成功执行说明调度器已经选中目标线程，并通过上下文切换使其开始运行。最后的 panic 与实验框架中尚未完整实现的线程退出处理有关，不代表前面的调度过程失败。

### 4. 自动评分与最终验证

结合实际执行结果，确认本模块的验证情况：

- 完整编译与链接成功，生成内核文件。
- `switch.S` 单独编译成功，并通过符号表检查确认 `switch_to` 符号。
- `git diff --check` 检查通过。
- `make grade` 的 PMM、Process Create 和 Schedule 三项检查均通过，最终得分为 30/30。
- `make qemu` 成功启动 uCore，并观察到 `initproc` 被调度执行。

通过这些检查，将代码实现、底层汇编、内核运行和自动评分联系起来，验证了本模块的实现结果。