# Lab3 Prompt 汇总
## 2413669 张懿鸾 （1号）

> 总结一下现在文件的分支还有所在目录

<details><summary>1 previous message</summary>

> 我先检查当前工作目录、Git 分支和文件结构，再给你一个简明的汇总。
</details>

当前所在目录：`/home/win/oslab/OS-Lab`

Git 分支：`lab3`，对应远程分支 `origin/lab3`；工作区目前没有未提交的改动。

目录结构概要：

- `/home/win/oslab/OS-Lab/code`：主要代码，包含 `kern`、`libs`、`obj`、`tools` 和 `Makefile`
- `/home/win/oslab/OS-Lab/report`：报告目录，包含 `report.md`、`prompt.md` 和 `images`
- `/home/win/oslab/OS-Lab/README.md`：项目说明
- `.agents`、`.aws`、`.codex`：本地配置目录
- `.git`：Git 版本库数据

> 帮我检查一下哪里需要插入lab2代码，以及是否可以直接复用lab2分支中相关部分的实现

<details><summary>3 previous messages</summary>

> 我会对比 `lab2` 与当前 `lab3` 的代码、构建配置和已有报告，先确认 Lab 2 的哪些实现被 Lab 3 依赖，以及接口和文件位置是否一致。
>
> 分支关系已经确认：`lab3` 是从 `lab2` 之上继续建的，但最近一次提交把 `lab3` 的源码恢复成了实验模板，因此 Lab 2 的页面管理实现确实从当前分支消失了。接下来我会核对启动、异常处理和测试路径，判断哪些 Lab 2 代码属于前置依赖、哪些与 Lab 3 模板冲突。
>
> 目前缺少的并不只是 `page_init()`：Lab 3 模板中的 `best_fit_pmm.c` 也删掉了空闲链表实现，而它仍会参与编译，所以只补 `page_init()` 仍会编译失败。我再做一次基线构建，确认所有阻断点，并区分“Lab 2 可直接移植”与“Lab 3 必须新写”的部分。
</details>

可以复用，但要“按函数移植”，不能直接用 Lab 2 的整个 `pmm.c` 或整个 `lab2` 分支覆盖当前 `lab3`。`lab3` 在提交 `44166e1` 中恢复成了实验模板，导致 Lab 2 的物理内存代码被删除；本地 `lab2` 与 `origin/lab2` 的源码部分完全相同，差异只在报告和图片。

## 需要补回的位置

| 当前文件 | 插入内容 | 复用判断 |
|---|---|---|
| [pmm.c](./code/kern/mm/pmm.c:84) | 插入 `static void page_init(void)` 完整函数，放在 `pmm_init()` 前 | 基本可直接复用，但要适配一个宏 |
| [best_fit_pmm.c](./code/kern/mm/best_fit_pmm.c:7) | 插入 `free_area`、`best_init`、`best_init_memmap`、`best_alloc_pages`、`best_free_pages`、`best_nr_free_pages`，并在文件末尾加入 `best_fit_pmm_manager` | 可直接从 Lab 2 移植 |
| [memlayout.h](./code/kern/mm/memlayout.h:8) | 如果原样复制 `page_init()`，需要恢复 `PHYSICAL_MEMORY_END`；也可以改用现有的 `KERNTOP - PHYSICAL_MEMORY_OFFSET` | 只补宏或一行适配 |
| [pmm.c](./code/kern/mm/pmm.c:38) | 如需继续使用 Lab 2 的 Best-Fit，把 `default_pmm_manager` 改为 `best_fit_pmm_manager` | Lab 3 不强制选择 Best-Fit，但这样与 Lab 2 保持一致 |
| [default_pmm.c](./code/kern/mm/default_pmm.c:46) | 不需要从 Lab 2 覆盖，当前模板已有 First-Fit 实现 | 不移植 |

不能整体覆盖 [pmm.c](./code/kern/mm/pmm.c:1) 的原因是：Lab 2 版本包含已经被 Lab 3 删除的 `buddy_pmm.h`，同时缺少 Lab 3 当前的中断保护包装。因此应保留当前 Lab 3 文件，只放入 `page_init()`。同样，不要覆盖 Lab 3 的 Makefile、`memlayout.h` 和 `entry.S`，它们已经加入了异常、时钟及启动相关改动。

## Lab 3 部分不能从 Lab 2 复用

Lab 2 分支没有 `kern/trap` 目录，也没有完整的异常处理实现，所以下面两处仍需独立完成：

- [trapentry.S](./code/kern/trap/trapentry.S:57)：补全 `RESTORE_ALL`，并在 `SAVE_ALL` 后调用 `trap`。
- [trap.c](./code/kern/trap/trap.c:83)：实现 `interrupt_handler()` 和 `exception_handler()`，包括时钟中断处理及非法指令、断点的 `EPC` 调整。

当前基线构建实测还会报三类错误：`page_init` 未定义、两个 Trap handler 未定义，以及 `best_fit_pmm.c` 中的 `free_list`/`nr_free` 未定义。因此至少要先补 `page_init()` 和 Best-Fit 相关实现，之后才能进入真正的 Lab 3 Trap 调试。检查过程中没有修改源代码。

> 我们这一步先补充lab3中需要的lab2的代码，暂时**不实现实验三的 Trap 入口或上下文恢复功能。**
>
> ## 一、开始修改前的检查
>
> 1. 确认当前目录、分支和 Git 工作区状态。
> 2. 确认当前分支是 `lab3`。
> 3. 执行 `git fetch origin lab2`，确保可以读取最新的 `origin/lab2`。
> 4. 查看以下文件的当前版本及 `origin/lab2` 版本：
>    - `code/kern/mm/pmm.c`
>    - `code/kern/mm/best_fit_pmm.c`
>    - `code/kern/mm/memlayout.h`
>    - `code/kern/mm/pmm.h`
>    - `code/kern/mm/best_fit_pmm.h`
> 5. 使用 `git show origin/lab2:文件路径` 获取参考实现。不要直接切换分支，也不要使用未经核对的旧版本代码。
>
> ## 三、需要恢复的代码
>
> ### 任务 A：恢复 `pmm.c` 中的 `page_init()`
>
> 文件：`code/kern/mm/pmm.c`
>
> 定位标记：
>
> `//lab2 请补充你在lab2中的代码`
>
> 从 `origin/lab2:code/kern/mm/pmm.c` 中提取与物理页初始化有关的完整 `static void page_init(void)` 函数，填入实验三模板中对应的位置。
>
> 要求：
>
> 1. 保留实验三现有的其他函数和代码结构。
> 2. 不要将实验二整个 `pmm.c` 覆盖到实验三。
> 3. 核对 `pmm_init()` 对 `page_init()` 的调用，确保函数定义、调用和声明关系正确。
> 4. 核对物理页描述符数组、内核结束地址、可用物理内存范围和页数计算逻辑。
> 5. 检查实验三的 `memlayout.h` 是否定义了该实现所需的宏。
> 6. 如果实验三缺少实验二实现所需的物理内存边界宏，先比较两版地址定义及其含义，再决定采用兼容现有宏的写法还是补充必要定义。不得直接引入未经验证的地址常量。
> 7. 不要删除实验三原有的中断保护代码或其他初始化逻辑。
>
> ### 任务 B：恢复 `best_fit_pmm.c` 中的 Best-Fit 实现
>
> 文件：`code/kern/mm/best_fit_pmm.c`
>
> 定位标记：
>
> `//lab2 请补充你在lab2中的代码`
>
> 从 `origin/lab2:code/kern/mm/best_fit_pmm.c` 中恢复该标记对应的 Best-Fit 实现。
>
> 重点核对并恢复以下内容（以实验二实际实现为准）：
>
> - `free_area_t free_area` 及必要的链表、空闲页数定义。
> - `best_init()`。
> - `best_init_memmap()`。
> - `best_alloc_pages()`。
> - `best_free_pages()`。
> - `best_nr_free_pages()`。
> - `best_fit_pmm_manager` 管理器对象及其函数指针。
>
> 要求：
>
> 1. 保留实验三模板已有的 `basic_check()` 等测试代码，避免重复定义。
> 2. 恢复完整的 Best-Fit 分配、释放、空闲块合并和空闲页计数逻辑。
> 3. 保留必要的边界检查、断言及链表操作约束。
> 4. 不要将实验二整个文件覆盖到实验三。
> 5. 核对 `pmm.h` 和 `best_fit_pmm.h` 中的数据结构与声明是否兼容；只有确认存在必要依赖时，才考虑修改头文件。
>
> ### 任务 C：核对默认内存管理器选择
>
> 检查实验三 `pmm.c` 中的 `init_pmm_manager()` 以及 `origin/lab2` 中的管理器选择方式。
>
> 判断恢复 Best-Fit 实现后，实验三是否还需要显式选择 `best_fit_pmm_manager`。
>
> - 不要为了让代码编译而随意切换管理器。
> - 如果实验二实际使用 Best-Fit，而实验三当前仍默认使用其他管理器，请说明两者差异及影响。只有在确认需要保留实验二的 Best-Fit 行为时，才进行必要的最小修改，并在结果报告中说明理由。
>
> * 不要修改 Makefile 中的项目名称、构建目录或实验三的其他配置。
>
> ## 四、严格禁止修改的内容
>
> 本次任务只涉及实验二代码恢复，禁止修改以下文件或功能：
>
> - `code/kern/trap/trapentry.S`
> - `code/kern/trap/trap.c`
> - `code/kern/trap/trap.h`
> - `code/kern/driver/clock.c`
> - `code/kern/driver/intr.c`
> - `code/kern/init/init.c`
> - `code/Makefile`
> - `code/tools/grade.sh`
> - `code/tools/inject.py`
> - 实验三测试文件及其他无关文件
>
> 特别注意：
>
> 1. 不要实现 `RESTORE_ALL`、`__alltraps` 或 `__trapret`。
> 2. 不要实现 `interrupt_handler()` 或 `exception_handler()`。
> 3. 不要通过增加空函数、占位函数或关闭编译警告来绕过错误。
> 4. 不要恢复 Buddy、SLUB、MMU Probe 等实验二专属扩展，除非当前实验三模板明确依赖它们；如发现此类依赖，先报告，不要擅自扩大修改范围。
> 5. 不要执行 `git commit`、`git push` 或切换分支。
>
> ## 五、验证要求
>
> 完成代码恢复后：
>
> 1. 执行 `git diff --check`，确保没有新增的空白符问题。
> 2. 检查 `git status --short`，确认修改范围符合本任务要求。
> 3. 查看 `git diff --` 涉及的全部文件，确认没有覆盖实验三已有逻辑。
> 4. 尝试使用项目现有 Makefile 编译 `pmm.c` 和 `best_fit_pmm.c` 对应的目标文件，验证这两部分代码本身能够编译。
> 5. 再尝试执行完整构建 `cd code && make clean && make`。
>
> 注意：当前实验三模板的 `trap.c` 已知存在 `interrupt_handler()` 和 `exception_handler()` 尚未声明或实现的问题，完整构建可能因此失败。该问题属于后续实验三任务，本次不得修改 Trap 代码来消除它。
>
> 如果完整构建仍然失败，请区分：
>
> - 本次恢复的物理内存管理代码是否编译成功；
> - 完整构建是否仍被现有 Trap 问题阻断。
>
> 不要将局部编译成功描述为整个内核构建成功。
>
> ## 六、完成后必须提供的报告
>
> 请输出：
>
> 1. 修改前后的 Git 工作区状态。
> 2. 实际修改的文件列表。
> 3. `page_init()` 从实验二移植了哪些内容，以及是否进行了地址宏适配。
> 4. Best-Fit 管理器恢复了哪些函数，是否保留了原有测试代码。
> 5. 是否修改默认内存管理器选择，以及原因。
> 6. `git diff --check` 的结果。
> 7. 两个物理内存管理目标文件的编译结果。
> 8. 完整 `make` 的结果及未解决错误。
> 9. 每个修改文件的关键 diff，尤其是与 `origin/lab2` 对应的代码差异。
> 10. 明确声明：没有修改任何 Trap 入口、上下文恢复、中断或异常处理代码。
>
> 完成后停下来等待人工审查，不要自行提交或推送。

