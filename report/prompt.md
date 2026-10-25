# Lab2 Prompt 汇总
## 2413669 张懿鸾 （1号）


> 本实验围绕物理内存的发现、初始化和分配展开：内核首先选择并初始化物理内存管理器，再从设备树获取物理内存范围，由`page_init()`建立物理页描述符并将可用页面交给物理内存管理器；随后在理解 First-Fit 分配流程的基础上，实现并接入 Best-Fit 分配器，并通过 Buddy System 和 SLUB 扩展任务进一步理解页级分配与任意大小内存对象分配。整体实现链可以概括为`pmm_init() -> init_pmm_manager() -> page_init() -> init_memmap() -> alloc_pages()/free_pages()`，最终使内核能够识别、组织和分配可用物理内存。  请先读取一下整个文件夹的内容，不要修改任何文件，整理一下这个文件夹的内容，告诉我各文件夹在这个实验中发挥着怎样的作用。

<details><summary>3 previous messages</summary>

> 我会以只读方式梳理仓库目录、构建入口、实验文档和关键源码，不做任何修改；随后按“启动构建、平台与设备树、物理内存、扩展任务、测试与工具”说明各目录在实验链路中的作用。
>
> 目录规模不大，主体是 `code/` 下的 RISC-V 内核最小实验工程，另有空的报告模板和图片目录。接下来我会读取构建文件、内存管理源码、设备树解析、启动入口以及测试脚本，重点确认 Best-Fit、Buddy/SLUB 在当前仓库中究竟是已有实现、占位接口还是仅存在于题目描述。
>
> 核心链路已经确认：工程确实围绕 `kern_init()` 进入 `pmm_init()`，设备树提供物理内存基址与大小，First-Fit 可作为参照实现；当前 `pmm.c` 的 `page_init()` 和 `best_fit_pmm.c` 的核心分配逻辑仍是待补全位置。测试并不是常驻源码的一部分，而是在评分时由 `inject.py` 注入到临时副本中运行。
</details>

已完成只读检查。仓库当前是一个基于 RISC-V 64、可在 QEMU 中运行的最小内核实验骨架，主体对应 Lab2 物理内存管理。`git status` 保持干净，未修改任何文件，也未运行会产生 `obj/`、`bin/` 的构建命令。

## 目录结构

```text
OS-Lab/
├── README.md
├── code/
│   ├── Makefile
│   ├── kern/
│   │   ├── init/
│   │   ├── driver/
│   │   ├── mm/
│   │   ├── libs/
│   │   └── debug/
│   ├── libs/
│   └── tools/
├── report/
│   └── images/
├── .git/
├── .agents/          # 空目录
├── .aws/             # 空目录
└── .codex/           # 空目录
```

## 各目录作用

| 目录 | 在实验中的作用 |
|---|---|
| 根目录 | 仓库入口。`README.md` 只有项目标题；`.git` 保存版本记录；`.agents`、`.aws`、`.codex` 均为空，不属于实验代码。 |
| `code/` | 完整实验工程根目录，包含内核、基础库、构建系统和评测工具。所有实际实现都从这里展开。 |
| `code/kern/init/` | 内核启动流程。`entry.S` 保存 `hartid` 和 DTB 物理地址，建立初始 Sv39 映射、设置栈并跳入 `kern_init()`；`init.c` 清空 BSS，依次执行 `dtb_init()`、`cons_init()`、打印内核信息，最后调用 `pmm_init()`。 |
| `code/kern/driver/` | 外部平台信息与最底层 I/O。`dtb.c` 解析设备树 `memory` 节点的 `reg` 属性，得到物理内存基址和大小，供 PMM 查询；`console.c` 通过 SBI `ecall` 完成字符输出和输入。 |
| `code/kern/mm/` | 本实验核心目录，对应“发现—初始化—分配—回收”的完整内存管理逻辑。 |
| `code/kern/libs/` | 内核层的基础 I/O。`stdio.c` 将格式化输出接到控制台，是 `cprintf()`、调试信息和测试结果的输出通道。 |
| `code/kern/debug/` | 内核断言和致命错误处理。`assert()` 失败时调用 `panic()`，在物理页状态或分配结果不合法时终止运行。 |
| `code/libs/` | 不依赖标准 C 库的公共基础库，包括双向链表、字符串和内存函数、格式化打印、SBI 调用封装，以及 RISC-V CSR、Sv39、指令编码和常量定义。 |
| `code/tools/` | 构建、链接、调试和评分工具。Makefile 文件生成规则、QEMU/GDB 支持、实验测试及评分注入逻辑都在这里。 |
| `report/` | 实验报告目录。`report.md` 和 `prompt.md` 当前为空，`images/` 只有 `.gitkeep`，目前没有报告内容。 |

## `kern/mm` 内部对应关系

- [pmm.h](./code/kern/mm/pmm.h:13)：定义可替换的 `struct pmm_manager` 接口，包括 `init`、`init_memmap`、`alloc_pages`、`free_pages`、`nr_free_pages` 和 `check`，同时提供 `Page` 与物理地址、页号之间的转换函数。
- [pmm.c](./code/kern/mm/pmm.c:66)：全局 PMM 入口。当前调用顺序是：

  ```text
  pmm_init()
    -> init_pmm_manager()
    -> page_init()
    -> check_alloc_page()
         -> alloc_pages() / free_pages()
  ```

  目前它明确选择 `default_pmm_manager`。
- [default_pmm.c](./code/kern/mm/default_pmm.c:46)：完整的第一适配分配参考实现。空闲页按物理地址顺序组成一个双向链表，分配时从左到右找到第一个足够大的连续块；释放时尝试与前后相邻空闲块合并。
- [best_fit_pmm.c](./code/kern/mm/best_fit_pmm.c:61)：Best-Fit 任务骨架，提供了 `basic_check()` 和 `best_fit_check()`，但真正的初始化、分配、释放和 manager 注册逻辑仍待补全。
- [memlayout.h](./code/kern/mm/memlayout.h:31)：定义物理页描述符 `struct Page`、页状态标志、空闲区结构和内核/物理内存映射范围。
- [mmu.h](./code/kern/mm/mmu.h:8)：定义 4 KiB 页大小、页号计算及 Sv39 页表相关常量。

## 当前完成度

这份仓库是实验骨架，而不是已经完成的最终版本：

- `pmm.c` 调用了 `page_init()`，但仓库中尚无该函数实现。
- `init_pmm_manager()` 当前仍选择 First-Fit；Best-Fit 只是文件骨架，尚未注册为当前 manager。
- `best_fit_pmm.c` 中虽然已有检查函数，但还缺少自己的空闲链表状态定义和实际分配算法。
- 仓库中没有 Buddy System 或 SLUB 的独立源码、头文件或目录。题目中的 Buddy/SLUB 是后续扩展方向，目前没有对应代码。
- `report/` 是空模板，尚不能作为已完成报告使用。

## 构建与测试目录

[code/Makefile](./code/Makefile:1) 负责编译 `libs/` 与 `kern/` 中的 C/汇编文件，使用 `tools/kernel.ld` 链接到虚拟地址 `0xFFFFFFFFC0200000`，再生成可加载镜像。常用入口包括 `make`、`make qemu`、`make debug`、`make gdb` 和 `make grade`。

[tools/grade.sh](./code/tools/grade.sh:214) 在 QEMU 中检查页初始化与 Best-Fit 是否接入；[tools/inject.py](./code/tools/inject.py:1) 会把评分测试注入 `/tmp` 下的临时副本，不污染原源码；[test_lab2.c](./code/tools/test_lab2.c:12) 检查内存范围、`npage`、`pages` 数组、内核占用页保留以及 Best-Fit manager 选择。

另外，`boot.ld` 和 `kernel_nopage.ld` 是遗留的 i386 链接脚本，并未被当前 Makefile 使用；当前实际生效的是 `kernel.ld`。

> 最后make的时候是要运行make grade吗？还是原来的make qemu？

最终验收应运行 `make grade`，`make qemu` 只用于本地启动调试。

两者的区别：

- `make`：只编译内核。
- `make qemu`：编译并启动内核，但不会注入评分测试，因此通常看不到 `[Test 1] Page Init: PASS` 和 `[Test 2] Best-Fit: PASS`。
- `make grade`：复制工程到 `/tmp`，注入测试，编译并在 QEMU 中检查，最后给出总分。是否通过应以它为准。
- `make handin`：如果还需要生成提交压缩包，再运行这个。

推荐流程：

```bash
cd /home/win/oslab/OS-Lab/code
make qemu     # 开发时检查内核能否启动
make grade    # 完成后的最终评测
```

在当前代码下，必须先实现 `page_init()` 和 Best-Fit，否则 `make grade` 无法通过。

> 请解释一下make grade具体会做什么

`make grade` 不是普通的编译命令，而是一次隔离的自动评测。它会在 `/tmp` 中复制一份工程、注入隐藏测试、重新构建并在 QEMU 中运行，最后检查串口输出并计分。

需要在 `code/` 目录运行：

```bash
cd /home/win/oslab/OS-Lab/code
make grade
```

## 具体流程

1. 先清理当前工程

根据 [Makefile](./code/Makefile:203)，它会运行 `make clean`，删除当前 `code/` 下的：

- `obj/`
- `bin/`
- `.gdb.in`
- `.qemu.out`
- `cscope*`、`tags`

这一步会删除构建产物，但不会删除源码。

2. 复制到临时评测目录

整个 `code/` 会被复制到：

```text
/tmp/lab2_grade
```

复制时排除 `.git`、`.vscode` 等目录。因此测试修改不会落在你的原始工程中。

3. 注入评分测试

[inject.py](./code/tools/inject.py:126) 在临时副本中做四件事：

- 把 `tools/test_lab2.c` 和 `test_lab2.h` 复制到 `kern/test/`；
- 修改临时 Makefile，把 `kern/test` 加入头文件和源码目录；
- 在临时 `pmm.c` 中加入 `#include <test_lab2.h>`；
- 在 `pmm_init()` 末尾插入：

```c
test_page_init();
test_best_fit();
```

所以测试发生在物理内存初始化和 manager 检查完成之后。

4. 在临时目录重新构建

[grade.sh](./code/tools/grade.sh:214) 会调用临时工程中的 `make`，完成编译、链接和 `ucore.img` 生成。编译器启用了 `-Werror`，因此编译警告也可能导致构建失败。

5. 启动 QEMU 并采集串口输出

评分脚本不使用普通的 `make qemu`，而是直接启动：

```text
QEMU + virt 机器 + OpenSBI + ucore.img
```

内核加载到 `0x80200000`，串口输出写入 `.qemu.out`，并设置默认 30 秒的 CPU 时间上限。内核进入 `while (1)` 不会主动退出，因此评分主要依靠它此前打印的测试结果。

6. 检查固定的 PASS 行

[grade.sh](./code/tools/grade.sh:239) 会检查串口日志中是否精确出现：

```text
[Test 1] Page Init: PASS
[Test 2] Best-Fit: PASS
```

两项各 10 分，总分 20 分。只要缺少对应行、内核 panic、编译失败或 QEMU 没有输出，该项就会失败。

7. 输出总分

最终会打印类似：

```text
Total Score: 20/20
```

如果总分没有达到满分，脚本返回非零状态，`make grade` 也会以失败结束。

## 两项测试检查什么

[Test 1](./code/tools/test_lab2.c:12) 检查 `page_init()` 的结果：

- `va_pa_offset` 是否为规定的虚实偏移；
- DTB 是否报告了非零物理内存范围和大小；
- `npage` 是否等于设备树内存末端的页号；
- `pages` 页描述符数组是否非空、页对齐且位于内核 `end` 之后；
- 是否建立了合理的空闲页数量；
- 内核镜像最后一个页是否保持保留状态；
- `pages` 数组自身占用的页面是否保持保留状态；
- `page2pa()` 和 `pa2page()` 能否正确往返转换。

[Test 2](./code/tools/test_lab2.c:104) 直接确认当前 manager 是否为：

```c
pmm_manager == &best_fit_pmm_manager
```