<details><summary>8 previous messages</summary>

> 我会严格只恢复 Lab 2 的物理内存管理代码，不触碰 Trap、时钟、中断、构建配置或测试文件。先核对目录和分支状态，再拉取并比对最新 `origin/lab2`，之后在 `lab3` 上做最小修改。
>
> 当前位于 `/home/win/oslab/OS-Lab`，分支为 `lab3`，跟踪 `origin/lab3`，工作区干净。现在按要求更新 `origin/lab2` 引用。
>
> `origin/lab2` 已成功更新。接下来逐项读取五份当前文件和远端 Lab 2 版本，重点核对地址宏、页描述符约定、Best-Fit 的末尾管理器注册，以及 Lab 3 自己保留的差异。
>
> 核对结果：`pmm.h`、`best_fit_pmm.h` 与 `origin/lab2` 的接口兼容，无需改动；`memlayout.h` 当前没有 `PHYSICAL_MEMORY_END`，但它已经提供等价的 `KERNTOP` 和 `PHYSICAL_MEMORY_OFFSET`，因此我会用 `KERNTOP - PHYSICAL_MEMORY_OFFSET` 适配，避免新增地址常量或覆盖头文件。管理器选择将最小改为 `best_fit_pmm_manager`，以保持 Lab 2 的实际运行行为。
>
> <details><summary>Edited 2 files</summary>
>
> - Wrote `./code/kern/mm/best_fit_pmm.c` (+137 -1)
> - Wrote `./code/kern/mm/pmm.c` (+100 -2)
>
> </details>
>
> 两处代码已恢复：`pmm.c` 只加入 `page_init()` 并切换为 Best-Fit；`best_fit_pmm.c` 只补管理逻辑和管理器对象，原有检查函数保持不动。先做差异与空白检查，再编译两个目标文件。
>
> 两个目标文件均已成功编译，说明恢复后的 `page_init()` 与 Best-Fit 实现本身通过 `-Werror`。下一步按顺序清理并执行完整构建；预计仍可能被尚未实现的 Trap 入口和处理函数阻断。
>
> 完整构建按预期停在 Lab 3 尚未实现的 `interrupt_handler()` / `exception_handler()`，没有报物理内存代码错误。为保证交付状态可复查，我又在缺少 Trap 的情形下单独编译了两个目标文件，二者仍成功；现在做最终的来源差异和修改范围核对。
</details>

已按要求只恢复 Lab 2 的物理内存管理代码，没有实现或修改任何 Lab 3 Trap 功能，也没有提交、推送或切换分支。

## 1. 工作区与分支

修改前：

```text
分支：lab3
状态：## lab3...origin/lab3
工作区：干净
```

修改后：

```text
分支：lab3

 M code/kern/mm/best_fit_pmm.c
 M code/kern/mm/pmm.c
```

已执行 `git fetch origin lab2`，更新后的远端版本为：

```text
912cbec (origin/lab2) Add member3 lab2 images
```

## 2. 修改文件

仅修改了以下两个文件：

- [pmm.c](./code/kern/mm/pmm.c:89)
- [best_fit_pmm.c](./code/kern/mm/best_fit_pmm.c:13)

共增加 237 行、删除 3 行。`memlayout.h`、`pmm.h`、`best_fit_pmm.h` 均未修改。

## 3. `page_init()` 移植

[pmm.c](./code/kern/mm/pmm.c:89) 已从 `origin/lab2` 恢复完整的 `static void page_init(void)`，包括：

- 设置 `va_pa_offset`。
- 从 DTB 获取物理内存基址和大小。
- 检查地址溢出、DRAM 下界和物理内存上界。
- 计算 `npage` 和页描述符数量。
- 将 `pages` 数组放在内核镜像 `end` 之后。
- 初始化并保留全部 `Page` 描述符。
- 排除内核、描述符数组和页内非完整区域。
- 对空闲范围进行页对齐。
- 调用现有 `init_memmap()` 注册空闲页。
- 输出物理内存、页描述符数组和空闲页范围信息。

Lab 3 的 `memlayout.h` 没有 `PHYSICAL_MEMORY_END`，但已有等价地址定义。因此没有新增地址常量，而是将 Lab 2 的：

```c
if (mem_end > PHYSICAL_MEMORY_END)
```

适配为：

```c
uint64_t mem_limit = KERNTOP - PHYSICAL_MEMORY_OFFSET;
if (mem_end > mem_limit)
```

两者在 RISC-V 当前布局下都表示物理地址上界 `0x88000000`。Lab 3 原有的中断保护包装、`pmm_init()` 调用及 `satp` 初始化逻辑均保留。

## 4. Best-Fit 恢复内容

[best_fit_pmm.c](./code/kern/mm/best_fit_pmm.c:13) 已恢复：

- `free_area_t free_area`
- `free_list` 和 `nr_free` 宏
- `best_init()`，第 13 行
- `best_init_memmap()`，第 19 行
- `best_alloc_pages()`，第 49 行
- `best_free_pages()`，第 81 行
- `best_nr_free_pages()`，第 132 行
- `best_fit_pmm_manager`，第 269 行
- 分配块拆分、释放时的前后相邻块合并及空闲页计数

Lab 3 原有 [basic_check()](./code/kern/mm/best_fit_pmm.c:137) 和 [best_fit_check()](./code/kern/mm/best_fit_pmm.c:190) 均保留，没有重复定义，也没有改动其测试逻辑。与 `origin/lab2` 的差异仅是 Lab 3 模板原本已有的注释、缩进、格式和 `string.h` 包含。

`pmm.h` 和 `best_fit_pmm.h` 的 `pmm_manager` 接口、管理器声明和 `Page` 类型均兼容，无需修改。

## 5. 默认内存管理器

已把 [init_pmm_manager()](./code/kern/mm/pmm.c:37) 从：

```c
pmm_manager = &default_pmm_manager;
```

最小修改为：

```c
pmm_manager = &best_fit_pmm_manager;
```

原因是 `origin/lab2` 在未启用 Buddy 或显式 First-Fit 时使用 Best-Fit；Lab 3 模板已经移除了相关编译开关，因此必须显式选择 Best-Fit，才能保留 Lab 2 的实际分配行为。该切换也会让 `pmm_init()` 执行 Best-Fit 自身的检查。未修改 Makefile。

## 6. `git diff --check`

执行结果为空，没有新增空白符错误：

```text
$ git diff --check
$
```

## 7. 目标文件编译

单独编译两个文件均成功：

```text
$ make obj/kern/mm/pmm.o obj/kern/mm/best_fit_pmm.o
+ cc kern/mm/pmm.c
+ cc kern/mm/best_fit_pmm.c
```

退出码为 0，说明恢复后的物理内存管理代码自身可以通过现有的 `-Werror` 编译选项。

## 8. 完整构建结果

已依次执行：

```text
make clean
make
```

完整构建仍失败，错误来自尚未实现的 Lab 3 Trap 处理函数：

```text
kern/trap/trap.c:89: implicit declaration of function 'interrupt_handler'
kern/trap/trap.c:92: implicit declaration of function 'exception_handler'
```

因此结论是：

- `pmm.c` 和 `best_fit_pmm.c` 可以独立编译成功。
- 完整内核构建仍被现有 Trap 任务阻断。
- 不能认为整个内核已经构建成功。
- 没有通过空函数、修改声明或关闭警告来绕过 Trap 错误。

## 9. 与 `origin/lab2` 的关键差异

`pmm.c`：

- 保留了 Lab 3 的 `sync.h` 中断保护包装。
- 保留了 Lab 3 当前结构，没有引入 `buddy_pmm.h` 或 Buddy 代码。
- 只移植 `page_init()` 函数。
- `PHYSICAL_MEMORY_END` 适配为 `KERNTOP - PHYSICAL_MEMORY_OFFSET`。
- 显式选择 Best-Fit，但未恢复 Lab 2 Makefile 中的 `PMM_BUDDY`、`PMM_FIRST_FIT` 等开关。

`best_fit_pmm.c`：

- 恢复的管理逻辑与 `origin/lab2` 一致。
- 没有覆盖整个文件，Lab 3 原有检查函数和格式差异保留。
- 未恢复 Buddy、SLUB、MMU Probe 或其他 Lab 2 扩展。

## 10. 禁止修改项确认

明确声明：没有修改任何 Trap 入口、Trap 上下文保存与恢复、中断处理、异常处理、时钟、启动流程、Makefile、评测脚本或 Lab 3 测试代码。

具体未修改：

- `code/kern/trap/trapentry.S`
- `code/kern/trap/trap.c`
- `code/kern/trap/trap.h`
- `code/kern/driver/clock.c`
- `code/kern/driver/intr.c`
- `code/kern/init/init.c`
- `code/Makefile`
- `code/tools/grade.sh`
- `code/tools/inject.py`
- 所有 Lab 3 测试文件

目前停在这里，等待人工审查，未执行 `git commit` 或 `git push`。

> 你现在负责操作系统实验三（RISC-V uCore）的一个独立子任务：**Trap 入口与上下文恢复**。
>
> ## 一、本次授权范围
>
> 唯一允许修改的文件：
>
> `code/kern/trap/trapentry.S`
>
> 允许只读查看以下文件，以确认接口和数据布局：
>
> - `code/kern/trap/trap.h`
> - `code/kern/trap/trap.c`
> - `code/libs/riscv.h`
> - `code/kern/mm/pmm.h`（仅在确有必要时）
>
> **不得修改其他文件，不得提交或推送 Git，不得切换分支。**
>
> ## 二、实施要求
>
> ### 1. 先检查现状，再修改
>
> 确认当前模板的实际内容和待填标记位置。不要假定模板与其他版本完全相同。
>
> ### 2. 正确理解 Trap 帧布局
>
> 根据现有 `SAVE_ALL` 和 `struct trapframe`，核对寄存器保存槽位与 C 结构体字段的对应关系。
>
> Trap 帧共 36 个机器字：
>
> - 偏移索引 0～31：通用寄存器 x0～x31。
> - 索引 32：`status`，对应 `sstatus`。
> - 索引 33：`epc`，对应 `sepc`。
> - 索引 34：`badvaddr`。
> - 索引 35：`cause`，对应 `scause`。
>
> RV64 下每个机器字为 8 字节，必须使用现有汇编宏和项目的寄存器保存约定，不得擅自改变 Trap 帧布局。
>
> ### 3. 完成上下文恢复
>
> 检查并补全 `RESTORE_ALL` 中的 `lab3 function1` 待填部分，确保：
>
> - 从 Trap 帧恢复保存的 `sstatus` 和 `sepc`。
> - 恢复所有应恢复的通用寄存器。
> - 正确处理 x2（sp），避免在仍需要使用 Trap 帧时提前覆盖当前栈指针。
> - 不错误地恢复 x0，也不遗漏应恢复的寄存器。
> - 恢复顺序符合 RISC-V 汇编和现有 `SAVE_ALL` 的约定。
>
> 不得为了通过编译而随意删除寄存器保存、恢复指令或屏蔽错误。
>
> ### 4. 检查 `__alltraps` 入口流程
>
> 根据现有代码，确认 Trap 入口流程是否完整：
>
> 1. 进入 `__alltraps`。
> 2. 调用现有 `SAVE_ALL` 保存上下文。
> 3. 将当前 Trap 帧地址作为 `trap()` 的第一个参数传入 a0。
> 4. 调用现有的 `trap()`。
> 5. 返回后进入 `__trapret`。
> 6. 调用 `RESTORE_ALL`。
> 7. 使用 `sret` 返回被中断或发生异常的执行上下文。
>
> 如果模板已经包含某一步，保留现有正确实现，不要重复添加指令或标签。只补充确实缺失的部分。
>
> 同时检查 `stvec` 指向的入口与现有 `idt_init()` 是否一致，但不要修改 `idt_init()` 或其他 C 文件。
>
> ### 5. 严格限制本次任务边界
>
> 本次不实现、不修改：
>
> - `trap.c` 中的 `interrupt_handler()`。
> - `trap.c` 中的 `exception_handler()`。
> - 时钟初始化与时钟中断处理。
> - `clock.c`、`intr.c`、`init.c`。
> - `Makefile`、评分脚本、测试文件。
> - `SAVE_ALL` 中已有且正确的代码，除非发现明确的必要问题；如发现问题，先报告，不要擅自扩大修改范围。
>
> ## 四、验证要求
>
> 修改后执行：
> ```css
> git diff --check
> git diff -- code/kern/trap/trapentry.S
> git status --short --branch
> ```
>
> 然后尝试只编译 Trap 入口汇编目标：
> ```bash
> cd code
> make obj/kern/trap/trapentry.o
> ```
>
> 如果该目标名称在当前 Makefile 中不成立，请先检查实际目标规则，不要修改 Makefile 来迁就命令。
>
> 本次不要运行完整评分测试，不要修改 `trap.c` 来解决其他编译错误。
>
> ##
>
> 完成后请提供：
>
> 1. 修改了哪个文件。
> 2. 填补了哪些待填标记，分别解决什么问题。
> 3. `RESTORE_ALL` 的寄存器恢复顺序及原因。
> 4. `__alltraps` 的入口调用流程。
> 5. 完整的 `git diff`。
> 6. `git diff --check` 和汇编目标编译结果。
> 7. 当前 Git 工作区状态。
>
> 如果发现模板存在歧义、寄存器保存约定不明确，或必须修改其他文件才能继续，请先停止并报告，不要自行扩大任务范围。本次只修改和验证，不提交、不推送。

<details><summary>4 previous messages</summary>

> 我先核对当前 `lab3` 工作区、Trap 帧结构、`SAVE_ALL` 的槽位约定以及 `stvec` 绑定，只修改 `trapentry.S`。现有物理内存改动会保留不动。
>
> 帧布局与 `SAVE_ALL` 已核对一致：x2 的原值保存在槽位 2，状态槽位为 32～35。实现时用 `a0` 临时恢复 `sepc`/`sstatus`，随后再把 `a0` 从槽位 10 恢复；x0 不加载，x2 最后恢复，避免提前丢失 Trap 帧基址。
>
> <details><summary>Edited a file</summary>
>
> - Wrote `./code/kern/trap/trapentry.S` (+41 -2)
>
> </details>
>
> [Trap 入口与恢复差异] 已只包含 `RESTORE_ALL` 和 `__alltraps` 中缺失的流程，`SAVE_ALL`、帧大小、标签和 `sret` 都保持原样。现在编译实际 Makefile 目标并检查汇编产物。
</details>

已完成 Trap 入口与上下文恢复。仅修改了 [trapentry.S](./code/kern/trap/trapentry.S:57)，未修改其他文件，未运行完整评分测试，未提交或推送。

## 1. 修改文件

```text
code/kern/trap/trapentry.S
```

原有 `SAVE_ALL`、36 字 Trap 帧布局、`__trapret` 标签和 `sret` 均保持不变。

## 2. 填补的待填标记

### `RESTORE_ALL`

补全了从 Trap 帧恢复 CPU 上下文的逻辑：

- 使用槽位 33 恢复 `sepc`。
- 使用槽位 32 恢复 `sstatus`。
- 恢复 x1 和 x3～x31。
- 不加载 x0，因为 RV64 的 x0 永远为零。
- x2 最后从槽位 2 恢复，避免过早覆盖 Trap 帧基址。

`a0` 暂时用于装载 `sepc` 和 `sstatus`，随后再从槽位 10 恢复原来的 `a0`，因此最终不会破坏调用者上下文。

### `__alltraps`

补全了进入 C 处理函数的调用：

```asm
mv a0, sp
jal trap
```

此时 `a0` 指向刚建立的 `struct trapframe`。`jal trap` 返回后顺序落入现有 `__trapret`，由 `RESTORE_ALL` 恢复上下文并执行 `sret`。

`idt_init()` 已将 `stvec` 设置为 `&__alltraps`，与当前入口一致，无需修改 `trap.c`。

## 3. 寄存器恢复顺序

实际顺序为：

1. 从槽位 33 读取 `sepc`。
2. 从槽位 32 读取 `sstatus`。
3. 恢复 x1。
4. 恢复 x3～x31。
5. 最后恢复 x2。

将 x2 放在最后，是因为前面所有 `LOAD ... (sp)` 都依赖当前 `sp` 指向 Trap 帧。x0 不需要恢复。

经反汇编确认：

```text
ld a0,264(sp)
csrw sepc,a0
ld a0,256(sp)
csrw sstatus,a0
...
ld sp,16(sp)
sret
```

## 4. `__alltraps` 流程

当前流程为：

```text
__alltraps
  -> SAVE_ALL
  -> a0 = sp
  -> jal trap
  -> __trapret
  -> RESTORE_ALL
  -> sret
```

没有重复添加 `SAVE_ALL`、`__trapret` 或 `sret`。

## 5. 完整 Git diff