另外，`pmm_init()` 平时会调用 `pmm_manager->check()`。因此完整实现还应当让 Best-Fit manager 的 `.check` 指向 [best_fit_check()](./code/kern/mm/best_fit_pmm.c:61)，这样 Best-Fit 的分配、分割、释放及相邻块合并等行为才会得到实际验证。

## 与普通 make qemu 的区别

| 命令 | 作用 |
|---|---|
| `make` | 只构建 |
| `make qemu` | 构建并启动内核，不注入测试 |
| `make grade` | 在临时副本中构建、注入测试、运行 QEMU 并评分 |

因此，本地调试可以用 `make qemu`，最终检查是否通过应使用 `make grade`。成功完成后 `/tmp/lab2_grade` 会被删除；如果评测失败，Makefile 可能停在中途，临时目录可能保留下来用于排查。

> 原来如此，那我们就先开始做第一部分的实验内容吧。
>
> [PROMPT]
>
> 任务说明：
>
> 请在当前操作系统lab2的这个uCore 项目中，完成`kern/mm/pmm.c`里的 `page_init()` 函数。任务目标是根据 OpenSBI 传递的DTB信息，获取当前机器的物理内存范围，结合内核自身的内存布局，为物理内存建立 `Page`描述符数组，并将真正可以使用的连续物理内存区域交给现有物理内存管理器。实现应当符合实验指导书对物理内存探测和初始化的要求，并与当前工程已有的地址映射方式、数据结构和初始化流程保持一致。
>
> 操作要求：
>
> 先阅读当前工程的实际源码，确认相关函数、宏、类型和调用关系，再进行实现。直接修改项目中的 `kern/mm/pmm.c`，不能只在对话框中展示代码。只修改完成本任务所必需的内容，不得顺带重构其他模块或重写其他区域的代码，也不得修改测试区域代码。如果存在不理解的问题，先暂停修改说明问题，我们讨论框定之后再说。
>
> 输出要求：
>
> 完成后，`page_init()` 应当能够正确完成物理内存探测、页面描述符布局和初始空闲物理页区域的建立。
>
> 1. 正确设置 `va_pa_offset`，与工程现有的虚拟地址和物理地址转换约定一致。
> 2. 正确获取、检查并记录物理内存的起始地址和大小。
> 3. 正确计算 `npage`，并保证它符合当前工程对物理页号上界的定义。
> 4. 在内核镜像结束位置之后正确安排 `pages` 数组，并为数组自身占用的内存保留空间。
> 5. 所有不能被分配的页面均不得进入初始空闲页区域。
> 6. 初始空闲区域只能包含合法范围内的完整物理页。
> 7. 通过现有 `init_memmap()` 接口将初始空闲区域交给当前物理内存管理器。
> 8. 对无效的内存信息、地址越界和没有可用完整物理页等异常情况进行合理处理。
>
> ## [RELY]
>
> - `kern/driver/dtb.c` 及对应头文件。
> - `get_memory_base()`：获取 DTB 解析出的物理内存起始地址。
> - `get_memory_size()`：获取 DTB 解析出的物理内存大小。
> - `dtb_init()`：了解内存信息何时被解析和保存。需要确认 `dtb_init()` 与 `pmm_init()` 的调用顺序，以及内存信息未能正确获取时的现有处理方式。
>
> ###
>
> - `kern/mm/pmm.c` 中已有的 `va_pa_offset`、`nbase`、`npage`、`pages`。
> - `kern/mm/pmm.h` 中的 `PADDR()`、`pa2page()` 等地址转换接口。
> - `kern/mm/memlayout.h` 中的 `struct Page`、`DRAM_BASE` 和相关页面状态定义。
> - 工程实际提供的 `PGSIZE`、`PGSHIFT`、`PPN()`、地址对齐宏及物理地址范围限制。
> - 内核链接脚本中的 `end` 符号。
>
> (`base` 表示 DRAM 起始地址对应的物理页号。
>
> - `npage` 表示可管理物理页号的上界，不应直接当作物理页总数。
> - `pages` 是 Page 描述符数组的起始地址。
> - 描述符数组需要覆盖 `[nbase, npage)` 对应的物理页，因此数组所需的描述符数量为 `npage - nbase`。
> - `end` 是链接脚本提供的符号，需要根据现有地址映射约定区分其虚拟地址与对应的物理地址。
>
> ###
>
> - `kern/mm/pmm.c` 中的 `init_memmap()`。
> - `kern/mm/pmm.h` 中的 `struct pmm_manager`。
> - `kern/mm/default_pmm.c` 中的 `default_init_memmap()`。
>
> `init_memmap()` 负责将一段连续的空闲物理页交给当前物理内存管理器。请根据现有实现确认传入参数的含义、描述符初始化要求和空闲页数量维护规则。
>
> 本任务只需要正确调用已有接口，不需要重新实现 First-Fit 或 Best-Fit 分配算法。
>
> ## [GUARANTEE]
>
> ### 1. 必须完成的实现
>
> 必须完成现有接口：
> ```c
> static void page_init(void);
> ```
>
> 该函数应当负责：
>
> - 初始化地址映射相关变量。
> - 获取并验证物理内存范围。
> - 确定内核可以管理的物理页号范围。
> - 建立并初始化 `Page` 描述符数组。
> - 确定不与已有占用区域重叠的初始空闲物理页区域。
> - 通过 `init_memmap()` 将合法的空闲页区域交给现有物理内存管理器。
>
> ## 保持不变的内容
>
> - `struct Page` 和 `struct pmm_manager` 的定义。
> - `PADDR()`、`pa2page()`、`init_memmap()` 等现有接口的语义。
> - `get_memory_base()`、`get_memory_size()` 的现有职责。
> - 当前工程的内核链接布局和物理内存映射约定。
> - 其他模块和测试代码中已经存在的接口及断言。
>
> 允许在 `page_init()` 内进行必要的局部变量计算，但不要新增不必要的跨文件接口或全局状态。
>
> ## [SPECIFICATION]
>
> ·`page_init()`函数：
>
> pre-condition:
>
> - 内核已经完成进入 C 语言阶段所需的启动初始化。
> - DTB 解析已经按照现有启动流程执行，或者函数能够识别并处理内存信息不可用的情况。
> - `pmm_manager` 已经按照现有初始化流程完成设置。
> - 链接符号 `end` 及相关地址转换接口可以按照当前工程的约定使用。
>
> ## 有效物理内存信息的处理
>
> **case 1：内存信息有效。**
>
> 当物理内存起始地址和大小有效时，应当根据它们确定实际可管理的物理内存范围，并输出必要的内存探测信息。
>
> 计算时需要考虑：
>
> - 物理内存结束地址采用不包含末尾地址的区间表示。
> - 物理内存范围必须符合当前内核支持的地址上界。
> - 内存起始地址、结束地址与 `nbase`、`npage` 的关系必须符合当前工程的定义。
> - 计算过程中不能发生整数溢出或无效的地址换算。
>
> **case 2：内存信息无效或无法获取。**
>
> 如果内存起始地址或大小无效、结束地址计算溢出，或者内存范围超出内核支持的范围，应当按照当前工程已有的错误处理约定处理，例如使用适当的 `panic()` 终止初始化。
>
> 不能在没有可靠内存范围的情况下继续建立错误的页面描述符数组或空闲页区域。
>
> ### ·`npage` 与 `pages` 的初始化
>
> **pre -condition：** 已经确定合法的物理内存范围。
>
> **post-condition：**
>
> - `npage` 正确表示可管理物理页号的上界。
> - `npage - nbase` 与需要建立的 Page 描述符数量一致。
> - `pages` 位于内核镜像结束位置之后，并满足工程要求的对齐条件。
> - `pages` 数组具有足够的空间容纳所有需要管理的物理页描述符。
> - 数组自身占用的物理内存不会被当作空闲物理页交给分配器。
>
> 请根据现有宏的真实语义确定页号计算和对齐方式，不要直接将未经对齐的内存字节数当作页面数量。
>
> ## ·页面描述符的初始化
>
> 在建立初始空闲页区域之前，必须确保页面描述符处于符合现有内存管理器约定的初始状态。
>
> **要求：**
>
> - 不得将 OpenSBI 占用的内存交给分配器。
> - 不得将内核镜像占用的内存交给分配器。
> - 不得将 Page 描述符数组自身占用的内存交给分配器。
> - 不得将不在当前可管理范围内的页面交给分配器。
> - 描述符的状态标志、引用计数和其他字段必须符合 `default_init_memmap()` 的实际要求。
>
> 请根据 `struct Page` 的定义及现有初始化代码选择正确的字段初始化方式，不要自行改变字段语义。
>
> ### ·初始空闲物理页区域的确定
>
> **pre -condition：** 已经确定可管理的物理内存范围，并完成 Page 描述符数组的布局。
>
> **post-condition：**
>
> - 初始空闲区域位于内核镜像及 Page 描述符数组所占用区域之后。
> - 起始地址和结束地址满足完整物理页的对齐要求。
> - 空闲区域不与任何保留区域重叠。
> - 空闲页数量与实际可分配的完整物理页数量一致。
>
> **case 1：存在可用的完整物理页。**
>
> 将有效的连续空闲区域转换为对应的 Page 描述符起始位置，并通过现有 `init_memmap()` 接口完成初始化。
>
> **case 2：没有可用的完整物理页。**
>
> 如果内核镜像和描述符数组占用空间后已没有可分配的完整物理页，不得将无效的地址范围或零页数量传入不接受该参数的接口。应当按照当前工程的约定明确处理这种异常情况。
>
> **case 3：地址边界不满足页对齐要求。**
>
> 计算时必须保证不会将不完整的起始页或末尾页纳入空闲区域，也不能通过错误的整数运算使区域边界越界。
>
> ### ·最终必须满足的要求
>
> 1. 地址映射关系与 `PADDR()`、`pa2page()` 等接口保持一致。
> 2. `nbase`、`npage` 和 `pages` 的含义及相互关系正确。
> 3. 描述符数组不会覆盖内核已有内容。
> 4. 所有保留页面都不会被加入初始空闲区域。
> 5. `init_memmap()` 接收到的地址和页数符合其接口约定。
> 6. 不引入与本任务无关的算法、接口或文件修改。
> 7. 保留必要的内存布局说明和关键计算注释。
>
> 请根据上述功能规格自主设计实现，不必拘泥于某一种固定的代码组织方式，但所有行为都必须符合当前工程的真实语义。完成后，请直接修改 `kern/mm/pmm.c` 中的 `page_init()`，并简要汇报修改范围、关键设计选择以及发现的待确认问题。不要执行编译、自动测试、QEMU 启动或 Git 提交，等待我进行下一阶段的人工审查。

<details><summary>6 previous messages</summary>

> 我会先复核当前的 `pmm.c`、DTB 接口、页布局常量和链接脚本，确保 `page_init()` 与现有 `PADDR`/`pa2page` 语义一致。实现只落在 `kern/mm/pmm.c`，随后做静态检查，不编译、不启动 QEMU，也不提交 Git。
>
> <details><summary>Explored 11 files, 2 searches, ran a command</summary>
>
> <details><summary>Explored 8 files</summary>
>
> - Read `./code/kern/mm/pmm.c`
> - Read `./code/kern/mm/pmm.h`
> - Read `./code/kern/mm/memlayout.h`
> - Read `./code/kern/mm/mmu.h`
> - Read `./code/libs/riscv.h`
> - Read `./code/kern/driver/dtb.c`
> - Read `./code/kern/driver/dtb.h`
> - Read `./code/kern/init/init.c`
>
> </details>
>
> - Ran `sed -n '1,100p' code/tools/kernel.ld; cat code/tools/test_lab2.c; git status --short`
>
> <details><summary>Explored 4 files, 2 searches</summary>
>
> - Read `./code/kern/mm/default_pmm.c`
> - Read `./code/kern/mm/default_pmm.c`
> - Searched for `PHYSICAL_MEMORY_END|KERNTOP|KMEMSIZE|KERNEL_BEGIN_PADDR|PHYSICAL_MEMORY_OFFSET|DRAM_BASE|nbase|npage` in `kern`
> - Searched for `#define panic|void __noreturn __panic|__panic` in `debug`
> - Read `./code/kern/debug/assert.h`
> - Read `./code/kern/debug/panic.c`
>
> </details>
>
> </details>
>
> 实现方案已确定：以 `PHYSICAL_MEMORY_END` 作为当前工程支持的物理内存上界；按 DTB 的 `[base, base + size)` 计算 `npage`；把所有 `[nbase, npage)` 描述符先统一标为保留，再把内核结束页之后对齐放置的 `pages` 数组也纳入保留范围，最后仅将数组末尾到内存末端之间的完整页面交给 `init_memmap()`。这样 OpenSBI、内核、描述符数组以及任何非完整边界页都不会进入空闲链表。
>
> <details><summary>Edited a file</summary>
>
> - Wrote `./code/kern/mm/pmm.c` (+99 -0)
>
> </details>
>
> `page_init()` 已写入，修改范围仅在 `code/kern/mm/pmm.c`。我现在做最后的人工静态核对，重点检查 C99 声明位置、整数溢出判断、`npage` 上界、描述符保留状态和空闲区间对齐；按要求不会调用编译器或运行评测。
>
> <details><summary>Explored a file, ran 2 commands</summary>
>
> - Ran `git diff --check -- code/kern/mm/pmm.c; git status --short`
>
> <details><summary>Explored a file</summary>
>
> - Read `./code/kern/mm/pmm.c`
>
> </details>
>
> - Ran `git diff --stat -- code/kern/mm/pmm.c; git diff -- code/kern/mm/pmm.c`
>
> </details>
</details>

已完成 [pmm.c](./code/kern/mm/pmm.c:70) 中 `page_init()` 的实现，修改范围仅为该文件，未修改其他源码或测试代码。

关键实现：

- 设置 `va_pa_offset = PHYSICAL_MEMORY_OFFSET`，确保后续 `PADDR()` 等转换正确。
- 从 `get_memory_base()` 和 `get_memory_size()` 获取 DTB 内存范围，检查起始地址、零大小、加法溢出以及 `PHYSICAL_MEMORY_END` 上界。
- 按 `npage = PPN(mem_end)` 计算可管理页号上界，描述符数量为 `npage - nbase`。
- 将 `pages` 数组放在物理页对齐的内核 `end` 之后，换算为虚拟地址，并检查数组大小和地址溢出。
- 先把全部描述符初始化为保留状态，清空引用计数、状态和属性，同时初始化链表节点。
- 将数组末尾向上对齐、DTB 内存末尾向下对齐，仅把中间完整的物理页交给 `init_memmap()`。
- 对非法内存范围、数组越界和没有可用完整物理页等情况调用 `panic()`。

额外执行了 `git diff --check`，未发现空白字符错误。按要求没有编译、运行测试或启动 QEMU。

有两个后续待确认点：

1. 当前 `init_pmm_manager()` 仍选择 First-Fit，因此第一部分完成 `page_init()` 后，第二项的 Best-Fit 检查仍会失败，这是下一阶段的工作。
2. 当前 DTB 解析器只返回内存范围，没有暴露 DTB 自身位置、大小或保留内存表。本实现按本阶段要求保留了 OpenSBI、内核和描述符数组，但未单独保留 DTB 所在物理页。如果实验要求严格保护 FDT 本身，需要再讨论如何在不扩大接口范围的前提下处理。

> ## [PROMPT]
>
> **任务：** 对 `code/kern/mm/pmm.c` 中刚刚实现的 `page_init()` 进行独立代码审查，判断实现是否满足实验二的功能要求、当前项目的接口约定和现有测试代码的检查条件。
>
> 本次任务只进行代码审查，不进行代码修改。
>
> 请直接检查当前工作区中的真实源码和 Git 差异，不要仅根据先前对话中的代码片段作出结论。
>
> **操作要求：**
>
> 1. 阅读 `kern/mm/pmm.c` 中的 `page_init()` 及其调用上下文。
> 2. 根据需要检查 `kern/mm/pmm.h`、`kern/mm/memlayout.h`、`kern/mm/mmu.h`、`libs/defs.h`、`kern/driver/dtb.c`、`kern/driver/dtb.h`、`kern/mm/default_pmm.c` 和 `tools/test_lab2.c`。
> 3. 通过实际定义核实相关宏、地址转换函数、页面状态标志、内存管理器接口和测试条件，不得根据函数名称自行假设其语义。
> 4. 检查当前 Git diff，确认本次修改的文件范围，并识别是否存在与任务无关的改动。
> 5. 不得修改任何源码、测试代码、配置文件或 Git 状态。
> 6. 本阶段不执行编译、测试、QEMU 启动或 Git 提交。
>
> ## [RELY]
>
> 审查需要依据以下现有接口与约定：
>
> - `get_memory_base()`、`get_memory_size()`：获取 DTB 解析出的物理内存范围。
> - `PHYSICAL_MEMORY_OFFSET`、`PHYSICAL_MEMORY_END`、`DRAM_BASE`、`PGSIZE`：地址映射、物理内存上界和页面大小。
> - `PADDR()`、`pa2page()`、`page2pa()`：虚拟地址、物理地址与 Page 描述符之间的转换。
> - `nbase`、`npage`、`pages`：物理页号范围与 Page 描述符数组。
> - `SetPageReserved()`、`PageReserved()`、`list_init()`：页面初始状态和链表节点初始化。
> - `init_memmap()` 及其底层 `default_init_memmap()`：空闲页面的注册要求。
> - `tools/test_lab2.c` 中现有的 `test_page_init()`：页面初始化的检查条件。
>
> 请根据实际源码确认上述接口的具体语义，并检查相关调用顺序。
>
> ## [GUARANTEE]
>
> 本次审查必须做到：
>
> 1. 不修改任何文件，不自动修复发现的问题。
> 2. 对每个问题指出具体文件、函数或代码位置，并说明对应的源码依据。
> 3. 区分确定存在的问题、潜在风险和仅需确认的设计选择。
> 4. 对没有发现问题的部分，也应说明核实依据，而不是只给出笼统的“代码正确”。
> 5. 如果缺少必要的源码或证据，应明确指出，不得假定已经验证。
> 6. 不得声称代码已经编译通过、测试通过或能够正常启动，因为本阶段不执行这些操作。
>
> ## [SPECIFICATION]
>
> ### 1. 地址与内存范围
>
> 检查：
>
> - `va_pa_offset` 是否在使用 `PADDR()` 前正确设置。
> - `get_memory_base()` 和 `get_memory_size()` 的返回值是否得到充分验证。
> - 内存结束地址计算是否可能溢出。
> - `npage = PPN(mem_end)` 是否符合当前项目的物理页号定义和测试要求。
> - `npage - nbase` 是否与描述符数组所需的数量一致。
> - `end` 的虚拟地址转物理地址过程是否正确。
> - `pages` 的起始地址、对齐方式、数组大小和结束地址是否正确。
>
> ### 2. 页面状态与空闲区域
>
> 检查：
>
> - 每个 Page 描述符的初始字段和链表节点是否正确初始化。
> - 保留状态是否符合 `default_init_memmap()` 的断言要求。
> - OpenSBI、内核镜像和 Page 描述符数组占用的页面是否被排除在初始空闲区之外。
> - 空闲区起止地址是否正确对齐。
> - 空闲页面数量是否与实际完整页面范围一致。
> - `pa2page()` 得到的起始描述符是否正确对应 `free_start_pa`。
> - `init_memmap()` 是否只在参数有效且页数大于零时调用。
> - 空闲区域是否可能与任何已占用区域重叠。
>
> ### 3. 异常情况与整数运算
>
> 检查：
>
> - DTB 内存信息无效或缺失时的处理。
> - 物理内存超出支持范围时的处理。
> - 描述符数组超出物理内存范围时的处理。
> - 页面对齐运算和地址加法是否存在未覆盖的溢出风险。
> - 没有可用完整物理页时的处理是否符合现有接口约定。
>
> ### 4. 测试条件与最终报告
>
> 逐项核对 `test_page_init()` 中的检查条件，说明当前实现从静态代码层面是否满足这些条件，以及哪些条件仍需通过实际运行才能确认。
>
> 最后按以下格式输出：
>
> **A. 总体结论：** 暂无明显问题 / 存在需要修改的问题 / 证据不足，无法判断。
>
> **B. 确定存在的问题：** 按严重程度列出；如果没有，明确说明。
>
> **C. 潜在风险与待确认事项：** 说明触发条件、可能影响和判断依据。
>
> **D. 与测试条件的对应关系：** 逐项列出测试检查点及静态审查结果，不得将静态审查结果描述为测试通过。
>
> **E. 下一步建议：** 明确指出是否可以进入编译阶段，以及编译前必须修复的阻塞问题。
>
> 完成审查后停止，等待人工决定下一步操作。

<details><summary>4 previous messages</summary>

> 我会基于当前工作区和实际 Git 差异重新审查，不复用先前结论。重点核实 `npage`、描述符布局、保留页状态，以及 `test_page_init()` 的每一个静态可判定条件；不会修改文件或执行构建、测试、QEMU。
>
> <details><summary>Explored 9 files, ran a command</summary>
>
> - Ran `git status --short; git diff --stat; git diff -- code/kern/mm/pmm.c`
>
> <details><summary>Explored 9 files</summary>
>
> - Read `./code/kern/mm/pmm.c`
> - Read `./code/kern/mm/pmm.h`
> - Read `./code/kern/mm/memlayout.h`
> - Read `./code/kern/mm/mmu.h`
> - Read `./code/libs/defs.h`
> - Read `./code/tools/test_lab2.c`
> - Read `./code/kern/mm/default_pmm.c`
> - Read `./code/kern/driver/dtb.c`
> - Read `./code/kern/driver/dtb.h`
>
> </details>
>
> </details>
>
> 目前 `page_init()` 的主链计算与 `test_page_init()` 的检查逻辑能够静态对应上。我还在核对一个关键前置条件：`pmm_init()` 会先调用当前 manager 的 `check()`，再执行注入的页面测试，因此需要确认现有 `default_pmm.c` 的检查流程不会在到达 `test_page_init()` 前因既有代码问题中断。
>
> <details><summary>Explored 8 files, 2 searches</summary>
>
> - Read `./code/kern/mm/default_pmm.c`
> - Read `./code/kern/init/init.c`
> - Read `./code/tools/inject.py`
> - Read `./code/kern/init/entry.S`
> - Read `./code/libs/list.h`
> - Read `./code/tools/kernel.ld`
> - Read `./code/kern/mm/best_fit_pmm.c`
> - Read `./code/kern/mm/best_fit_pmm.h`
> - Searched for `free_list|nr_free` in `best_fit_pmm.c`
> - Searched for `best_fit_pmm.c|KSRCDIR|kern/mm` in `Makefile`
>
> </details>
</details>

**A. 总体结论：**

`page_init()` 本身暂未发现确定的实现错误，核心地址计算、描述符布局和空闲区划分与当前接口及测试条件静态一致。

但当前工作区整体存在确定的构建阻塞，来源是尚未完成的 Best-Fit 骨架；因此不能据此认为整个项目已经可以编译或通过 `make grade`。

本次审查确认 Git 工作区只有 [pmm.c](./code/kern/mm/pmm.c:70) 被修改，新增 99 行，没有发现与任务一无关的 Git 改动。

**B. 确定存在的问题：**

1. **严重：Best-Fit 源码当前无法通过编译。**

   [best_fit_pmm.c](./code/kern/mm/best_fit_pmm.c:24) 从第 24 行开始使用 `free_list` 和 `nr_free`，但该文件没有声明这两个对象：

   ```c
   list_entry_t free_list_store = free_list;
   unsigned int nr_free_store = nr_free;
   ```

   文件中也没有对应的 `free_area_t` 或宏定义。这两个标识符不是 [pmm.h](./code/kern/mm/pmm.h:15) 提供的全局对象。

   [Makefile](./code/Makefile:104) 会把整个 `kern/mm` 目录加入 `KSRCDIR`，因此 `best_fit_pmm.c` 会被无条件编译。这是确定存在的编译阻塞，不属于本次 `page_init()` 修改引入的问题。

2. **严重：评分注入后会缺少 `best_fit_pmm_manager` 定义。**

   [best_fit_pmm.h](./code/kern/mm/best_fit_pmm.h:6) 只声明了：

   ```c
   extern const struct pmm_manager best_fit_pmm_manager;
   ```

   但当前仓库中没有该对象的定义。[test_lab2.c](./code/tools/test_lab2.c:106) 会实际引用该符号，因此即使前面的编译问题解决，注入测试后仍可能在链接阶段因符号未定义而失败。

   这一项同样属于后续 Best-Fit 任务，而不是 `page_init()` 的逻辑问题。

**C. 潜在风险与待确认事项：**

1. **DTB/FDT 自身及设备树保留内存没有被排除。**

   [page_init()](./code/kern/mm/pmm.c:136) 将描述符数组结束到 `mem_end` 之间的所有完整页面交给分配器。它排除了 OpenSBI、内核镜像和描述符数组，但没有处理：

   - `boot_dtb` 指向的 FDT 数据；
   - FDT 中的 `memory reserve map`；
   - 设备树可能报告的保留内存区域。

   [dtb.c](./code/kern/driver/dtb.c:107) 目前只解析第一个 `memory` 节点的 `reg`，没有暴露 DTB 大小或 `off_mem_rsvmap`。如果 FDT 恰好位于 `[free_start_pa, free_end_pa)` 内，后续就可能被分配并覆盖。

   现有 `test_page_init()` 不检查这一点，因此静态上不能判定测试会因此失败，但它与“所有不能被分配的页面不得进入空闲区”的广义要求存在风险。需要确认实验是否只要求保留 OpenSBI、内核和描述符数组。

2. **DTB 已有多内存区的情况无法表达。**

   当前 `get_memory_base()` 和 `get_memory_size()` 只返回一个基址和一个大小，`extract_memory_info()` 也只接受第一个 `memory` 节点。存在多个 RAM 区或内存空洞时，`page_init()` 会把这些空洞当成可用内存。当前 QEMU 默认布局通常只有一个连续内存区，但接口本身不支持更复杂情况。

3. **`dtb_init()` 对异常 DTB 的边界检查有限。**

   `page_init()` 能处理 `get_memory_base()` 或 `get_memory_size()` 返回 0 的情况，但 `dtb_init()` 会直接根据 `boot_dtb` 访问头部，尚未检查该地址是否可读、头部偏移是否越界或 DTB 总大小是否合法。严重损坏的 DTB 可能在进入 `page_init()` 前就触发访存异常。

4. **内存上界固定为 128 MiB。**

   [memlayout.h](./code/kern/mm/memlayout.h:12) 定义 `PHYSICAL_MEMORY_END = 0x88000000`。`page_init()` 会拒绝超过该上界的 DTB 范围。这与工程当前 QEMU 配置一致，但如果更换机器配置，就必须同步调整这个常量。

**D. 与测试条件的对应关系：**

| 测试检查点 | 静态审查结果 | 依据 |
|---|---|---|
| 1. `va_pa_offset` | 静态满足 | [pmm.c](./code/kern/mm/pmm.c:74) 在首次调用 `PADDR()` 前设置为 `PHYSICAL_MEMORY_OFFSET`。 |
| 2. DTB 内存信息非零 | 条件处理满足，实际值需运行确认 | `page_init()` 对基址小于 `DRAM_BASE`、大小为零和加法溢出均会 panic；DTB 成功解析后才能得到有效值。 |
| 3. `npage == PPN(mem_end)` 且大于 `nbase` | 静态满足 | [pmm.c](./code/kern/mm/pmm.c:92) 使用与测试相同的 `PPN(mem_end)`，并显式拒绝 `npage <= nbase`。 |
| 4. `pages` 非空、对齐、位于 `end` 之后 | 静态满足 | 物理起点通过 `ROUNDUP(kernel_end_pa, PGSIZE)` 获得，再使用 `va_pa_offset` 转为虚拟地址。 |
| 5. 空闲页数量非零且不超过描述符总数 | 正常内存配置下静态满足，数量需运行确认 | 至少释放一个完整页，否则 panic；空闲终止页号不会超过 `npage`，因此交给 `init_memmap()` 的页数不超过 `npage - nbase`。 |
| 6. 内核尾页保持 reserved | 静态满足 | 所有描述符先被标记为 reserved，只有描述符数组之后的页面会被 `default_init_memmap()` 清除保留标志。 |
| 7. Page 描述符数组所在页保持 reserved | 静态满足 | 数组紧随内核结束位置，且空闲区起始位置从数组末尾向上对齐，数组覆盖页不会进入空闲区。 |
| 8. `page2pa()` / `pa2page()` 往返 | 静态满足 | `pa2page()` 返回 `&pages[PPN(pa)-nbase]`，`page2pa()` 再通过 `page - pages + nbase` 恢复原页号。 |

[default_init_memmap()](./code/kern/mm/default_pmm.c:18) 要求每个待释放页面在进入时都是 `PageReserved`，当前实现已先统一设置该状态；它随后清除空闲页的 reserved、property 和引用计数，并将整段连续区域加入空闲链表，这符合当前实现语义。

另外，注入测试位于 `pmm_init()` 末尾，而此前会执行 `check_alloc_page()`。因此测试能够实际运行还依赖当前 First-Fit `default_check()` 在运行时正常完成。这一部分静态逻辑看起来符合预期，但未经运行不能确认。

**E. 下一步建议：**

可以围绕 `page_init()` 进入下一步的编译审查，本函数本身未发现必须先修复的静态逻辑问题。

但不能直接认为整个工程能够进入完整构建和评分阶段。完整 `make` 或 `make grade` 前，仍存在 `best_fit_pmm.c` 中 `free_list`/`nr_free` 未声明以及 `best_fit_pmm_manager` 未定义的确定阻塞。这两项属于后续 Best-Fit 任务；如果当前阶段只审查任务一，则应向审查者明确：`page_init()` 可继续编译，但整个项目尚不能构建，更未经验证通过测试。

> ## [PROMPT]
>
> **任务：** 对 `code/kern/mm/pmm.c` 进行独立编译验证，确认刚刚实现的 `page_init()` 能否通过项目现有的编译配置。
>
> **操作要求：**
>
> 先检查项目根目录的 Makefile 和相关编译规则，确认 `pmm.c` 对应的实际目标文件及其编译命令。优先使用项目原有的构建规则，仅编译 `pmm.c` 对应的目标文件，无需执行完整 `make`、`make grade`、QEMU 启动或运行测试。如果单独编译目标无法避免触发整个工程的构建，或者存在其他依赖阻塞，立即停止并说明原因。
>
> ## [RELY]
>
> - 当前工作区的 `code/kern/mm/pmm.c`。
> - 项目现有 Makefile、编译规则、交叉编译器配置和头文件依赖。
> - 上一步独立代码审查的结果
>
> ## [GUARANTEE]
>
> 实际执行可行的独立编译命令，并提供真实的终端输出。区分编译成功、编译失败和因依赖问题无法单独编译三种情况。如果失败，提供完整的首个有效错误信息、文件位置和错误原因分析。
>
> ## [SPECIFICATION]
>
> ### 最终输出
>
> 请按以下格式报告：
>
> **A. 编译目标：** 实际目标文件及其路径。
>
> **B. 执行命令：** 使用的真实编译命令。
>
> **C. 编译结果：** 成功、失败或无法单独编译。
>
> **D. 错误分析：** 如果失败，列出错误位置、原因和是否与 `page_init()` 有关。
>
> **E. 下一步建议：** 明确说明是否可以继续完成 Best-Fit 实现，以及何时适合进行完整工程测试。
>
> 完成后停止

<details><summary>4 previous messages</summary>

> 我会先用 Makefile 的 `-n` 规则确认 `pmm.c` 的最小目标确实只触发它自身的依赖生成和编译，并检查交叉编译器是否可用；确认不会连带构建整个工程后，再执行该单一目标。
>
> <details><summary>Ran 4 commands</summary>
>
> - Ran `git status --short; sed -n '1,145p' code/Makefile; sed -n '1,115p' code/tools/function.mk`
> - Ran `make -n obj/kern/mm/pmm.o`
> - Ran `command -v riscv64-unknown-elf-gcc; riscv64-unknown-elf-gcc --version | sed -n '1,3p'`
> - Ran `find obj -maxdepth 4 -type f -name 'pmm.*' -printf '%p %s bytes\n' 2>/dev/null; find bin -maxdepth 1 -type f -printf '%p %s bytes\n' 2>/dev/null`
>
> </details>
>
> Makefile 的干运行结果只包含 `kern/mm/pmm.c` 的依赖与对象编译规则，没有触发链接、镜像生成或其他源文件编译；交叉编译器也已确认可用。现在执行这个单一目标并保留真实输出。
>
> <details><summary>Ran 3 commands</summary>
>
> - Ran `make obj/kern/mm/pmm.o`
> - Ran `stat -c '%n %s bytes %y' obj/kern/mm/pmm.o; file obj/kern/mm/pmm.o`
> - Ran `find obj -type f -printf '%p %s bytes\n' | sort; git status --short`
>
> </details>
</details>

**A. 编译目标：**

实际目标文件：

```text
/home/win/oslab/OS-Lab/code/obj/kern/mm/pmm.o
```

根据 Makefile 的 `cc_template` 规则，`kern/mm/pmm.c` 被编译为 `obj/kern/mm/pmm.o`，不属于整个 kernel 或 `ucore.img` 目标。

**B. 执行命令：**

实际执行的 Makefile 命令：

```bash
make obj/kern/mm/pmm.o
```

首先通过 `make -n obj/kern/mm/pmm.o` 确认该目标只触发 `pmm.c` 自身的编译，不会编译其他源文件或执行链接。展开后的实际编译命令为：

```bash
riscv64-unknown-elf-gcc -Ikern/mm/ -mcmodel=medany -std=gnu99 \
  -Wno-unused -Werror -fno-builtin -Wall -O2 -nostdinc \
  -fno-stack-protector -ffunction-sections -fdata-sections -g \
  -Ilibs/ -Ikern/debug/ -Ikern/driver/ -Ikern/mm/ -Ikern/arch/ \
  -c kern/mm/pmm.c -o obj/kern/mm/pmm.o
```

**C. 编译结果：**

编译成功。真实终端输出只有：

```text
+ cc kern/mm/pmm.c
```

交叉编译器版本为 `riscv64-unknown-elf-gcc 13.2.0`。生成的产物为：

```text
obj/kern/mm/pmm.o 58080 bytes
ELF 64-bit LSB relocatable, UCB RISC-V, RVC, double-float ABI,
version 1 (SYSV), with debug_info, not stripped
```

编译使用了 Makefile 原有的 `-Wall -Werror` 配置，过程中没有警告或错误输出。`page_init()` 通过了 `pmm.c` 的独立编译验证。

本次编译只生成了目标文件和依赖文件；`obj/` 当前显示为未跟踪的构建目录，源码的 Git 修改范围仍只有 `kern/mm/pmm.c`。

**D. 错误分析：**

不适用，本次编译成功，没有错误信息。

需要明确，这只验证了 `pmm.c` 的编译阶段。由于没有编译其他对象和链接内核，不能据此判断整个工程能否链接或运行。

**E. 下一步建议：**

可以继续完成 Best-Fit 实现。完成 Best-Fit 后，仍需处理并验证：

- `best_fit_pmm.c` 中当前缺失的 `free_list`、`nr_free` 状态；
- `best_fit_pmm_manager` 的定义与注册；
- 完整工程的编译和链接；
- 最后再执行 `make grade` 和 QEMU 测试。

在 Best-Fit 完整实现前，不适合进行完整工程构建或评分测试。

> ## [PROMPT]
>
> **任务目标：**
>
> 在 `kern/mm/best_fit_pmm.c` 中完成 Best-Fit 连续物理内存分配器，并在 `kern/mm/pmm.c` 中完成管理器切换，使内核初始化和页面分配检查实际使用 `best_fit_pmm_manager`，而不是现有的`default_pmm_manager`。
>
> Best-Fit是从当前所有能够满足分配页数要求的空闲连续内存块中，选择容量最小的一个；如果多个块的容量相同，则选择空闲链表中地址靠前的块。
>
> 请结合项目现有代码、First-Fit 参考实现及已有测试函数，理解模块的完整职责后完成实现。
>
> **操作要求：**
>
> 1. 必须直接修改项目中的实际源码文件。
> 2. 仅允许修改 `kern/mm/best_fit_pmm.c` 和 `kern/mm/pmm.c`。
> 3. 不得修改 `basic_check()`、`best_fit_check()` 的函数内容、断言或测试逻辑，也不得删除、绕过或弱化已有测试。
> 4. 不得修改 `struct pmm_manager`、`struct Page`、`free_area_t` 等既有数据结构，不得随意修改已有宏定义、头文件接口或 First-Fit 实现。
> 5. `pmm.c` 中的 `page_init()`已经完成 ，本次只需修改 `init_pmm_manager()` 中的管理器选择。
> 6. 保留原有注释及文件结构。可以增加模块内部的辅助函数，但不得增加不必要的跨文件接口。
>
> **输出要求：**
>
> - 实际完成源码修改。必须符合 `pmm.h` 中的 `struct pmm_manager` 接口约定。
>
> * `best_fit_pmm_manager` 必须具有正确的名称，且其各成员指向对应的 Best-Fit 实现。
> * `init_pmm_manager()` 必须选择 `&best_fit_pmm_manager`。
> * 现有 `basic_check()` 和 `best_fit_check()` 必须能够继续检验分配、释放、空闲页计数和 Best-Fit 选择语义。
>
> ## [RELY]
>
> 请优先使用实现本模块所必需的以下现有代码和接口：
>
> 1. `kern/mm/pmm.h`
>    - `struct pmm_manager`
>    - `struct Page`
>    - `alloc_pages()`、`free_pages()`、`nr_free_pages()`
>    - `page2pa()`、`pa2page()` 等页面地址转换接口。
> 2. `kern/mm/memlayout.h`
>    - `struct Page`
>    - `free_area_t`
>    - `PageReserved()`、`PageProperty()`、`SetPageProperty()`、`ClearPageProperty()` 等页面状态宏。
> 3. `kern/libs/list.h`
>    - `list_entry_t`
>    - `list_init()`、`list_add()`、`list_add_before()`、`list_del()`
>    - `list_empty()`、`list_next()`、`list_prev()`。
> 4. `kern/mm/default_pmm.c`
>    - 作为初始化、空闲块登记、链表维护、分配、释放及相邻块合并的参考实现可以借鉴其数据结构维护方式，但不能直接复制 First-Fit 的分配选择策略。
>
> &#x20;  5.`kern/mm/best_fit_pmm.c`
>
> - 保留已有的 `basic_check()` 和 `best_fit_check()`。
> - 这两个函数使用 `free_list`、`nr_free` 检查管理器内部状态，因此需要提供正确的模块内部状态及对应宏定义。
>
> &#x20;  6.`kern/mm/pmm.c`
>
> - `init_pmm_manager()` 负责选择管理器并初始化其内部状态。
> - `pmm_init()` 后续通过 `page_init()` 登记可用内存，再调用 `check_alloc_page()` 执行管理器检查。
>
> 以当前工作区的真实接口为准，并在输出中说明差异，不要自行修改公共接口来绕过问题。
>
> ## [GUARANTEE]
>
> 必须提供以下实现：
> ```c
> static void best_init(void);
>
> static void best_init_memmap(struct Page *base, size_t n);
>
> static struct Page *best_alloc_pages(size_t n);
>
> static void best_free_pages(struct Page *base, size_t n);
>
> static size_t best_nr_free_pages(void);
>
> const struct pmm_manager best_fit_pmm_manager;
> ```
>
> 可以增加仅供本模块内部使用的辅助函数和静态状态。
>
> `best_fit_pmm_manager` 应正确初始化以下成员：
>
> - `name`
> - `init`
> - `init_memmap`
> - `alloc_pages`
> - `free_pages`
> - `nr_free_pages`
> - `check`
>
> 其中 `check` 必须指向已有的 `best_fit_check()`，不得改用 First-Fit 的检查函数。
>
> ## [SPECIFICATION]
>
> ### 1. `best_init()`
>
> **前置条件：**
>
> - 管理器内部的空闲链表和计数尚未初始化。
>
> **后置条件：**
>
> - 空闲链表处于合法的空状态。
> - 空闲页计数初始化为零。
> - 后续内存区登记可以安全地使用这些状态。
>
> ### 2. `best_init_memmap(struct Page *base, size_t n)`
>
> **前置条件：**
>
> - `n > 0`。
> - `[base, base + n)` 中的页面均处于保留状态，且尚未登记为空闲内存。
>
> **后置条件：**
>
> - 合法登记这段连续物理内存。
> - 空闲块的起始页正确记录块大小，并设置 `PageProperty`。
> - 其余页面不被误标记为空闲块头。
> - 空闲链表按照物理页面地址顺序维护。
> - `nr_free` 正确增加 `n`。
>
> 不得将尚未登记或仍被保留的页面错误地加入可分配区域。
>
> ### 3. `best_alloc_pages(size_t n)`
>
> **前置条件：**
>
> - `n > 0`。
> - 空闲链表、各块的 `property` 和 `nr_free` 已被正确维护。
>
> **情况一：分配成功。**
>
> 当存在一个或多个容量不小于 `n` 的空闲块时：
>
> - 选择其中 `property` 最小的块。
> - 容量相同时，选择链表中地址靠前的块。
> - 返回所选块的起始页描述符。
> - 将被分配的页面从空闲链表中移除。
> - 如果原块大于 `n`，剩余页面必须形成一个合法的空闲块，并正确维护其大小、标志和链表位置。
> - `nr_free` 必须减少 `n`。
>
> **情况二：分配失败。**
>
> 当 `n > nr_free`，或者不存在任何容量不小于 `n` 的连续空闲块时：
>
> - 返回 `NULL`。
> - 不得改变空闲链表、空闲块属性或 `nr_free`。
>
> **情况三：非法请求。**
>
> 当 `n == 0` 时，沿用项目现有的断言约定进行处理。
>
> 特别注意：仅检查 `nr_free >= n` 并不足以保证分配成功，因为空闲页面总数足够时，也可能不存在足够大的连续空闲块。
>
> ### 4. `best_free_pages(struct Page *base, size_t n)`
>
> **前置条件：**
>
> - `n > 0`。
> - 待释放的页面属于合法的已分配区域，不是保留页面，也没有重复释放。
>
> **后置条件：**
>
> - 待释放页面的引用计数、标志和块属性得到正确维护。
> - 新释放的连续页面形成合法空闲块，并按照物理地址顺序插入空闲链表。
> - `nr_free` 正确增加 `n`。
> - 如果新块与前一个空闲块物理地址连续，应合并。
> - 如果新块与后一个空闲块物理地址连续，也应合并。
> - 合并后只有真正的空闲块头保留有效的块大小及 `PageProperty` 标记。
>
> 相邻块合并不得重复增加空闲页计数，也不得破坏链表结构。
>
> ### 5. `best_nr_free_pages()`
>
> 返回当前真实的空闲物理页总数，结果必须与空闲链表中各空闲块容量之和一致。
>
> 6\. `best_fit_pmm_manager` 与管理器接入
>
> - `.name` 必须为 `"best_fit_pmm_manager"`。
> - 每个接口成员均指向对应的 Best-Fit 实现。
> - `.check` 指向已有的 `best_fit_check()`。
> - `kern/mm/pmm.c` 中的 `init_pmm_manager()` 必须将 `pmm_manager` 设置为 `&best_fit_pmm_manager`，并通过现有输出语句显示当前管理器名称。
> - 不得修改 `page_init()` 的实现。
> - 不得修改已有测试函数来使检查通过。
>
> 完成后请输出：
>
> 1. 实际修改的文件及对应位置。
> 2. 各函数的实现摘要，并简要对比 Best-Fit 与 First-Fit 。
> 3. 管理器注册与切换的实际修改内容。
> 4. 尚未验证的事项和下一步建议。

<details><summary>14 previous messages</summary>

> 我会先复核 Best-Fit 文件、First-Fit 的完整状态维护和当前 `pmm.c` 差异，确认哪些代码属于不可修改的检查函数。实现会复用现有页标志和链表语义，只替换空闲块选择策略，并把 manager 切换为 Best-Fit。
>
> <details><summary>Explored 6 files, ran a command</summary>
>
> <details><summary>Explored 3 files</summary>
>
> - Read `./code/kern/mm/best_fit_pmm.c`
> - Read `./code/kern/mm/best_fit_pmm.h`
> - Read `./code/kern/mm/default_pmm.c`
>
> </details>
>
> - Ran `nl -ba code/kern/mm/pmm.c | sed -n '1,205p'; git status --short; git diff --stat`
>
> <details><summary>Explored 3 files</summary>
>
> - Read `./code/kern/mm/pmm.h`
> - Read `./code/kern/mm/memlayout.h`
> - Read `./code/tools/test_lab2.c`
>
> </details>
>
> </details>
>
> 实现策略已明确：登记与合并逻辑沿用 First-Fit 的有序空闲链表约定；分配阶段遍历整个链表，只在 `property` 严格更小时替换候选，因此同容量自然保留链表中地址更靠前的块。测试函数保持原样，仅在模块内部补充状态、实现和 manager 注册。
>
> <details><summary>Edited 2 files</summary>
>
> - Wrote `./code/kern/mm/best_fit_pmm.c` (+137 -0)
> - Wrote `./code/kern/mm/pmm.c` (+1 -1)
>
> </details>
>
> Best-Fit 代码和 manager 切换已写入。接下来先确认 Git 差异没有触碰两个检查函数或公共接口，再用项目规则分别编译两个修改过的对象；通过后再决定是否执行完整链接验证。
>
> <details><summary>Ran 4 commands</summary>
>
> - Ran `git diff --check; git status --short; git diff --stat`
> - Ran `git diff -U3 -- code/kern/mm/best_fit_pmm.c code/kern/mm/pmm.c`
> - Ran `git diff -U0 -- code/kern/mm/best_fit_pmm.c | rg -n "^[+-](static void|basic_check|best_fit_check|    assert|    struct Page|    list_entry_t|    unsigned int nr_free_store|    free_|    alloc_|    #ifdef|    #endif|    score|    cprintf|    le =|    while)"`
> - Ran `make obj/kern/mm/best_fit_pmm.o obj/kern/mm/pmm.o`
>
> </details>
>
> 两个修改文件的独立编译均已通过，且差异确认没有改动 `basic_check()` 或 `best_fit_check()` 的函数内容。下一步执行一次普通完整 `make`，验证 `best_fit_pmm_manager` 是否能与其余内核对象正确链接；这不会启动 QEMU。
>
> <details><summary>Ran a command</summary>
>
> - Ran `make`
>
> </details>
>
> 完整编译与链接已通过，`bin/kernel` 和 `bin/ucore.img` 均成功生成，说明 manager 符号与现有对象的链接关系正确。为验证实际选择语义和空闲链表的运行期行为，我将再运行项目自带的 `make grade`；它会在 `/tmp` 的副本中注入测试并通过 QEMU 执行。
>
> <details><summary>Ran a command</summary>
>
> - Ran `make grade`
>
> </details>
>
> 运行期测试已经达到 20/20，但静态复核发现一个测试未覆盖的边界：在模块的“按地址插入链表尾部”分支中，插入后若继续循环，会再次处理新节点，可能破坏 `page_link.prev`，进而影响后续前向合并。我要只在 Best-Fit 模块内补上插入后的退出，确保规范要求的地址有序和前向合并在所有路径上都成立，然后重新编译与评分。
>
> <details><summary>Edited a file, ran 2 commands</summary>
>
> - Wrote `./code/kern/mm/best_fit_pmm.c` (+2 -0)
> - Ran `make obj/kern/mm/best_fit_pmm.o obj/kern/mm/pmm.o`
> - Ran `make grade`
>
> </details>
>
> 修正后的模块再次通过独立对象编译和官方 `make grade`，两项检查均为 OK，总分 20/20。最后我会核对当前差异范围、测试函数确未变化，并整理实现位置与已验证/未验证边界。
>
> <details><summary>Explored 2 files, ran 2 commands</summary>
>
> - Ran `git diff --check; git status --short; git diff --stat`
>
> <details><summary>Explored 2 files</summary>
>
> - Read `./code/kern/mm/best_fit_pmm.c`
> - Read `./code/kern/mm/pmm.c`
>
> </details>
>
> - Ran `git diff -U0 -- code/kern/mm/best_fit_pmm.c | sed -n '1,220p'`
>
> </details>
</details>

**1. 实际修改的文件及对应位置**

只修改了允许范围内的两个文件：

- [best_fit_pmm.c](./code/kern/mm/best_fit_pmm.c:8)
  - 增加模块内部 `free_area`、`free_list` 和 `nr_free`。
  - 实现 Best-Fit 初始化、内存登记、分配、释放和空闲页统计。
  - 在文件末尾注册 `best_fit_pmm_manager`。
- [pmm.c](./code/kern/mm/pmm.c:36)
  - 将 `init_pmm_manager()` 的选择从 `default_pmm_manager` 改为 `best_fit_pmm_manager`。
  - `page_init()` 未做新的修改。

`basic_check()` 和 `best_fit_check()` 的函数内容、断言和测试逻辑均未修改。Git 差异只有两个新增区域：测试函数之前的管理器实现，以及测试函数之后的管理器注册。

**2. 各函数实现摘要及与 First-Fit 的区别**

- `best_init()`：初始化空链表并把 `nr_free` 清零。
- `best_init_memmap()`：检查待登记页面均为 reserved，清除其初始状态，把连续区域作为空闲块头登记，并按物理页面地址插入链表。插入末尾后立即退出，避免重复处理新节点和维护错误的 `prev` 指针。
- `best_alloc_pages()`：遍历全部空闲块，选择满足 `property >= n` 且容量最小的块。候选替换只在容量严格更小时发生；因此多个块容量相同时，保留链表中地址更靠前的块。成功分配后删除原块，必要时把剩余页面作为一个新的空闲块插回原位置，并减少 `nr_free`。
- `best_free_pages()`：检查页面不是保留页且不是重复释放，清除页面状态并恢复引用计数，按地址顺序插入新空闲块，然后分别尝试与前块和后块合并。空闲页计数只在释放时增加一次，合并不会重复增加。
- `best_nr_free_pages()`：返回当前 `nr_free`。

与 First-Fit 的主要区别在分配选择：First-Fit 遍历到第一个容量足够的空闲块就停止；Best-Fit 必须检查所有候选块并选择最小可用块。容量相同时，两者都优先选择链表靠前的块。Best-Fit 通常能减少大块被过早切分的情况，但分配搜索成本更高。

Best-Fit 的登记、拆分、释放和相邻块合并语义与 First-Fit 保持一致。

**3. 管理器注册与切换**

[best_fit_pmm_manager](./code/kern/mm/best_fit_pmm.c:269) 已完整注册：

```c
const struct pmm_manager best_fit_pmm_manager = {
    .name = "best_fit_pmm_manager",
    .init = best_init,
    .init_memmap = best_init_memmap,
    .alloc_pages = best_alloc_pages,
    .free_pages = best_free_pages,
    .nr_free_pages = best_nr_free_pages,
    .check = best_fit_check,
};
```

[pmm.c](./code/kern/mm/pmm.c:37) 现在执行：

```c
pmm_manager = &best_fit_pmm_manager;
```

因此 `page_init()` 中调用 `init_memmap()`、后续 `alloc_pages()`/`free_pages()` 以及 `check_alloc_page()` 都会通过 Best-Fit manager 完成。

**4. 验证情况与下一步建议**

实际完成的验证：

```text
+ cc kern/mm/best_fit_pmm.c
+ cc kern/mm/pmm.c
```

随后完整 `make` 编译和链接成功，`bin/kernel`、`bin/ucore.img` 均成功生成。

项目官方 `make grade` 最终结果：

```text
-check page init:                          OK
-check best fit:                           OK
Total Score: 20/20
```

尚未单独验证的事项：

- 现有检查主要验证最小可用块选择和基本的拆分、合并行为，没有构造多个“候选块容量完全相同”的独立测试场景。当前实现通过“仅在严格更小时替换候选”保证地址优先级，但仍建议后续用定向用例验证。
- 尚未进行随机分配、随机释放或长时间碎片化压力测试。
- 先前审查指出的 DTB 保留内存表处理限制仍然存在，但不属于 Best-Fit 任务范围。

Best-Fit 实现、注册和切换已完成，并通过现有编译及评分检查。下一步适合进行人工代码审查；确认无误后再提交 Git。

## 2412419 张祎然 （2号）
## 1. 从阅读文件开始

**我：** 2号部分是我要实现的，你教我怎么读文件学习，然后完成实验，而不是帮我完成，一点一点来。

**AI（解释整理）：** 先看 `memlayout.h` 中的 `struct Page`，了解一个页描述符记录什么；再看 `pmm.h` 中分配器需要提供哪些接口。随后结合 `list.h` 阅读 `default_pmm.c`，按初始化、分配、释放的顺序理解 First-Fit。每次只处理一个函数中的一小段。

## 2. 分清 base 和它指向的页面

**我：** 那base是啥。

**AI（解释整理）：** `base` 是 `struct Page *`，指向这一段物理页的第一个描述符。`base + 1` 指向下一个描述符，对应下一页；它不是给物理地址加1，也不是直接操作那一页里的数据。

**我：** 这里是把当前页的引用计数设成0，我这样写吗。

```c
base = 0;
```

**AI（纠正整理）：** 这会把首页指针改成空指针。当前循环处理的是 `p` 指向的页面，应写 `set_page_ref(p, 0);`，让页面的引用计数归零。

## 3. property 的数值不等于空闲状态

**我：** 不能，因为 `ClearPageProperty(page)` 修改的是 `flags`，原来存着的块长度可能还留在字段里。

**AI（解释整理）：** 判断空闲块首页要看 `PageProperty(page)`，不能只看 `property` 是否大于0。后面的 Buddy 实现还会在已分配首页中保存实际分配页数，用于释放时核对长度，此时空闲标志仍然是清除的。

## 4. 链表初始化和空表判断

**我：** 初始化我写的是这个。

```c
buddy_free_lists[order] = nullptr;
```

判断这个链表有没有空闲块，是不是这样。

```c
if (buddy_free_lists[order] != NULL) {
    return (int)order;
}
```

**AI（纠正整理）：** 数组中存放的是链表哨兵结点，不是一个可以赋空的指针。初始化使用：

```c
list_init(&buddy_free_lists[order]);
```

判断某一阶是否有空闲块使用：

```c
!list_empty(&buddy_free_lists[order])
```

空链表的哨兵指向自己，不能用它是否等于 `NULL` 来判断。

## 5. 阶数与函数调用

**我：** 不会写，但我知道是2的n次方。

**AI（解释整理）：** `order` 阶块包含 `2^order` 页，C 中可以写 `1UL << order`。申请5页需要向上取到8页，对应3阶。这里求的是“最小的、能容纳请求的阶”，不是寻找哪个链表非空。

**我：** `target_order`；所以是 `int found_order = buddy_find_free_order(unsigned int target_order)`。

**AI（纠正整理）：** `unsigned int target_order` 是声明参数时的写法，调用时只传变量：

```c
int found_order = buddy_find_free_order(target_order);
```

返回值保留为 `int`，因为没有合适空闲块时需要返回 `-1`。

## 6. 登记块后继续处理剩余页面

**我：**

```c
buddy_add_block(p, order);
p += block_pages;
remain -= block_pages;
```

**AI（纠正整理）：** 登记、移动首页、扣除剩余页数这三个步骤是连贯的。变量名要与前面的声明保持一致，这里应为 `remaining`。还需要外层 `while (remaining > 0)`，反复选出满足长度和对齐要求的块。

## 7. 找伙伴和摘除结点

**我：**

```c
size_t buddy_ppn = base_ppn(base_ppn, block_pages);
```

**AI（纠正整理）：** `base_ppn` 是页号变量，不是函数。伙伴页号应通过异或计算：

```c
size_t buddy_ppn = base_ppn ^ block_pages;
```

**我：** 这两句我这样写。

```c
list_del(buddy->page_link);
ClearPageProperty(Page->buddy);
```

**AI（纠正整理）：** `list_del` 接收结点地址，应传 `&buddy->page_link`；清除空闲首页标志时传入 Page 指针 `buddy`：

```c
list_del(&buddy->page_link);
ClearPageProperty(buddy);
buddy->property = 0;
```

完成摘链后，还需处理合并后的首页、阶数、最终登记和空闲计数；这一阶段的释放函数尚未完整。

## 8. 合并以后，base应该指向哪里

**我：** 伙伴已经删掉了，下面还要写什么。

**AI（解释整理）：** 先更新合并后块的首页，再把阶数增加1。如果伙伴在 `base` 前面，合并后的首页应该选谁？

**我：** buddy，那这样写吗。

```c
if (buddy > base) {
    base = buddy;
}
order++;
```

**AI（纠正整理）：** 选 `buddy` 的想法对，但比较符号反了。我们需要地址较小的首页，条件应是 `buddy < base`：

```c
if (buddy < base) {
    base = buddy;
}
order++;
```

例如两个4页伙伴块的简化首页编号是8和12，合并后是从8开始的8页块，不会从12开始。

**我：** 所以是地址小的当首页，阶数加1，不是加块的页数。

**AI（解释整理）：** 对。阶数从2变成3，块大小就从4页变成8页。

## 9. 登记空闲块放在循环里面吗

**我：** 那这句就接在order++后面？

```c
buddy_add_block(base, order);
```

**AI（纠正整理）：** 它应该放在合并循环结束之后。循环中还可能继续向上合并，先完成所有合并，再把最终块登记一次。最终的 `buddy_merge_block` 结构是：

```c
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
```

**我：** 这个越界判断能不能放到得到buddy以后。

**AI（解释整理）：** 应在构造和访问伙伴描述符之前判断。如果 `buddy_ppn` 不在管理范围内，就不应继续生成该位置的 Page 指针；除了首页，还要检查整个伙伴块的长度有没有越过上界。

## 10. 最后空闲页数加多少

**我：** 最后合成16页了，那我这样加？

```c
buddy_nr_free += 1UL << order;
```

**AI（纠正整理）：** 如果这个 `order` 是合并后的阶数，就会重复计算伙伴原本已经空闲的页。例如本次释放4页，合并后变成16页，空闲总数只增加新释放的4页。开始释放时先保存实际释放长度：

```c
unsigned int order = buddy_get_order(n);
assert(order <= BUDDY_MAX_ORDER);
size_t released_pages = 1UL << order;
assert(base->property == released_pages);
```

逐页清理完成后，释放函数的结尾是：

```c
buddy_merge_block(base, order);
buddy_nr_free += released_pages;
```

**我：** 那前面申请5页，这里是加5还是8。

**AI（解释整理）：** 加8。当前 Buddy 以整个整幂块为单位，申请5页会占用8页；释放时传入原来的5，`buddy_get_order(5)` 得到3，因此 `released_pages` 为8。不能只回收其中5页。

## 11. 初始化为什么也调用合并函数

**我：** 我前面写的是buddy_add_block，怎么这里变成buddy_merge_block了。

**AI（解释整理）：** 前面那版先解决“把一段内存切成合法块”。最终实现还要处理多次 `init_memmap`：假如第一次登记8页，第二次又登记它的8页伙伴，应该能合成16页。最终初始化循环的这一段改为：

```c
size_t block_pages = 1UL << order;
buddy_merge_block(p, order);
p += block_pages;
remaining -= block_pages;
```

**我：** 合并变大了，那p也按合并后的大小往后走吗。

**AI（纠正整理）：** 仍按本轮新处理的 `block_pages` 移动。被合并的伙伴可能来自之前已经登记的区域，不属于本轮尚未处理的页面。最后也只把这次新增的 `n` 页计入总数：

```c
buddy_nr_free += n;
```

## 12. 函数都写完了，怎么让系统用它

**我：** 是不是文件放进去就会调用。

**AI（解释整理）：** 还需要通过 `pmm_manager` 接口注册，并在构建时选择它。`buddy_pmm.c` 最后这段把各个入口连接起来：

```c
const struct pmm_manager buddy_pmm_manager = {
    .name = "buddy_pmm_manager",
    .init = buddy_init,
    .init_memmap = buddy_init_memmap,
    .alloc_pages = buddy_alloc_pages,
    .free_pages = buddy_free_pages,
    .nr_free_pages = buddy_nr_free_pages,
    .check = buddy_check,
};
```

**我：** 那我直接make就行？

**AI（纠正整理）：** 当前工程默认是 Best-Fit，选择 Buddy 使用：

```bash
make PMM=buddy
make PMM=buddy qemu
```

运行后还要确认输出中确实出现 `memory management: buddy_pmm_manager`，避免把其他分配器的结果当成 Buddy 的结果。

## 13. 怎么检查3页和5页的取整

**我：** 申请成功就可以了吧。

**AI（纠正整理）：** 还要检查实际块长、空闲标志和页数变化。下面是隔离测试中的节选，开始时已经准备好64页空闲池：

```c
size_t requests[] = {1, 2, 3, 5, 8, 16};
size_t expected[] = {1, 2, 4, 8, 8, 16};
struct Page *allocated[6];
size_t used = 0;

for (size_t i = 0; i < 6; i++) {
    allocated[i] = buddy_alloc_pages(requests[i]);
    assert(allocated[i] != NULL &&
           allocated[i]->property == expected[i]);
    assert(!PageProperty(allocated[i]));
    used += expected[i];
    assert(buddy_nr_free == 64 - used);
    buddy_check_invariants();
}
```

**我：** used为什么不加requests[i]。

**AI（解释整理）：** 因为申请3页实际占4页，申请5页实际占8页，总空闲数也按实际占用扣除。这里加 `expected[i]` 才能检查实现是否把内部碎片也算进去了。

## 14. 两个相邻空闲块为什么申请不了大块

**我：** 空闲32页，申请32页还失败，是不是分配写错了。

**AI（解释整理）：** 要看能否组成合法的32页 Buddy 块。下面的测试先把64页分为四个16页块，然后释放中间两个。它们地址相邻，但不是伙伴。

```c
struct Page *quarters[4];
for (size_t i = 0; i < 4; i++) {
    quarters[i] = buddy_alloc_pages(16);
    assert(quarters[i] == fixture + i * 16);
}

buddy_free_pages(quarters[1], 16);
buddy_free_pages(quarters[2], 16);
assert(buddy_nr_free == 32 && buddy_alloc_pages(32) == NULL);
```

**我：** 所以这个NULL才是正确的。

**AI（解释整理）：** 对，这个测试就是检查实现有没有把“相邻”误当成“伙伴”。再释放最前面的16页后，它能与第2个块组成32页，此时应分配成功：

```c
buddy_free_pages(quarters[0], 16);
struct Page *half = buddy_alloc_pages(32);
assert(half == fixture);
```

以上是完整测试函数中的节选，测试末尾还会释放剩余对象并检查64页是否全部恢复。

## 15. 页数恢复了，还要测数据吗

**我：** 最后空闲页数跟开始一样，应该就够了。

**AI（纠正整理）：** 还不够。如果两个分配对象重叠，计数也可能看起来正常。真实 RAM 测试会在各块首尾写入不同标记，然后在其他块分配、释放后再次检查。以下是写入部分的节选：

```c
size_t bytes = p->property * PGSIZE;
volatile unsigned char *data =
    (volatile unsigned char *)(page2pa(p) + va_pa_offset);
data[0] = (unsigned char)(i + 1);
data[bytes - 1] = (unsigned char)(255 - i);
```

检查时重新从每个存活对象取得地址和长度，并验证数据及对象区间不重叠。总页数还需要满足：

```c
assert(buddy_nr_free + used == initial);
```

**我：** 那20/20是不是就是这些都过了。

**AI（解释整理）：** `20/20` 对应原评分脚本的 page_init 和 Best-Fit 检查；Buddy 另外有8组测试。原日志分别记录了以下结果节选：

```text
Buddy self-test: ALL PASS; initial=31927 final=31927
check_alloc_page() succeeded!
```

```text
  -check page init:                          OK
  -check best fit:                           OK
Total Score: 20/20
```

两类结果要分别说明。对应测试截图见 [report.md](report.md) 第5.1、5.2节。

## 16. 双GDB先停在哪条指令

**我：** 两个GDB我不知道先看哪个。

**AI（解释整理）：** 先在目标 RISC-V GDB 中定位内核的第一条数据加载。探针先刷新地址翻译，再连续执行两次同址读取，关键汇编节选如下：

```c
"sfence.vma\n"
".global lab2_probe_first\n"
"lab2_probe_first:\n"
"ld %0, 0(%2)\n"
".global lab2_probe_second\n"
"lab2_probe_second:\n"
"ld %1, 0(%2)\n"
```

这几行位于 `mmu_probe.c` 的内联汇编中，操作数分别连接两个结果变量与同一个数据地址。目标 GDB 的关键命令是：

```gdb
break *lab2_probe_first
continue
x/3i $pc
info registers pc satp
p/x &mmu_probe_data
```

**我：** 那另一个GDB看这个ld怎么找物理地址？

**AI（解释整理）：** 对。宿主 GDB 调试的是 QEMU，针对本次数据地址设置条件断点：

```gdb
break riscv_cpu_tlb_fill if address == 0xffffffffc0208000 && access_type == 0
continue
```

宿主继续后，在目标端执行 `si`。目标等待这条指令完成时，宿主可以停在数据读取的缺失处理入口。该地址来自本次镜像，重新编译后应重新查询。

## 17. Sv39为什么没有查满三级

**我：** 不是三级页表吗，怎么这里就结束了。

**AI（解释整理）：** Sv39 最多三级，但遇到叶子项就停止。宿主执行到读取 PTE 后，查看这些变量：

```gdb
p levels
p idx
p/x base
p/x pte_addr
p/x pte
p ptshift
```

本次实际日志中的值对应：

```text
levels   = 3
idx      = 511
base     = 0x80206000
pte_addr = 0x80206ff8
pte      = 0x200000cf
ptshift  = 18
```

这是把本次调试中的变量和值并列整理，对应截图和地址计算见 [report.md](report.md) 第5.5.4节。

**我：** 有R、W、X就直接用了？

**AI（解释整理）：** 还需要检查有效位、权限组合、大页对齐等条件。本次该项是合法的1 GiB叶子，因此直接形成物理地址 `0x80208000`，不用继续查询下面两级。

## 18. TLB字段怎么查看

**我：** 我想看TLB，是这样吗。

```gdb
p/x $cpuenv->tlb
```

**AI（纠正整理）：** 这个字段路径不适用于宿主 QEMU 4.1.1。实际排错日志中出现过成员不存在的问题，需要从 `RISCVCPU` 的 `neg.tlb` 查看。在 `riscv_cpu_tlb_fill` 断点处可以建立变量：

```gdb
set $rvcpu = (RISCVCPU *)cs
set $cpuenv = &$rvcpu->env
set $va = 0xffffffffc0208000
set $idx = ($va >> 12) & ($rvcpu->neg.tlb.f[1].mask >> 5)
p/x $rvcpu->neg.tlb.f[1].table[$idx].addr_read
```

本次填充前是无效标签 `0xffffffffffffffff`；执行完登记后再查看，标签变成目标虚拟页地址 `0xffffffffc0208000`。

**我：** 那VA加addend就是物理地址吗。

**AI（纠正整理）：** 是宿主进程的 RAM 地址，不是 RISC-V 物理地址。填充后可以实际读取：

```gdb
p/x $rvcpu->neg.tlb.f[1].table[$idx].addend
x/gx (void *)($va + $rvcpu->neg.tlb.f[1].table[$idx].addend)
```

本次宿主地址是 `0x7fffe0008000`，数据是 `0x1122334455667788`；RISC-V 数据物理地址则是前面得到的 `0x80208000`。

## 19. 第二次读到相同的值就算命中吗

**我：** a2和a5一样，就是命中了吧。

**AI（纠正整理）：** 数据相同还不够，重新遍历页表也可能得到相同数据。还要保留目标地址的填充断点，并记录调用次数。首次设置断点时，脚本同时配置了计数：

```gdb
set $fills = 0
commands 1
silent
set $fills = $fills + 1
end
```

这里的 `1` 是本次会话中该条件断点的编号，其他会话应按实际编号调整。完成两条 `ld` 后，目标端和宿主端分别检查：

```gdb
# 目标端：检查两个加载结果
info registers a2 a5
```

```gdb
# 宿主端：检查目标地址的填充次数
p $fills
info breakpoints
```

**我：** fills还是1，那第二次没有再走缺失处理。

**AI（解释整理）：** 对。本次还观察到断点记录 `breakpoint already hit 1 time`，且两次加载都完成、数据一致。这些证据共同支持第二次复用了已有翻译。

## 20. Bare模式怎么作对照

**我：** 直接把satp改成0就行了？

**AI（纠正整理）：** 不能继续沿用原来的高虚拟地址。这个对照使用同一条 `ld` 的物理位置，并把数据地址也改为物理地址。以下是本次已暂停调试会话中目标端的设置节选，完整双端执行顺序由 `debug_mmu.py` 协调：

```gdb
set $satp = 0
set $pc = 0x80201a40
set $a1 = 0x80208000
info registers pc satp a1
x/i $pc
```

**我：** 关了分页怎么还进get_physical_address。

**AI（解释整理）：** 看它走的分支。Bare 模式中关键处理是：

```c
*physical = addr;
*prot = PAGE_READ | PAGE_WRITE | PAGE_EXEC;
```

此时不遍历 Sv39 页表。QEMU仍要把客体地址连接到宿主内存或设备，所以还能使用软件 TLB。目标端实际读出的 `a2` 仍为 `0x1122334455667788`。这只是受控调试对照，结束后终止该次 QEMU，不把临时寄存器状态作为正常内核运行方式。

## 21. 提交文件怎么整理

**我：** 一个分支对应一次实验，命名labx。分支里放code和report，report里面要report.md、prompt.md和images。现在没有的再整理一下。

**AI（解释整理）：** 本次对应 `lab2`。实现代码放入 `code/`，报告统一命名为 `report/report.md`，已保存的学习问答与实现、测试、调试、截图和交付要求集中在 `report/prompt.md`，报告引用的36张截图保存在 `report/images/`。

```text
lab2 分支根目录/
├── code/
│   ├── kern/
│   ├── libs/
│   ├── tools/
│   └── Makefile
└── report/
    ├── report.md
    ├── prompt.md
    └── images/
```


## 22. First-Fit阅读的最终提示词

**我：** 最终要求整理为：

```text
请结合 memlayout.h、pmm.h、list.h 和 default_pmm.c，按初始化、分配、释放的顺序教我阅读 First-Fit。每次只解释一小段，重点说明 base、property、空闲标志和链表节点的区别，再用具体页数让我判断拆分和合并的结果。
```

**AI（解释整理）：** 这段对应报告第4.1节。先分清 Page 描述符、空闲块首页和链表节点，再结合具体页数理解分配和合并，前面第1—7节保留了相关问答。


## 23. Buddy实现与测试的最终提示词

**我：** 最终要求整理为：

```text
阅读现有 Page 结构、链表接口和 pmm_manager，在 buddy_pmm.c 中实现并注册 Buddy 物理页分配器。

每阶使用带哨兵的循环双向链表，最大阶为15；申请向上取整到2的幂，采用整块释放。空闲标志与块长度分别维护。

初始化支持非对齐起点和非整幂长度。对齐和伙伴查找统一使用绝对物理页号，伙伴为 ppn ^ (1UL << order)。先检查边界，再访问伙伴描述符；只合并空闲、同阶的真正伙伴，合并后保留低地址首页。

拆分和合并只改变块的组织方式，分配、释放分别按实际页数更新一次总空闲数。补齐管理器接入和运行依赖，保留原测试及断言。

测试应覆盖取整、非对齐区间、相邻但非伙伴、越界和释放后恢复，并在真实QEMU内存中检查数据、不重叠和页数守恒。保存实际命令与输出。
```

**AI（解释整理）：** 这段对应报告第4.2节。先统一整块释放、对齐和计数约定，再实现拆分与合并，最后用边界测试和真实内存读写验证。前面第8—15节保留了具体代码及纠错过程。


## 24. 双GDB调试的最终提示词

**我：** 最终要求整理为：

```text
在同一个QEMU 4.1.1进程上使用宿主GDB和RISC-V目标GDB。用两条同址ld构造可重复的访问，在目标端定位指令，在宿主端用地址条件断点排除无关访存。

记录第一次缺失的调用栈、SATP、根页表地址、索引、PTE、最终物理地址和软件TLB槽位；执行第二次读取，检查结果与目标地址填充次数。最后使用同一条ld的物理位置作Bare模式对照。

解释本次根层大页为什么不需要查满三级，以及QEMU软件TLB中的宿主地址偏移与客体物理地址有什么区别。
```

**AI（解释整理）：** 这段对应报告第4.3节。目标端控制内核指令，宿主端观察QEMU翻译路径；两次数据相同还要结合填充次数，才能说明第二次复用了已有翻译。前面第16—20节保留了调试问答。

## 2413788 张旗庭 （3号）
本实验主要使用 AI 辅助理解实验要求、分析 Makefile 与链接流程、设计 GDB 调试步骤以及解释实际运行结果。

实验过程中与 AI 进行了多轮交互，并且本实验多为理解类问题，其中有许多细小知识点追问，以下内容为根据实际实验过程，同时让AI协助总结整理得到的关键 Prompt。

---

## Prompt 1：分析 Makefile 与内核编译链接流程

### [PROMPT]

阅读 Lab1 项目中的 `Makefile`，结合实际项目结构，解释从 `.c/.S` 源文件到最终内核镜像的完整构建过程。

重点分析：

- `GCCPREFIX`、`CC`、`LD`、`OBJCOPY`、`OBJDUMP`、`GDB` 分别代表什么；
- `KSRCDIR`、`LIBDIR` 如何确定参与编译的源文件；
- `add_files_cc` 和 `KOBJS` 在构建流程中的作用；
- `.c/.S` 如何生成 `.o`；
- 多个 `.o` 如何链接生成 `bin/kernel`；
- `bin/kernel` 如何进一步生成 `bin/ucore.img`；
- `make` 为什么能够自动完成上述过程。

不要只解释 Makefile 语法，而要结合本实验实际生成的文件说明每一步的作用。

### [RELY]

可以依赖当前 Lab1 项目中的以下内容：

```text
Makefile
tools/function.mk
tools/kernel.ld
kern/
libs/
```

已知本实验使用的 RISC-V 交叉工具链前缀为：

```text
riscv64-unknown-elf-
```

实际执行 `make` 后可以观察到：

```text
+ cc kern/init/entry.S
+ cc kern/init/init.c
+ cc kern/libs/stdio.c
...
+ ld bin/kernel
riscv64-unknown-elf-objcopy bin/kernel --strip-all -O binary bin/ucore.img
```

### [GUARANTEE]

必须说明以下构建链：

```text
.c / .S
↓
.o
↓
ld + kernel.ld
↓
bin/kernel（ELF）
↓
objcopy
↓
bin/ucore.img
```

必须区分：

```text
CC
LD
OBJCOPY
OBJDUMP
GDB
```

各自的功能。

不得把 `add_files_cc`、`totarget`、`read_packet` 等课程 Makefile 辅助函数解释成标准 GNU Make 内建命令。

### [SPECIFICATION]

#### Pre-Condition

当前位于 Lab1 项目根目录，Makefile 和源码均为课程提供版本。

#### Post-Condition

分析后应能够解释：

1. 为什么直接执行 `make` 可以生成内核；
2. 为什么链接前需要 `.o` 目标文件；
3. `bin/kernel` 与 `bin/ucore.img` 分别是什么；
4. Makefile 中哪些规则真正负责链接和镜像转换；
5. 哪些内容只是辅助分析、清理或调试规则。

---

## Prompt 2：分析 `kernel.ld`、ELF/BIN 与内核地址布局

### [PROMPT]

结合 `tools/kernel.ld`、`bin/kernel` 和 `bin/ucore.img`，分析 Lab1 中内核的链接地址、section 布局以及 ELF 与裸二进制镜像的区别。

重点解释：

```ld
OUTPUT_ARCH(riscv)
ENTRY(kern_entry)
BASE_ADDRESS = 0x80200000;
```

以及：

```text
.text
.rodata
.data
.bss
```

各 section 的主要作用。

同时使用 `readelf`、`nm`、`objdump` 等工具验证链接结果，并解释为什么最终 ELF 入口地址为 `0x80200000`。

### [RELY]

可以依赖：

```text
tools/kernel.ld
bin/kernel
bin/ucore.img
```

以及以下实际命令：

```bash
file bin/kernel
file bin/ucore.img

riscv64-unknown-elf-readelf -h bin/kernel
riscv64-unknown-elf-readelf -S bin/kernel

riscv64-unknown-elf-nm -n bin/kernel

riscv64-unknown-elf-objdump -d bin/kernel
```

实际实验结果包括：

```text
Entry point address: 0x80200000
```

以及：

```text
kern_entry    = 0x80200000
bootstack     = 0x80201000
bootstacktop  = 0x80203000
edata         = 0x80203008
end           = 0x80203008
```

### [GUARANTEE]

必须说明：

- `kernel.ld` 是链接脚本；
- 链接器负责符号解析、重定位以及最终地址确定；
- `kernel.ld` 主要规定入口地址和各 section 的布局；
- `bin/kernel` 是 ELF 文件；
- `bin/ucore.img` 是由 `objcopy -O binary` 生成的裸二进制镜像；
- ELF 文件适合符号分析和 GDB 调试；
- `ucore.img` 用于本实验中的 QEMU 直接加载。

不得把 `kernel.ld` 描述成负责决定“哪些函数互相调用”的文件。

### [SPECIFICATION]

#### Pre-Condition

`make` 已成功执行，并生成：

```text
bin/kernel
bin/ucore.img
```

#### Post-Condition

分析结果需要能够回答：

1. `0x80200000` 从哪里来；
2. ELF 的入口地址为什么是 `0x80200000`；
3. `.text/.rodata/.data/.bss` 分别保存什么；
4. ELF 与 BIN 的区别；
5. 为什么 GDB 使用 `bin/kernel`，而 QEMU 加载 `bin/ucore.img`。

---

## Prompt 3：使用 GDB 验证 RISC-V 从复位到 uCore 内核入口的启动流程

### [PROMPT]

指导我使用 QEMU + GDB 对 Lab1 的启动过程进行动态调试，从 CPU 复位后的第一条指令开始，一直跟踪到 uCore 的 `kern_entry`。

需要逐步给出 GDB 命令，并在每一步说明：

- 当前应该观察哪个寄存器；
- 当前 PC 应处于什么位置；
- 当前指令完成了什么；
- 下一步为什么执行该命令。

重点验证启动路径：

```text
0x1000
↓
0x80000000
↓
0x80200000
```

并回答 CPU 复位后最初几条指令位于哪里以及主要完成什么工作。

### [RELY]

Makefile 已提供：

```bash
make debug
make gdb
```

`make debug` 使用：

```text
-s -S
```

使 QEMU 暂停 CPU 并等待 GDB 连接。

已知 GDB 连接成功后初始位置为：

```text
0x0000000000001000
```

可以使用：

```gdb
i r pc
i r t0
si
x/10i $pc
b *0x80200000
c
```

进行调试。

### [GUARANTEE]

必须实际验证：

```text
PC = 0x1000
```

并分析：

```asm
0x1000: auipc t0,0x0
0x1004: addi  a1,t0,32
0x1008: csrr  a0,mhartid
0x100c: ld    t0,24(t0)
0x1010: jr    t0
```

必须通过单步观察：

```text
t0 = 0x80000000
```

并确认执行 `jr t0` 后：

```text
PC = 0x80000000
```

最后在：

```text
0x80200000
```

设置断点并确认命中：

```text
0x80200000 <kern_entry>
```

对 `x/10i $pc` 输出中后续出现的 `unimp`，必须区分“GDB 将后续数据按指令解释”与“CPU 实际执行非法指令”这两个概念。

### [SPECIFICATION]

#### Pre-Condition

QEMU 已通过 `make debug` 启动，GDB 已通过 `make gdb` 成功连接。

#### Post-Condition

最终应得到并解释：

```text
CPU 复位
↓
0x1000
↓
QEMU 复位启动代码
↓
0x80000000
↓
OpenSBI
↓
0x80200000
↓
kern_entry
```

所有结论必须依据实际 GDB 输出，而不是仅根据理论流程推断。

---

## Prompt 4：验证 `kern_entry` 中的栈初始化以及进入 `kern_init`

### [PROMPT]

在 GDB 已经停在：

```text
0x80200000 <kern_entry>
```

的基础上，继续调试 `kern/init/entry.S`，验证：

```asm
la sp, bootstacktop
tail kern_init
```

两条源码的实际作用。

需要解释：

- `sp` 是什么；
- 为什么进入 C 函数前需要设置栈；
- `bootstacktop` 是什么；
- `la` 和 `tail` 为什么在反汇编中显示为其他机器指令；
- 如何证明程序最终进入 `kern_init()`。

进一步验证 `kern_init()` 是否实际调用 `cprintf()` 并产生启动输出。

### [RELY]

可以使用以下 GDB 命令：

```gdb
i r sp
i r pc
info address bootstacktop
x/3i $pc
si
list
b cprintf
c
bt
```

实际可观察到：

```text
初始 sp = 0x8001bd80
bootstacktop = 0x80203000
```

单步后：

```text
sp = 0x80203000
```

继续执行后：

```text
pc = 0x8020000a <kern_init>
```

并可以在 `cprintf` 处命中断点。

### [GUARANTEE]

必须验证以下关系：

```text
进入 kern_entry
↓
原 sp = 0x8001bd80
↓
执行 la sp, bootstacktop
↓
sp = 0x80203000
↓
执行 tail kern_init
↓
pc = 0x8020000a <kern_init>
```

还需要验证：

```text
kern_init
↓
cprintf
```

的调用关系，并确认继续运行后 QEMU 终端出现：

```text
(THU.CST) os is loading ...
```

不得仅根据源码推断，必须结合实际 GDB 和 QEMU 输出进行说明。

### [SPECIFICATION]

#### Pre-Condition

GDB 已在 `kern_entry` 处暂停。

#### Post-Condition

调试结果应能够证明：

1. uCore 进入内核后主动建立自己的栈；
2. `sp` 最终与 `bootstacktop` 地址一致；
3. 汇编入口成功跳转到 C 函数 `kern_init()`；
4. `kern_init()` 实际调用了 `cprintf()`；
5. QEMU 终端最终成功显示内核启动信息。


---

## 补充交互问题

实验过程中还针对以下问题进行了追加询问：

- `si`、`i r pc`、`i r t0` 的含义；
- `nm`、`readelf`、`objdump` 的作用；
- `sp` 与 `bootstacktop` 的关系；
- GDB 中 `unimp` 的含义；
- `cprintf` 断点与 QEMU 输出的对应关系；
- 链接阶段的符号解析、重定位以及 `kernel.ld` 的作用；
- Makefile 中 `add_files_cc`、`KOBJS`、`totarget` 等辅助函数在整体构建流程中的位置。