```diff
diff --git a/code/kern/trap/trapentry.S b/code/kern/trap/trapentry.S
index c648c0a..6b21309 100644
--- a/code/kern/trap/trapentry.S
+++ b/code/kern/trap/trapentry.S
@@ -56,7 +56,45 @@

     .macro RESTORE_ALL

-    #lab3 function1 YOUR CODE
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

@@ -65,8 +103,9 @@
 __alltraps:
     SAVE_ALL

-    #lab3 function1 YOUR CODE
     # sp should be the same as before "jal trap"
+    mv a0, sp
+    jal trap

     .globl __trapret
 __trapret:
```

## 6. 验证结果

`git diff --check` 成功，输出为空，没有空白符问题。

汇编目标编译成功：

```text
$ make obj/kern/trap/trapentry.o
+ cc kern/trap/trapentry.S
```

当前 Git 状态：

```text
## lab3...origin/lab3
 M code/kern/trap/trapentry.S
```

没有修改 `trap.c` 中的 `interrupt_handler()`、`exception_handler()`，也没有修改时钟、中断、初始化、Makefile、评分脚本或测试文件。

> 好的，我负责的实验部分进入尾声，我们来回顾一下。我负责的部分的要求为功能模块一：Trap 上下文恢复与 Trap 入口功能（设计提示词借助 ai 完成）
>
> 要求实现的函数或汇编功能有(kern\trap\trapentry.S)：
> ```armasm
> RESTORE_ALL
> __alltraps
>
> ```
>
> 整个功能模块的具体功能为：这个模块负责完成 Trap 从“进入内核”到“返回原执行流”之间的底层上下文管理。`SAVE_ALL` 已经负责把 Trap 发生时的处理器状态保存到当前内核栈上的 Trap frame 中，学生需要根据这一已有的保存关系，设计提示词让 AI 完成对应的 `RESTORE_ALL`，使被打断程序的执行状态能够从 Trap frame 中恢复出来。同时，`__alltraps` 需要负责建立汇编 Trap 入口与 C 语言 `trap(struct trapframe *tf)` 之间的调用关系，使保存后的 Trap frame 能够按照 RISC-V 调用约定正确传递给 `trap()`；这个模块完成后，系统应能够形成完整的“Trap 进入 -> 上下文保存 -> C 语言处理 -> 上下文恢复 -> Trap 返回”闭环。
>
> 整个功能模块的注意事项为：这里最关键的是保持“Trap frame 布局、上下文保存与恢复、函数调用约定、Trap 返回状态”之间的一致性。`RESTORE_ALL` 必须与已经提供的 `SAVE_ALL` 保持对应关系，不能自行假设一个与实际 Trap frame 不一致的数据布局。特别需要注意，`SAVE_ALL` 中对 `x2(sp)` 的处理方式与其他通用寄存器并不完全相同，因此设计 `RESTORE_ALL` 时不能简单地把所有寄存器按照完全相同的方式恢复。`__alltraps` 则需要保证当前 Trap frame 的地址能够按照 RISC-V 调用约定作为 `trap()` 的参数传递，同时不能破坏后续恢复上下文所需要的信息。本功能模块不要求学生直接按照某一种固定的汇编指令序列实现，而是要求学生根据已有代码、接口和体系结构约束设计 Prompt，让 AI 完成功能实现。
>
> 整个功能模块应该满足的对外的接口为：Trap 发生以后，系统应能够通过 `__alltraps` 建立完整的 Trap frame，并将其作为 `struct trapframe *` 传递给：
> ```c
> void trap(struct trapframe *tf);
>
> ```
>
> `trap()` 返回以后，应能够进入 `__trapret`，完成上下文恢复并通过 `sret` 返回。对于 `RESTORE_ALL` 而言，对外表现为“输入当前 Trap frame，输出恢复后的处理器执行状态”；对于 `__alltraps` 而言，对外表现为“将 Trap 上下文正确传递给 C 语言 Trap 处理函数”；整个模块不得修改 `SAVE_ALL`、`struct trapframe` 和 `trap()` 的函数签名。
>
> 整个功能模块可以借助的接口函数、结构体、数据结构为：这个模块主要依赖 `struct trapframe`、`trap()`、已经提供的 `SAVE_ALL`、`__alltraps`、`__trapret`、`RESTORE_ALL` 以及 RISC-V 函数调用约定和特权级 Trap 返回机制。其中，`struct trapframe` 决定保存后的上下文数据组织形式，`SAVE_ALL` 是已经提供的上下文保存参考，`trap(struct trapframe *tf)` 是 C 语言层面的 Trap 处理接口，而 `sret` 是最终完成 Trap 返回的处理器特权指令。学生需要结合这些已有接口和约束设计 Prompt，而不是直接依据指导书中的伪代码填写汇编指令。
>
> 不允许修改文件，只能查看，以上功能全部都实现了吗？以及之前我们提到的void interrupt_handler(struct trapframe \*tf);(kern\trap\trap.c) void exception_handler(struct trapframe \*tf);(kern\trap\trap.c)这两个函数的函数声明有了吗？可以交给我的队友进行下一步对于这两个函数的编写了吗？

<details><summary>2 previous messages</summary>

> 我以只读方式核对当前 `lab3`：确认 `trapentry.S` 是否仍是已实现版本、36 字槽位是否完全对应，并检查 `trap.c` 中这两个 Handler 是否已有声明或定义。
>
> <details><summary>Explored 3 files, a search, ran 3 commands</summary>
>
> - Ran `git status --short --branch`
>
> <details><summary>Explored 3 files, a search</summary>
>
> - Read `./code/kern/trap/trapentry.S`
> - Searched for `interrupt_handler|exception_handler|trap_dispatch|void trap\(` in `trap.c`
> - Read `./code/kern/trap/trap.c`
> - Read `./code/kern/trap/trap.h`
>
> </details>
>
> - Ran `git diff -- code/kern/trap/trapentry.S code/kern/trap/trap.c code/kern/trap/trap.h`
>
> - Ran `find code/obj -path '*trapentry.o' -o -path '*trap.o' 2>/dev/null | sort`
>
> </details>
</details>

功能模块一已经实现完整，可以交给队友继续编写两个 Handler；但目前还不能认为整个 Lab 3 已经可以完整编译。

## Trap 入口与恢复完成情况

[trapentry.S](./code/kern/trap/trapentry.S:57) 的检查结果如下：

- `RESTORE_ALL` 已从槽位 33 恢复 `sepc`。
- 已从槽位 32 恢复 `sstatus`。
- 已恢复 x1、x3～x31。
- 没有恢复 x0，符合 RISC-V 规定。
- x2 最后从槽位 2 恢复，避免提前覆盖 Trap 帧基址。
- 使用 `a0` 临时恢复 CSR，随后从槽位 10 恢复原来的 `a0`。
- `__alltraps` 已执行 `mv a0, sp`，将 `struct trapframe *` 传给 `trap()`。
- `jal trap` 返回后落入现有 `__trapret`，随后执行 `RESTORE_ALL` 和 `sret`。
- `idt_init()` 设置的 `stvec` 指向 `__alltraps`，入口关系一致。

保留了 `#lab3 function1 2413669` 注释，但下方已经没有 `YOUR CODE` 待填标记，因此它目前只是负责人标记，不表示功能未完成。

## 两个 Handler 的声明状态

当前 [trap.c](./code/kern/trap/trap.c:83) 中：

- 没有 `interrupt_handler()` 的函数声明。
- 没有 `exception_handler()` 的函数声明。
- 也还没有这两个函数的定义。
- `trap_dispatch()` 已经直接调用它们，因此当前完整 `make` 会因隐式函数声明错误被阻断。

当前 [trap.h](./code/kern/trap/trap.h:49) 也只声明了 `trap()`、`idt_init()` 等接口，没有这两个 Handler。

## 能否交给队友继续

可以交接。建议明确告诉队友：

1. 你的模块一，也就是 `RESTORE_ALL` 和 `__alltraps`，已经完成。
2. 队友下一步应在 `trap.c` 的 `//lab3 function2` 位置实现：
   ```c
   void interrupt_handler(struct trapframe *tf);
   void exception_handler(struct trapframe *tf);
   ```
3. 两个函数不能声明为 `static`。Lab 3 测试代码会以外部函数方式调用它们，因此必须具有全局链接属性。
4. 如果函数定义写在 `trap_dispatch()` 后面，就必须在前面补充原型，或者直接放进 `trap.h`；否则仍会触发当前的隐式声明错误。
5. 只有这两个 Handler 补齐后，整个内核才可能继续通过完整构建。

当前 Git 状态为：

```text
## lab3...origin/lab3 [ahead 1]
```

工作区干净，没有新的未提交修改。本次只进行了查看，没有修改任何文件。

> 我是不是需要补全这两个函数的函数声明呢？

需要让这两个函数在使用前“可见”，但不一定非要单独写两条函数声明。

最清晰的分工是：

- 如果你只负责 `trapentry.S`，可以把这一步交给负责 Handler 的队友，无需扩大自己的修改范围。
- 队友在 `trap_dispatch()` 之前直接写函数定义，那么函数定义本身就是声明，不需要额外补两条原型。
- 如果队友把函数定义写在 `trap_dispatch()` 后面，或者想先把接口固定下来，就应补充：

```c
void interrupt_handler(struct trapframe *tf);
void exception_handler(struct trapframe *tf);
```

更推荐放在 [trap.c](./code/kern/trap/trap.c:83) 的 `//lab3 function2` 下方、`trap_dispatch()` 上方；放进 `trap.h` 也可以，但这两个函数目前只在该模块内部使用，不是必须暴露到头文件。

目前这两个函数既没有声明也没有定义，所以完整构建会报隐式声明错误。你需要做的是把这一要求明确交接给队友；只有队友补上函数定义，或者至少先补上函数声明，完整内核才能继续编译。你的 `RESTORE_ALL` 和 `__alltraps` 已经不需要为此改动。


---

## 2412419 张祎然（2号，成员 C）：中断处理

我负责 `kern/trap/trap.c` 中的中断处理模块。开发分工中的“成员 C”和报告中的“2号”都指本人。

## 1. 确认任务范围

**我：** 我负责成员 C 的中断处理，这部分代码由我自己编写。请先讲清已有接口和模块要求，提供基础框架即可，写好后再帮我检查。功能包括中断原因识别、持续时钟、计数与输出、达到次数后关机，以及其他中断提示。修改限定在 `trap.c` 中我的标记区域，保留原有接口、宏定义、结构体和测试，不改其他成员的部分。

**AI：** 本模块接收已经保存好的 TrapFrame，识别原因并完成处理，随后返回公共恢复路径。实现入口是 `interrupt_handler()`，需要用到时钟、打印和关机接口。时钟是否持续触发、计数与返回地址是否正确，可以通过运行和 GDB 检查；其他分支可以构造输入，核对输出和状态变化。

**我：** `RESTORE_ALL`、`__alltraps` 和异常处理由另外两位同学负责，我可以读它们的接口，但不改实现。另外，这次只排除 `CHALLENGE 1` 对应的 `lab3_switch_test()`，中断处理的基础要求都要保留。

**AI：** 那正式改动就集中在你的中断处理区域。涉及其他模块的依赖问题，先确认接口和当前完成情况，再安排独立测试。

## 2. 确认输入和依赖

**我：** 我打开了 `trap.c`，还不清楚 `tf` 从哪里来。请先根据与中断处理直接相关的定义，说明输入、调用关系和返回过程。

**AI：** `trap.h` 中的 `struct trapframe` 定义了 `cause`、`epc` 等字段。汇编入口保存现场后，将地址传给 `trap()`，再由 `trap_dispatch()` 分发到中断或异常处理函数。中断处理结束后，经调用者返回 `__trapret`，恢复寄存器并执行 `sret`。

**我：** 那我需要提供哪些代码，才能把自己的模块写完整？

**AI：** 先看下面这些定义和接口即可。遇到编译或启动问题时，再根据报错位置补充相关代码。

| 需要了解的内容 | 相关文件 | 用途 |
|----------------|----------|------|
| TrapFrame 字段和数据宽度 | `trap.h`、`defs.h` | 读取原因码和返回地址 |
| 分发、统计输出和现场打印 | `trap.c` | 保持调用关系和原有输出格式 |
| 中断原因号 | `riscv.h` | 与已有 `IRQ_*` 常量对应 |
| 共享计数和下一次时钟事件 | `clock.h`、`clock.c` | 更新 `ticks`，维持时钟触发 |
| 定时与关机接口 | `sbi.h`、`sbi.c` | 使用现有固件服务封装 |

## 3. 判断编辑器提示

**我：** `clock.c` 里的 `set_csr(sie, MIP_STIP)` 被标红了。这个文件是公共驱动，先不要改，帮我判断是代码问题还是编辑器没有识别出来。

**AI：** `sie` 在这里是 `set_csr` 宏使用的 CSR 名称。先查看 `riscv.h` 中的宏定义，并核对编辑器是否按 RISC-V 工具链解析项目。编辑器标红不一定对应编译错误，要以交叉编译器的输出为准。

## 4. 原因码与返回地址

**我：** 中断模块要根据 `tf->cause` 区分类型，还要保持原程序能够继续执行。请说明原因码里哪些位参与分类，以及处理结束后哪些现场不能改。

**AI：** 当前目标是 RV64，最高位表示是否为中断，其余位表示具体原因。例如 `0x8000000000000005` 对应 Supervisor 时钟中断。分类时可以使用去掉最高位后的局部值，原始 `tf->cause` 留在 TrapFrame 中。

**我：** 时钟处理后，`epc` 要像某些异常那样向后调整吗？

**AI：** 时钟打断的是正常执行过程，`epc` 应保持不变。需要跳过断点或非法指令的情况由异常处理模块负责。你的函数也不应改写 TrapFrame 中的其他寄存器现场。

## 5. 基础框架和检查条件

**我：** 请给出保留原有接口的基础框架，函数体先留空，由我自己填写。再说明输入、依赖和各分支需要达到的结果，我写好后贴出来检查。

**AI：** 先保留函数入口，下面的框架还没有实现处理逻辑：

```c
void interrupt_handler(struct trapframe *tf) {
    /* TODO: 补充中断处理逻辑 */
}
```

接口保留外部链接属性，输入是已有入口保存的有效 TrapFrame。需要辅助函数时，由你在本人标记区域内编写，作为私有实现。填写后可以按下面几种情况检查。

| 情况 | 应有的处理结果 | 检查方法 |
|------|----------------|----------|
| Supervisor 时钟中断 | 重设下一次事件，`ticks` 恰好增加一次 | 跟踪调用并比较入口、返回处的计数 |
| 达到输出间隔 | 调用原有统计函数输出一次，保留累计计数 | 检查输出条件和相邻计数位置 |
| 达到结束次数 | 完成最后一次输出后关机 | 统计输出行数，读取 QEMU 退出状态 |
| 其他已知类型 | 输出对应提示，不更新时钟状态 | 构造原因值，比较提示和状态 |
| 未知原因 | 输出原因号和 TrapFrame，正常返回 | 构造未知原因，检查现场输出 |

**我：** 现有的 `ticks`、`TICK_NUM` 和打印函数都继续使用，不再另建一套计数或输出。代码中的实验标记用我的学号2412419。

**AI：** 可以。检查实现时还要确认没有修改公共分发、异常处理、寄存器布局和原有测试断言。

## 6. 检查原因码分类

**我：** 现在原因分类使用了下面的表达式。请根据 RV64 类型定义检查它是否会正确识别中断，同时保留原始现场。

```c
uintptr_t cause = tf->cause & ~((uintptr_t)1 << 63);
```

**AI：** 这里先把1转换成 `uintptr_t` 再左移，避免按普通32位整数做63位移位。取反后与原值按位与，得到不含最高标志位的原因号。结果存入局部变量 `cause`，不会改动 `tf->cause` 或 `tf->epc`。

**我：** 以 `0x8000000000000005` 为输入，后面的分支就应该收到原因号5。

**AI：** 对。可以用这个例子检查真实时钟入口，再用软件中断和未知原因检查其他分支。

## 7. 检查时钟事件与共享计数

**我：** 时钟模块需要持续触发，每次处理后共享计数只能增加一次。这部分实现如下，请结合 `clock.c` 和 `clock.h` 检查调用和计数是否合适。

```c
clock_set_next_event();
ticks++;
```

**AI：** `clock_set_next_event()` 安排的是一次后续事件，所以每次进入时钟分支都需要调用。这里随后更新已有的 `ticks`，顺序与当前框架一致。这两项操作应在每次时钟中断时执行，输出频率再单独控制。

**我：** 也就是持续触发和计数不能依赖是否到了打印时间，`ticks` 也不应在函数里重新初始化。

## 8. 检查周期输出

**我：** `TICK_NUM` 是100。我需要每处理100次时钟中断输出一次，其他次数继续计数，输出后不能清零。请检查当前这段代码是否符合要求。

```c
case IRQ_S_TIMER:
    clock_set_next_event();
    ticks++;
    if (ticks % TICK_NUM == 0) {
        print_ticks();
    }
    break;
```

**AI：** 按这段代码，计数由99变为100时会输出一次，100变为101时不会输出，199变为200时再次输出。判断放在递增之后，初始的0不会造成额外输出。`break` 使时钟分支结束后退出 `switch`。

**我：** 这些边界可以在测试时单独观察。我还要检查其他分支不会误改时钟计数。

## 9. 检查其他中断分支

**我：** 除 Supervisor 时钟外，其他已知中断只需要按类型提示，未知原因还要打印现场。它们都应保持计数和返回地址不变。请对照 `IRQ_*` 定义检查下面这些分支。

```c
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
```

**AI：** 八个分支分别对应三个级别的软件中断、另外两个级别的时钟中断，以及三个级别的外部中断，类型与提示一致。它们没有更新 `ticks`，也没有调用时钟重设接口。

**我：** 未知原因的处理是这两句，请一起检查输出内容和接口是否匹配。

```c
cprintf("Unknown interrupt: cause %lu\n", (unsigned long)cause);
print_trapframe(tf);
```

**AI：** `%lu` 与转换后的 `unsigned long` 参数匹配。原因号之外，`print_trapframe(tf)` 会打印原始现场，便于定位。检查整个函数时，还要确认每个分支能够结束，且计数操作只出现在 Supervisor 时钟分支中。

**我：** 我先保存这一版，进行编译和运行检查。

## 10. 分清源码和命令的输入位置

**我：** 终端里执行 `wsl -d Ubuntu-22.04` 提示没有这个命令，粘贴代码后又报 `cprintf` 和 `break` 错误。先帮我判断是不是输入位置弄错了。

**AI：** 提示符已经是 Ubuntu 用户和 `/mnt/d/...` 路径，说明当前就在 WSL 内。这里输入 Linux 命令即可。`cprintf`、`case` 和 `break` 是 C 代码，应该写进编辑器中的 `trap.c`，Bash 会把它们当成命令，所以报错。

**我：** 后面把源码、终端命令和 GDB 命令分开给我。我执行后再反馈输出。

**AI：** 编译和运行命令放在 Bash；出现 `(gdb)` 后再输入调试命令。用于核对的预期输出不用再次输入。

## 11. 独立编译

**我：** 我的中断处理已经写进 `trap.c`，但3号的异常处理还没有合进来。请给出只检查当前代码的编译方法，保留公共接口和编译警告，不修改对方的函数。

**AI：** 可以在 `/tmp` 中准备临时声明头，声明 `struct trapframe` 和 `exception_handler()`，通过 `-include` 引入，再用 `-c` 单独编译 `kern/trap/trap.c`。目标文件也放在 `/tmp`，无需改动公共头文件。

**我：** 执行后显示“C部分编译检查通过”。

**AI：** 说明当前代码通过了这次编译检查。`-c` 不进行完整链接，声明也不等于异常函数已经实现；下一步还需要构建测试内核运行时钟。

对应记录：[图 C1：独立编译结果](images/member2/c01-compile-check.png)。

## 12. 构建测试内核

**我：** 完整链接还依赖异常处理。请把测试准备放在独立副本里，正式源码只保留我的中断实现。另外，现在构建报 `unknown CSR 'sbadaddr'`，先定位这个错误，不要直接改公共汇编。

**AI：** 使用 `/tmp/lab3-c-test.AXvPFB` 作为测试副本，注入框架时钟测试并启用 `GRADE_LAB3_TIMER`。临时异常函数只用于满足链接，意外进入时打印现场并停止。临时 C 声明要排除汇编预处理场景。

**我：** 报错位置是 `trapentry.S:47`，读取的 CSR 名称为 `sbadaddr`。

**AI：** 当前汇编器不识别这个旧名称。可以在测试构建参数中加入 `-Dsbadaddr=stval`，由 `.S` 预处理完成名称替换，保留汇编源码。构建成功后应生成 `bin/kernel` 和 `bin/ucore.img`。

**我：** 加上参数后显示“时钟测试内核构建成功”。使用的关键参数如下：

```text
-DGRADE_LAB3_TIMER
-include /tmp/lab3-c-test.AXvPFB/c_only_declarations.h
-Dsbadaddr=stval
```

对应记录：[图 C2：CSR 报错](images/member2/c02-csr-build-error.png)、[图 C3：构建成功](images/member2/c03-csr-build-success.png)。

## 13. 核对实际运行的 QEMU

**我：** 现在只有固件输出，没有内核的时钟信息。我预期使用的是 QEMU 4.1.1，请先检查实际调用路径和版本，再判断启动代码是否有问题。

**AI：** 用 `type -a qemu-system-riscv64` 和版本输出确认。当前结果显示，默认路径为 `/opt/qemu/bin/qemu-system-riscv64`，版本是7.0.0，本机还有4.1.1的安装目录。

**我：** GDB 里 `scause = 0xc`，`sepc` 和 `stval` 都是 `0x80200038`，没有停到 `kern_init()`。

**AI：** 这是启动阶段的取指缺页，此时还没有进入中断处理函数。先用已安装的4.1.1和原启动代码重新运行，再根据结果决定下一步。这个现场本身还不能说明 QEMU 7 或你的 handler 有错误。

后续运行使用的路径为：

```text
/home/zhangyiran/lab1-tools/qemu-4.1.1-install/bin/qemu-system-riscv64
```

对应记录：[图 C4：版本与路径](images/member2/c04-qemu-version-paths.png)、[图 C5：启动异常现场](images/member2/c05-boot-page-fault.png)。

## 14. 持续时钟测试通过后的检查

**我：** 换成 QEMU 4 后，出现了连续的 `100 ticks` 和 `Test 3 ... PASS (40/40, ticks=200)`。请结合中断模块的完整要求，看看还要检查哪些地方。

**AI：** 这说明持续时钟测试已经得到预期输出。还需要确认进入 handler 时的真实原因码、重设接口的调用、计数和返回地址的变化，以及其他分支和最后的关机条件。

**我：** 接下来用 GDB 检查这些行为。测试仍在独立副本中进行，符号也加载这个副本的 `bin/kernel`。

## 15. 观察真实中断入口

**我：** 我要确认收到的确实是 Supervisor 时钟中断，并保存计数和返回地址作为比较起点。GDB 应该停在哪里、查看哪些值？

**AI：** 在 `interrupt_handler` 入口设置硬件断点，第一次命中后读取 `tf->cause`、`ticks` 和 `tf->epc`。按函数符号设置断点，地址以本次运行读取的值为准。

```gdb
hbreak interrupt_handler
continue
p/x tf->cause
p ticks
p/x tf->epc
```

**我：** 原因码是 `0x8000000000000005`，`ticks` 是0，`epc` 是 `0xffffffffc0201a18`。

**AI：** 原因码符合 Supervisor 时钟中断。入口还没有执行计数更新，所以此时为0。保存当前 `tf` 和 `epc`，之后就能比较同一次处理前后的状态。

对应记录：[图 C6：首次入口现场](images/member2/c06-timer-handler-entry.png)。

## 16. 跟踪时钟重设调用

**我：** 现在需要确认下一次事件是由本次中断处理安排的，而不只是初始化时调用过。请给出能检查调用来源和定时参数的调试步骤。

**AI：** 先保存入口的 `tf` 和 `epc`，再分别停在 `clock_set_next_event()` 和 `sbi_set_timer()`，查看调用栈、参数和计数。

```gdb
set $c_tf = tf
set $c_epc = tf->epc
thbreak clock_set_next_event
continue
bt 4
thbreak sbi_set_timer
continue
info args
p ticks
```

**我：** 调用栈里有 `interrupt_handler()` 和 `__alltraps`。到 `sbi_set_timer()` 时，`stime_value = 447943`，`ticks` 还是0。

**AI：** 这次调用来自 handler，并且发生在计数递增之前。447943是本次设置的绝对触发时间，间隔由 `get_cycles() + timebase` 中的配置决定。

**我：** GDB 还显示了 `<optimized out>` 和 `frame did not save the PC`，这会影响判断吗？

**AI：** 前者与优化编译有关，后者表示 GDB 无法继续按普通调用帧展开这段汇编。可以使用入口保存的指针，在 `__trapret` 直接比较现场，再观察下一次中断是否到达。

对应记录：[图 C7：重设调用链和实参](images/member2/c07-timer-rearm-call.png)。

## 17. 检查计数和返回地址

**我：** 我需要核对一次处理只增加一次计数，而且返回地址没有改变。请让比较发生在同一次中断的入口和返回处，避免把两次中断的值混在一起。

**AI：** 在 `__trapret` 设置临时断点，用入口保存的 `$c_tf` 和 `$c_epc` 做比较，然后继续到下一次 handler。

```gdb
thbreak __trapret
continue
p ticks
p/x $c_tf->epc
p $c_tf->epc == $c_epc
continue
p/x tf->cause
p ticks
```

**我：** 返回处 `ticks = 1`，`epc` 的比较结果为1。继续执行后再次进入 handler，原因码仍是 `0x8000000000000005`，这时 `ticks` 是1。

**AI：** 这次计数从0变为1，保存的返回位置未变，下一次时钟也成功到达。这里检查的是时钟路径和返回地址，全部通用寄存器的恢复还需要单独的上下文测试。

对应记录：[图 C8：返回前后比较与再次进入](images/member2/c08-timer-return-and-repeat.png)。

## 18. 检查软件中断和未知原因

**我：** 其他分支应输出对应提示，并保持时钟计数和返回地址不变。请设计函数输入检查。我可以用 GDB 改原因码，但需要分清它和真实硬件触发的区别。

**AI：** 可以在已经进入 `interrupt_handler()`、尚未读取原因码时，保存 `tf`、`cause`、`epc` 和 `ticks`，再临时替换输入。Supervisor 软件中断使用 `0x8000000000000001`，检查返回处的状态和控制台输出，之后恢复原因值。

**我：** 控制台输出了 `Supervisor software interrupt`，计数和 `epc` 的相等比较都返回1。

**AI：** 提示和状态符合预期。这次改变的是函数输入，没有设置硬件的软件中断挂起位。

**我：** 未知原因设为15后，输出了 `Unknown interrupt: cause 15` 和 TrapFrame，两个比较也是1，最后恢复了原值。

**AI：** 这检查了 `default` 分支。15没有最高中断标志位，但修改时已经进入 handler，因此不涉及公共分发如何判断真实 `scause=15`。另外，这次替换了时钟原因，时钟分支没有重设下一次事件，最终持续运行测试需要重新启动。

**我：** 现场文本中的 `epc` 只有 `0xc0201a18`，与 GDB 看到的位数不同。

**AI：** 公共打印函数使用了 `%08x` 格式。完整地址是否改变，以 GDB 读取和比较的结果为准，不能仅凭打印的位数判断地址被截断。

对应记录：[图 C9：软件分支检查](images/member2/c09-software-input-check.png)、[图 C10：软件分支输出](images/member2/c10-software-console-output.png)、[图 C11：未知原因检查](images/member2/c11-unknown-input-check.png)、[图 C12：现场输出](images/member2/c12-unknown-trapframe.png)。

## 19. 检查并补齐结束条件

**我：** Test 3 已经通过，但时钟还在一直输出。模块要求累计输出到设定次数后结束，请帮我检查遗漏了什么，说明需要维护的状态和复测方法，具体代码由我自己补充。

**AI：** 当前实现少了累计输出后的关机。`ticks` 记录中断次数，还需要记录已经输出了多少次统计信息，这个值要能跨多次调用保留。达到输出阈值后，使用已有的 `sbi_shutdown()`。

**我：** 这次就以10次统计输出作为结束条件，`TICK_NUM` 仍用原来的100。我先在自己的函数里补上这部分，不改其他文件。

**AI：** 注意区分中断次数和输出次数，并检查未到阈值时是否正常返回、达到阈值时是否先完成最后一次输出。你写好后，把代码贴出来检查。

**我：** 下面是我补充后的代码，请帮我检查计数的保存方式、关机条件，以及原有的分类和返回行为。

```c
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
```

**AI：** 代码使用局部静态变量保存输出次数，每次统计输出后更新，达到阈值时调用关机接口。原有时钟重设、`ticks` 计数和其他中断分支保持不变，也没有修改 `epc`。接下来通过运行结果确认输出次数和退出状态。

**我：** 我重新编译并运行这一版，再把实际结果发给你检查。

**AI：** 复测时重新启动内核，避免前面的 GDB 输入修改影响结果。

## 20. 验证关机结果

**我：** 怎样确认是达到次数后正常关机，而不是卡住后被超时命令结束？

**AI：** 使用 QEMU 4.1.1完整路径运行当前测试内核，检查控制台中 `100 ticks` 的输出次数和实际运行命令的退出状态，确认达到次数后正常结束。

**我：** 这次运行结束后的结果是：

```text
[Test 3] Sustained supervisor timer: PASS (40/40, ticks=200)
QEMU_EXIT_STATUS=0
TICK_OUTPUT_LINES=10
```

**AI：** 持续时钟测试通过，输出恰好10行，退出状态为0，与设定的关机条件一致。按每100次中断输出一次推算，关机对应第1000次中断；这里直接记录的是输出行数和退出状态。

**我：** 我把这次结果单独截图保存，前面的 GDB 截图用于说明调试过程。

对应记录：[图 C13：最终结果](images/member2/c13-timer-shutdown-result.png)。退出状态和行数由 Shell 在运行后打印，保存在截图中。

## 21. 检查交付范围

**我：** Git 最初显示很多文件变化。请检查真正的源码改动，并确认独立测试中的临时文件没有进入交付代码。

**AI：** 排除 Windows 与 WSL 的行尾差异后，源码统计只有 `code/kern/trap/trap.c`，62行增加、1行删除，对应学号标记和 `interrupt_handler()`。交付函数补丁和截图即可，合并时按函数核对，保留其他成员在同一文件中的实现。

**我：** 异常占位和临时声明留在测试副本里。我的40分结果应该怎样表述？

**AI：** 可以记录 Test 3 的40分输出，以及已检查的软件中断和未知原因分支。小组完整评分还需要合入正式异常处理等模块，再运行 `make grade`，目前没有完整的 `Total Score: 100/100` 结果。

对应记录：[图 C14：源码差异统计](images/member2/c14-code-diff-scope.png)、[本人函数补丁](C_2412419/interrupt_handler.patch)。

## 22. 学习与调试提示词

**我：** 把学习和调试这部分代码的要求整理成一份提示词，写清模块目标、必要依赖、接口和验证方法。AI 只提供基础框架、解释和检查建议，具体实现和测试由我完成。

**AI：** 可以这样说明任务和协作方式：

```text
【Prompt：任务说明】

我负责 RISC-V uCore Lab3 的中断处理模块，姓名张祎然，学号2412419，
报告编号为2号，开发分工为成员 C。
我将在 code/kern/trap/trap.c 中本人负责的 lab3 function2 区域编写实现，
包括原因识别、持续时钟、计数与统计输出、累计输出后关机，以及其他中断提示。
请解释相关接口和要求，只提供不含具体处理逻辑的基础框架。函数体由我自己填写；
我贴出代码后，请指出问题、解释原因并给出检查建议，不代写函数体或替换我的实现。

本人实现时遵守以下范围：
- 保留本人实验标记，将对应的 YOUR CODE 替换为学号2412419。
- 保持公共接口、结构体和宏定义不变，不修改 IRQ_*、TICK_NUM、TrapFrame，
  不修改 SAVE_ALL、RESTORE_ALL、__alltraps、__trapret、trap、trap_dispatch、
  exception_handler，以及时钟驱动、SBI、启动代码、公共头文件和构建配置。
- 保留已有的中断使能与保护、现场保存与恢复、共享计数、打印和固件调用方式。
  不修改评分脚本、测试函数、断言或编译警告选项。
- 需要内部辅助函数时，由我在本人区域内编写，不增加公共接口。

请给出基础框架、接口说明和检查步骤，每次先解释当前问题，等我贴出代码或反馈结果后
再继续检查。编译、运行和调试命令由我执行，源码、Bash 命令、GDB 命令和预期输出分开。
先验证局部编译，再检查运行行为；其他模块未合入时，我只在独立测试副本中准备临时依赖，
不将它们加入交付补丁。
最后指导我核对修改范围、导出自己的函数补丁，并整理实际结果和未验证内容。

【Rely：依赖说明】

请结合下列文件中与本模块有关的定义和接口，帮助我理解输入和调用关系：
- trap.h、defs.h：struct trapframe、字段含义及 RV64 数据类型。
- trap.c：本人实现区域、公共分发调用、TICK_NUM、print_ticks、print_trapframe。
- riscv.h：IRQ_* 原因号。
- clock.h、clock.c：共享 ticks、初始化约定、clock_set_next_event 的单次定时语义。
- sbi.h、sbi.c：现有定时与关机接口。

验证时查看 test_lab3.c 中的持续时钟和 handler 分支测试，以及 grade.sh 的对应检查。
报错涉及其他文件时，再根据位置补充阅读。运行前确认实际 QEMU 路径和版本，
本次使用已安装的4.1.1；sbadaddr 名称兼容问题在测试构建参数中处理。

【Guarantee：接口清单】

我的实现需要保持以下对外接口：
void interrupt_handler(struct trapframe *tf);

该入口保留外部链接属性，供公共分发和现有测试调用。
输入为有效的、已保存现场的 TrapFrame。正常处理后返回调用者，由公共汇编恢复现场；
达到结束条件时使用现有 sbi_shutdown 请求关机。
继续使用框架的 ticks、clock_set_next_event、print_ticks 和 print_trapframe。
我编写的辅助函数应为私有接口，检查时说明其输入、输出和允许修改的状态。

【Specification：行为规格】

以下用于检查本人完成的代码。
前置条件：当前为 RV64 单核运行，未主动开启嵌套中断；tf 有效，时钟和中断初始化
由框架完成。干净启动时 ticks 和累计输出次数为0。TICK_NUM 保持100，
本次 SHUTDOWN_AFTER 配置为10次统计输出。

S1 原因分类与现场保持
根据去掉最高中断标志位后的原因号分类。原始 cause、epc、status、badvaddr 和
通用寄存器保存区保持不变，不跳过被中断指令。
检查0x8000000000000005和0x8000000000000001分别对应 Supervisor 时钟和软件中断，
并比较处理前后的 TrapFrame。

S2 Supervisor 时钟处理
每次处理安排一次后续时钟事件，且共享 ticks 从进入时的n恰好增加到n+1。
沿用框架接口，时钟重设发生在计数更新之前；未达到关机条件时正常返回。
用 GDB 检查 handler 到 clock_set_next_event、sbi_set_timer 的调用，
比较入口与 __trapret 的计数，并观察下一次中断能否到达。

S3 周期输出
更新后的 ticks 为 TICK_NUM 整数倍时，调用已有 print_ticks 输出一次，
累计输出次数增加一次；其余时钟处理中不输出统计行。ticks 不清零，输出次数跨调用保留。
检查98→99不输出、99→100输出一次、100→101不输出、199→200再次输出。

S4 结束条件
累计输出不足10次时继续运行；完成第10次统计输出后调用 sbi_shutdown。
检查第9次输出后尚未关机、第10次输出后结束。独立运行应得到 Test 3 的
ticks=200 PASS、10行100 ticks及 QEMU_EXIT_STATUS=0。

S5 其他已知中断
IRQ_U_SOFT、IRQ_S_SOFT、IRQ_M_SOFT、IRQ_U_TIMER、IRQ_M_TIMER、
IRQ_U_EXT、IRQ_S_EXT、IRQ_M_EXT 分别输出对应级别的软件、时钟或外部中断提示。
提示文本与框架约定一致。处理后 ticks、累计输出次数和 TrapFrame 不变，
不重设定时器、不输出100 ticks、不请求关机，正常返回。
逐项构造函数输入，比较输出、状态和接口调用。

S6 未知原因
不属于已知类型时，输出原因号并调用 print_trapframe(tf) 打印原始现场，随后返回。
ticks、累计输出次数和 TrapFrame 不变，不重设定时器或请求关机。
分别用15和0x800000000000000f检查，均应输出 Unknown interrupt: cause 15。
这些输入用于直接检查 handler，不代替公共分发或真实硬件触发测试。

验证时保留原有测试，边界和分支检查在独立副本中进行。人工修改输入后，
重新启动当前内核完成最终持续运行与关机检查。完整 make grade 在正式模块合并后执行。
```

**我：** 我现有的截图已经验证了哪些要求？

**AI：** 已有记录包括真实时钟原因、重设调用、计数从0到1和 `epc` 保持，以及持续时钟和10次输出后的正常退出。软件中断和直接输入15的分支也做过检查。其余已知类型、带最高标志位的未知原因、全部现场字段和输出临界点，还没有逐项测试记录，后续需要补充。

实现说明和实验图片见 [report.md](report.md) 的2号部分。

## 2413788-张旗庭：异常处理

## 一、基本信息

- **课程名称**：操作系统
- **实验名称**：实验三
- **小组成员**：3号，2413788 张旗庭
- **负责模块**：`exception_handler(struct trapframe *tf)` 异常处理函数
- **使用的 AI 工具**：ChatGPT、Codex（VS Code）
- **记录说明**：以下按照实际交流内容整理，概括本人向 AI 提出的问题及 AI 的主要回复，不是逐字对话记录。实际发送给 Codex 的代码生成提示词另外保留。

## 二、实验任务理解与代码实现

### 问题1：本次实验三需要完成什么？我负责的部分是什么？

**我的提问：**

希望 AI 帮助梳理实验三的整体要求、小组分工，以及本人需要实现的功能。

**AI 回复摘要：**

本实验主要实现 RISC-V uCore 的 Trap 处理机制，包括 Trap 入口、上下文保存与恢复、中断处理和异常处理。本人负责 `trap.c` 中的 `exception_handler()`，主要完成：

1. 识别非法指令异常和断点异常。
2. 输出对应的异常信息。
3. 根据异常指令长度更新 `tf->epc`。
4. 对其他异常调用 `print_trapframe()` 输出异常现场。

### 问题2：异常处理函数与其他模块有什么关系？

**我的提问：**

询问 `trap.c`、`trapentry.S`、`trap.h` 等文件的作用，以及异常处理函数如何被调用。

**AI 回复摘要：**

AI 解释了完整的 Trap 调用流程：

```text
异常发生
    ↓
__alltraps → SAVE_ALL
    ↓
trap() → trap_dispatch()
    ↓
exception_handler()
    ↓
__trapret → RESTORE_ALL
    ↓
sret
```

其中 `trapframe` 用于保存处理器现场，`tf->cause` 用于判断异常类型，`tf->epc` 保存异常返回地址。异常处理函数只负责具体处理和更新 EPC，上下文恢复由汇编代码完成。

### 问题3：如何设计提示词，让 Codex 只实现我负责的函数？

**我的提问：**

希望 AI 帮助明确代码修改范围、相关依赖、实现要求和验证方式，避免与其他组员的代码发生冲突。

**AI 回复摘要：**

AI 建议在提示词中明确任务背景、函数接口、需要阅读的源码、异常处理逻辑以及修改限制。

随后将任务交给 Codex，要求只修改 `exception_handler()`，不修改其他成员的函数、不改动公共框架、不擅自执行 Git 提交。

另外，根据老师要求的 `[PROMPT]`、`[RELY]`、`[GUARANTEE]`、`[SPECIFICATION]` 四段式框架，对原始提示词进行了结构化整理。该四段式版本属于后续整理，并非最初发送给 Codex 的原始文本。

### 问题4：为什么异常处理时需要区分16位和32位指令？

**我的提问：**

询问 RISC-V 指令长度的判断依据，为什么不能统一将 EPC 增加4字节。

**AI 回复摘要：**

RISC-V 支持16位压缩指令和32位普通指令。在本实验涉及的指令范围内，可以通过指令编码最低两位判断长度：

- 最低两位不为 `11`：按16位指令处理，EPC 增加2字节。
- 最低两位为 `11`：按32位指令处理，EPC 增加4字节。

因此采用以下代码：

```c
uint16_t insn = *(const volatile uint16_t *)tf->epc;
tf->epc += (insn & 0x3) == 0x3 ? 4 : 2;
```

AI 同时解释了该方法与 RISC-V 指令编码规则的关系，以及为什么只需要读取指令的前16位。

## 三、代码检查与集成测试

### 问题5：Codex 生成代码后，应该如何验证？

**我的提问：**

询问代码生成完成后需要检查哪些内容，如何确认异常处理函数正确。

**AI 回复摘要：**

AI 建议先检查源码差异和编译情况，再利用 QEMU 与 GDB 验证实际行为，重点包括：

- `tf->cause` 是否正确匹配异常类型。
- 输出信息是否符合测试要求。
- EPC 是否按照指令长度正确更新。
- Trap 返回是否使用更新后的地址。
- 默认分支是否调用 `print_trapframe()`。

同时强调不能仅根据 AI 生成代码的结果判断实验通过，必须进行实际编译和运行验证。

### 问题6：为什么一开始无法完成内核完整编译？

**我的提问：**

询问编译错误的原因，以及如何在不覆盖队友代码的情况下测试自己的实现。

**AI 回复摘要：**

分析发现，最初的代码尚未集成另一位成员的 `interrupt_handler()`，因此不能完成完整内核编译和链接。

AI 建议在独立测试分支中整合相关函数，而不是直接修改公共 `lab3` 分支。

随后使用 `test/lab3-integration` 临时分支完成代码集成，并成功执行编译、QEMU 运行和自动评分。

### 问题7：`inject.py` 和 `GRADE_LAB3_CONTEXT`、`GRADE_LAB3_ILLEGAL` 是什么？

**我的提问：**

询问为什么需要注入测试代码，编译宏有什么作用，以及这些宏是否存在于源码中。

**AI 回复摘要：**

`inject.py` 用于在实验测试副本中加入测试代码及相应配置，使内核主动执行指定测试。

`GRADE_LAB3_CONTEXT` 和 `GRADE_LAB3_ILLEGAL` 是用于选择测试内容的编译宏，通过 GCC 的 `-D` 选项传入。

其中：

- `GRADE_LAB3_CONTEXT`：主要测试 Trap 上下文保存恢复和断点异常。
- `GRADE_LAB3_ILLEGAL`：主要测试非法指令异常及其他处理分支。

测试代码与相关配置可以在 `code/tools/` 下查看。为了不影响正式源码，测试在 `/tmp/ucore_lab3_b_gdb` 临时目录中进行。

## 四、GDB 调试与问题排查

### 问题8：如何验证16位断点异常的 EPC 更新？

**我的提问：**

询问 GDB 中应该观察哪些寄存器和变量，以及如何确认 EPC 增加2字节。

**AI 回复摘要：**

AI 指导在 `exception_handler()` 设置断点，检查 `tf->cause`、`tf->epc` 和异常指令编码。

实际观察到：

```text
cause = 3
指令编码 = 0x9002（c.ebreak）
原始 EPC = 0xffffffffc0201b04
更新 EPC = 0xffffffffc0201b06
```

由于 `0x9002` 是16位压缩断点指令，因此 EPC 增加2字节，符合预期。

### 问题9：为什么使用 `finish` 后没有观察到 EPC 变化？

**我的提问：**

最初尝试使用 GDB 的 `finish` 观察函数执行结果，但 EPC 的差值仍然是0，询问原因和解决方法。

**AI 回复摘要：**

AI 分析认为，编译优化及断点位置可能导致 GDB 的停止位置不符合预期，因此建议使用硬件监视点直接观察 EPC 所在内存的变化：

```gdb
set $saved_tf = tf
set $old_epc = tf->epc
disable 1
watch -location $saved_tf->epc
continue
```

继续运行后，硬件监视点成功捕获 EPC 的修改。

AI 进一步解释：真正修改 EPC 的是 `exception_handler()` 中的代码，GDB 监视点只负责在数据变化时暂停程序，并不会主动修改 EPC。

### 问题10：EPC 更新后，程序如何真正返回到下一条指令？

**我的提问：**

询问 `tf->epc`、`sepc` 和 PC 的关系，以及 `RESTORE_ALL` 和 `sret` 的具体作用。

**AI 回复摘要：**

AI 解释了三个概念：

- `tf->epc`：内存中保存的异常返回地址。
- `sepc`：CPU 中记录异常返回地址的 CSR 寄存器。
- PC：处理器当前执行的指令地址。

异常处理函数先修改 `tf->epc`，随后 `RESTORE_ALL` 将更新后的地址恢复到 `sepc`，最后执行 `sret`，CPU 才真正跳转到新的地址继续执行。

通过 GDB 单步调试，验证了16位断点异常返回后 PC 为 `0xffffffffc0201b06`。

### 问题11：如何验证32位非法指令异常？

**我的提问：**

询问非法指令测试的编译和调试步骤，以及如何判断 EPC 是否正确增加4字节。

**AI 回复摘要：**

通过启用 `GRADE_LAB3_ILLEGAL` 编译测试内核，并在 GDB 中检查异常现场。

实际观察到：

```text
cause = 2
指令前16位 = 0x007b
原始 EPC = 0xffffffffc0201ab4
更新 EPC = 0xffffffffc0201ab8
```

由于指令最低两位为 `11`，本次按照32位指令处理，EPC 增加4字节。

随后单步观察 `sepc` 恢复和 `sret` 返回，确认 PC 到达 `0xffffffffc0201ab8`。

### 问题12：`test_lab3_handler_fallbacks()` 是什么？源码在哪里？

**我的提问：**

询问 fallback 的含义、测试函数的位置，以及默认异常处理分支是如何被验证的。

**AI 回复摘要：**

Fallback 表示没有匹配专门处理分支时采用的默认处理逻辑。

测试函数来自实验框架的 `code/tools/test_lab3.c`，通过人工构造 TrapFrame，设置 `tf.cause = 20` 并调用 `exception_handler()`。

因为原因码20不属于本次专门处理的非法指令异常和断点异常，所以函数进入 `default` 分支，调用 `print_trapframe()` 输出现场。

GDB 调用栈验证了这一过程。


## 五、AI 协作总结

本次实验中，我主要使用 ChatGPT 辅助理解实验要求、分析 Trap 处理流程、设计提示词、制定 GDB 调试方案和整理实验报告；使用 Codex 阅读工程源码并生成异常处理函数。

在实际操作中，AI 帮助我较快理解了 `exception_handler()` 的职责和相关接口，但代码生成并不意味着功能已经得到验证。特别是在编译环境检查、模块集成和 GDB 调试过程中，仍然需要根据真实输出判断问题，并调整调试方案。

最终，通过编译检查、QEMU 运行、GDB 硬件监视点和汇编单步调试，验证了16位断点异常、32位非法指令异常以及默认处理分支，并在临时集成分支上取得 `make grade` 100/100 的结果。

本次 AI 协作的主要收获是：**明确任务和接口约束能够提高 AI 生成代码的准确性，而实际编译、运行和调试是验证 AI 输出是否正确的必要环节。**
