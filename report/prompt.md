# 1号prompt汇总 2413669 张懿鸾

由于本节没有代码书写内容，所以主要是和AI进行了一些理解项目的对话，故没有严格按照提示词模版进行，把内容附在下面。进入代码开发环节之后会严格按照提示词模版和AI进行交互。
> ```
> 我现在看这个Lab1感觉东西很多，有Makefile、kernel.ld、entry.S、init.c，还有QEMU、OpenSBI、GDB，这些到底是什么关系？请帮我梳理一下这些文件的关系
> ```

<details><summary>2 previous messages</summary>

> 我先看看 Lab1 的实际文件结构和内容，再给你梳理它们的关系。
>
> <details><summary>Explored 5 files, 3 searches, ran 6 commands</summary>
>
> - Ran `pwd && ls -la`
>
> <details><summary>Explored 2 searches</summary>
>
> - Searched for `*/.git/*` in `OS-Lab`
> - Searched for `*/obj/*` in `lab1`
>
> </details>
>
> - Ran `cat /home/win/oslab/OS-Lab/report/prompt.md; echo "=== README ==="; cat /home/win/oslab/OS-Lab/report/README.md`
>
> <details><summary>Explored a file</summary>
>
> - Read `~/oslab/OS-Lab/code/lab1/Makefile`
>
> </details>
>
> - Ran `cat /home/win/oslab/OS-Lab/code/lab1/tools/kernel.ld; echo "=== entry.S ==="; cat /home/win/oslab/OS-Lab/code/lab1/kern/init/entry.S`
>
> <details><summary>Explored a file</summary>
>
> - Read `~/oslab/OS-Lab/code/lab1/kern/init/init.c`
>
> </details>
>
> - Ran `cat /home/win/oslab/OS-Lab/code/lab1/kern/mm/memlayout.h; echo "=== mmu.h ==="; cat /home/win/oslab/OS-Lab/code/lab1/kern/mm/mmu.h; echo "=== sbi.h ==="; cat /home/win/oslab/OS-Lab/code/lab1/libs/sbi.h`
>
> <details><summary>Explored a file</summary>
>
> - Read `~/oslab/OS-Lab/code/lab1/tools/function.mk`
>
> </details>
>
> - Ran `head -120 /home/win/oslab/OS-Lab/report/report.md`
>
> <details><summary>Explored a search</summary>
>
> - Searched for `^#\|^###` in `report.md`
>
> </details>
>
> - Ran `ls -la /home/win/oslab/OS-Lab/code/lab1/obj /home/win/oslab/OS-Lab/code/lab1/bin; file /home/win/oslab/OS-Lab/code/lab1/bin/* 2>/dev/null`
>
> <details><summary>Explored 2 files</summary>
>
> - Read `~/oslab/OS-Lab/code/lab1/obj/kernel.sym`
> - Read `~/oslab/OS-Lab/code/lab1/obj/kernel.asm`
>
> </details>
>
> </details>
</details>

我把 Lab1 里这些东西按两条链理清了：**一条是"构建链"（怎么把源码变成能启动的镜像），一条是"运行链"（CPU 上电后怎么一步步跑到你的 C 代码）**。GDB 不在这两条链里，它是旁观的观察工具。下面结合你仓库里 [lab1](~/oslab/OS-Lab/code/lab1) 的实际代码说。

## 一句话总览

| 东西 | 身份 | 在什么时候起作用 |
|------|------|------------------|
| `Makefile` | 构建/运行的总调度 | 构建 + 启动 QEMU/GDB |
| `kernel.ld` | 链接脚本，决定内存布局 | 链接阶段 |
| `entry.S` | 内核真正的第一段代码 | CPU 跳进内核的瞬间 |
| `init.c` | 内核第一个 C 函数 `kern_init` | 栈建好之后 |
| `QEMU` | 模拟出来的 RISC-V 机器 | 一直 |
| `OpenSBI` | 跑在 M-mode 的固件 | 内核之前 |
| `GDB` | 调试器/观察者 | 只在调试时 |

## 构建链：源码 → ucore.img

`Makefile` 是这条链的导演，它做四件事：

```text
entry.S / init.c / libs/*.c / driver/*.c
        │  交叉编译 riscv64-unknown-elf-gcc -c
        ▼
obj/**/*.o        （一堆目标文件）
        │  链接 riscv64-unknown-elf-ld -T tools/kernel.ld
        ▼
bin/kernel        （ELF，带符号、带调试信息）
        │  objcopy --strip-all -O binary
        ▼
bin/ucore.img     （纯二进制，QEMU 真正加载的东西）
```

- `Makefile` 里的 `$(kernel): $(KOBJS)` 那条规则负责链接，`-T tools/kernel.ld` 就是把链接脚本交给 ld。
- **`kernel.ld` 的作用不是"放代码"，而是"决定代码放在哪、谁在最前面"**。它做三件事：`ENTRY(kern_entry)` 指定 ELF 入口是 `kern_entry`；`BASE_ADDRESS = 0x80200000` 把整个内核定位到这个地址；按 `.text → .rodata → .data → .bss` 排好段，并把 `.text.kern_entry` 放在 `.text` 最前面。
- 它还用 `PROVIDE(edata = .)` 和 `PROVIDE(end = .)` 导出两个符号——这就是 `init.c` 里 `memset(edata, 0, end - edata)` 用的那两个名字。文件之间是靠**符号**对接的。
- 从 [kernel.sym](~/oslab/OS-Lab/code/lab1/obj/kernel.sym) 能验证：`kern_entry = 0x80200000`，正好等于 `BASE_ADDRESS`；`bootstack = 0x80201000`，落在 `.data` 段里。

## 运行链：上电 → 你的 C 代码

```text
CPU 复位，PC = 0x1000（QEMU 的复位向量）
        │
        ▼
OpenSBI 在 0x80000000 跑（-bios default）
        │  做底层初始化：内存、时钟、串口…
        │  切换到 S-mode，跳转到 0x80200000
        ▼
kern_entry（entry.S 的第一条指令）
        │  la sp, bootstacktop   ← 先建好内核栈
        │  tail kern_init
        ▼
kern_init()（init.c 的第一个 C 函数）
        │  memset(edata, 0, end-edata)  清零 BSS
        │  cprintf("(THU.CST) os is loading ...")
        │  while(1);
        ▼
挂住，等后续实验再加东西
```

这条链里每个环节的职责：

- **QEMU 是"机器"**。它虚拟出一颗 RISC-V CPU 和一块从 `0x80000000` 开始的物理内存。它不关心你的操作系统逻辑，只负责"假装自己是一台电脑"。
- **OpenSBI 是"固件"**，类比 PC 上的 BIOS/UEFI。`make qemu` 里的 `-bios default` 让它被加载到 `0x80000000`。它跑在最高权限的 M-mode，把机器初始化好后**降级**到 S-mode，把控制权交给内核地址 `0x80200000`。内核和硬件（比如串口）之间的很多事都要通过它——`libs/sbi.c` 里的 `sbi_console_putchar` 就是内核用 `ecall` 请求 OpenSBI 帮忙往串口吐一个字符。
- **为什么内核必须是 `0x80200000`？** 因为这是 QEMU 约定好的"固件结束、内核开始"的位置，OpenSBI 初始化完就跳到这儿。所以 `Makefile` 里用 `-device loader,file=ucore.img,addr=0x80200000` 把镜像放进去，`kernel.ld` 也必须把内核链接到 `0x80200000`——**两边地址必须对上，否则跳过去就是一团垃圾**。
- **`entry.S` 为什么是汇编、为什么排第一？** 因为 C 编译出来的代码一开始就要用栈（存 `ra`、局部变量），而开机时 `sp` 是没意义的。所以必须先用汇编 `la sp, bootstacktop` 把栈指针指向 `entry.S` 自己在 `.data` 里预留的 `bootstack`，然后才能 `tail kern_init` 进 C。
- **`init.c` 是"内核逻辑的真正起点"**。到这个函数时，栈、段布局、地址都已经就绪，终于可以写 C 了。它做两件最小的事：清零 `.bss`（因为纯二进制镜像里没有 `.bss` 的实际内容），然后打印一行字，最后 `while(1)` 挂住。

## GDB 和它们什么关系

GDB **不参与启动**，它只是挂在旁边看。`make debug` 启动 QEMU 时加了 `-s -S`，意思是"开启调试端口 1234，并且在开机前先暂停"；`make gdb` 再用 `target remote localhost:1234` 连上去。之后 GDB 就能冻结 CPU、看 PC/寄存器、下断点、单步——你是用它来**证明**上面那条运行链真的发生了（比如看到 PC 从 `0x1000` → `0x80000000` → `0x80200000`）。

## 地址视角（把两条链钉在一起）

| 地址 | 谁 | 来自哪里 |
|------|-----|---------|
| `0x1000` | QEMU 复位向量 | QEMU 硬件约定 |
| `0x80000000` | OpenSBI 固件 | `-bios default` |
| `0x80200000` | `kern_entry` | `kernel.ld` 的 `BASE_ADDRESS` + `ENTRY` |
| `0x80203000` | `bootstacktop` | `entry.S` 的 `.data` 段 |

一条线记住它：**QEMU 提供机器 → OpenSBI 做固件、移交控制权 → `kernel.ld` 决定了内核落在 `0x80200000` 和谁先执行 → `entry.S` 在那里先建栈 → `init.c` 开始跑 C 内核；`Makefile` 负责把这一切编译、链接、打包并启动，GDB 负责在旁边验证。**

如果你想，我可以把这段整理成一个「Lab1 文件与组件关系总览」小节补进 [report.md](./report.md)，接在第三章开头。

> QEMU、OpenSBI 和 uCore这几个概念能帮忙梳理一下吗？

<details><summary>1 previous message</summary>

> <details><summary>Explored 2 files</summary>
>
> - Read `~/oslab/OS-Lab/code/lab1/libs/sbi.c`
> - Read `~/oslab/OS-Lab/code/lab1/kern/driver/console.c`
>
> </details>
</details>

这三者其实是**三个不同层次的东西**，用一句话区分：**QEMU 是"机器"，OpenSBI 是"固件"，uCore 是"操作系统"**。你在 Lab1 里同时碰到的困惑，本质上是"一台虚拟电脑从上电到跑起 OS"的完整叠层。

## 三层结构

```text
┌─────────────────────────────────────────────────┐
│  uCore          操作系统内核（你写的东西）        │  S-mode
│  entry.S / init.c / libs/ / driver/ …            │
├─────────────────────────────────────────────────┤
│  OpenSBI         固件 / 运行环境（别人写好给你用） │  M-mode
│  提供 SBI 服务：串口、定时器、IPI、关机…          │
├─────────────────────────────────────────────────┤
│  QEMU            虚拟硬件（"电脑"本身）           │  ——
│  RISC-V CPU + 内存 + UART + CLINT/PLIC …         │
└─────────────────────────────────────────────────┘
      真实世界：x86 主机 + WSL/Linux + QEMU 进程
```

## 逐个说清楚

**QEMU = 虚拟出来的那台电脑，不是操作系统的一部分**

它用 `qemu-system-riscv64` 在 x86 主机上"假装"出一颗 RISC-V CPU 和一块内存，以及串口、定时器等外设。`-machine virt` 选的是它内置的一个虚拟主板型号，这个型号固化了内存布局：DRAM 从 `0x80000000` 开始、复位向量在 `0x1000`、UART 在 `0x10000000` 等等。

要特别注意一个用词陷阱：平时操作系统课上说的"虚拟机"常指"OS 里跑的程序以为自己独占 CPU"；而 QEMU 是**整机模拟器**，它模拟的是硬件的另一颗 CPU 和另一台机器，和 uCore 是"机器 ↔ 操作系统"的关系，不是"OS ↔ 进程"的关系。Lab1 里 QEMU 的角色就是"硬件"。

**OpenSBI = 固件，类比 PC 上的 BIOS/UEFI**

SBI 是 Supervisor Binary Interface，OpenSBI 是这套接口的一个实现。它跑在 RISC-V 最高特权级 **M-mode（Machine mode）**，在 QEMU 里由 `-bios default` 自动加载到 `0x80000000`。它干两件事：

1. 上电后做最底层的初始化（内存、时钟、中断控制器、串口等），这些事只有 M-mode 有权限做；
2. 然后**降级到 S-mode**，跳到 `0x80200000`，把控制权交给 uCore。

所以内核不需要、也不允许自己直接操作 M-mode 的寄存器和很多硬件细节，而是通过 `ecall` 指令"向上"请求 OpenSBI 代劳。这正是为什么 uCore 在 `0x80200000`——`0x80000000` 到 `0x80200000` 这 2MB 是留给 OpenSBI 的地盘，你的内核紧挨在它后面。

**uCore = 你要写的操作系统内核，跑在 S-mode**

它就是你在这门课里真正开发的对象。Lab1 里它还极简，只有 [`entry.S`](~/oslab/OS-Lab/code/lab1/kern/init/entry.S) 建栈 + [`init.c`](~/oslab/OS-Lab/code/lab1/kern/init/init.c) 打印一行字然后死循环。它和 OpenSBI 的分工是"**操作系统负责策略和管理，固件负责最底层的硬件服务**"。

## 三者怎么衔接

| | QEMU | OpenSBI | uCore |
|---|---|---|---|
| 是什么 | 虚拟硬件 | 固件 | 操作系统内核 |
| 谁写的 | QEMU 项目 | OpenSBI 项目 | **你** |
| 运行特权级 | ——（提供硬件） | M-mode | S-mode |
| 在内存哪里 | 整个地址空间 | `0x80000000` 起 | `0x80200000` 起 |
| 上电顺序 | 第 1 | 第 2 | 第 3 |

启动时的时间线：

```text
QEMU 进程启动
  → 虚拟 CPU 复位，PC = 0x1000（QEMU 的复位代码）
  → 跳到 0x80000000，开始执行 OpenSBI（M-mode）
  → OpenSBI 初始化硬件，然后用 sret/mret 降级到 S-mode
  → PC = 0x80200000，进入 uCore 的 kern_entry
  → la sp, bootstacktop → tail kern_init → 你的 C 代码
```

## 一个具体例子：`cprintf` 怎么穿过这三层

你 Lab1 里那行 "(THU.CST) os is loading ..." 的输出，完整地穿过了 uCore 和 OpenSBI 的边界：

```text
uCore:  cprintf()          kern/libs/stdio.c
          ↓
uCore:  cons_putc()        kern/driver/console.c
          ↓
uCore:  sbi_console_putchar()   libs/sbi.c
          ↓  x17=1(SBI_CONSOLE_PUTCHAR), x10=字符
uCore:  ecall  ────────────►  特权级从 S 切到 M
                                 ↓
OpenSBI: 收到 ecall，按功能号 1 处理，真正去操作 UART 硬件
                                 ↓
OpenSBI: sret  ────────────►  特权级从 M 切回 S
uCore:  继续执行
```

所以 [`libs/sbi.c`](~/oslab/OS-Lab/code/lab1/libs/sbi.c) 里那个 `ecall` 不是普通函数调用，而是**uCore 向 OpenSBI 请求服务的唯一通道**。你的内核自己碰不到 UART，是 OpenSBI 帮它碰的。

## 一句话记忆

**QEMU 虚拟出一台 RISC-V 电脑，OpenSBI 是这台电脑的固件、负责点亮机器并把权限降给操作系统，uCore 就是装在这台电脑上、跑在 S-mode 的操作系统内核。** 三者是"机器 → 固件 → 操作系统"的叠层关系，越往下特权越高、越靠近硬件，越往上越接近你在课上要实现的逻辑。

需要的话，我可以把「QEMU / OpenSBI / uCore 三层关系」整理成一小节补进 [report.md](./report.md)。

> 我看好几个地方都出现了`0x80200000`  ，我知道这个是kern_entry所在的地方，就是opensbi运行结束之后进入的汇编入口，不同地方出现的`0x80200000`  有啥不同的作用呢？能区别一下吗？

<details><summary>2 previous messages</summary>

> 我先把仓库里所有出现 `0x80200000` 的地方都找出来，再逐个区分它们各自的作用。
>
> <details><summary>Explored a file, a search, ran 2 commands</summary>
>
> - Ran `grep -rn "80200000\|0x8020000" /home/win/oslab/OS-Lab/code/lab1 --include=* -I 2>/dev/null | grep -v '^Binary'`
>
> <details><summary>Explored a file, a search</summary>
>
> - Searched for `80200000\|0x8020000` in `report.md`
> - Read `./report.md`
>
> </details>
>
> - Ran `sed -n '160,200p' /home/win/oslab/OS-Lab/report/report.md; echo "--- 560-595 ---"; sed -n '560,595p' /home/win/oslab/OS-Lab/report/report.md`
>
> </details>
</details>

问得很好——这正是很多人在 Lab1 里绕不过去的一个坎。`0x80200000` 出现的每一处，**数值是同一个，但"它在对谁说话、在什么时候起作用"完全不同**。你可以把它理解成：同一个地址，被四套系统各自"理解"了一次。下面按作用分层。

## 四类角色

**① 链接时的"决策"——`tools/kernel.ld` 的 `BASE_ADDRESS`**

```ld
BASE_ADDRESS = 0x80200000;
. = BASE_ADDRESS;
```

这是**源头**。它告诉链接器："把内核的代码段从 `0x80200000` 这个地址开始摆。"这是一个**约定/主张**：链接器据此给每个符号、每条指令分配地址。你在 Lab1 里没有开分页，CPU 运行在裸机模式下，所以这里的链接地址**直接就是物理地址**。如果以后开了页表，链接地址（虚拟地址）和实际装载的物理地址就可以不一样了——Lab1 里它们恰好相等，是简化。

**② 由①派生出来的"记录"——符号表、ELF 头、反汇编**

这三处的 `0x80200000` 都不是独立的决定，而是**同一件事的三种表达**：

```text
obj/kernel.sym:  0000000080200000 kern_entry      ← 符号 kern_entry 的值
bin/kernel:      Entry point address: 0x80200000  ← ELF 头的 e_entry 字段
obj/kernel.asm:  0000000080200000 <kern_entry>:   ← 反汇编给这条指令标的地址
                 80200000:  auipc sp,0x3
```

- `kernel.sym` 是 `objdump -t` 导出的：链接器按 `BASE_ADDRESS` 排完布局后，`kern_entry` 这个标签正好落在最前面，值就是 `0x80200000`。
- ELF 的 `Entry point address` 是 `ld` 根据 `ENTRY(kern_entry)` 把符号值写进 ELF 头的 `e_entry`，本质是"把①的结论抄进文件"。
- `kernel.asm` 里那个 `80200000:` 是反汇编输出时根据符号/段地址标出来的，**你分析时用它对照 PC 很有用，但它本身不控制任何东西**。

一句话：这三处都是**只读的记录**，它们能证明"内核认为自己在 `0x80200000`"，但不会让 CPU 真的去那儿。

**③ 装载时的"物理摆放"——`Makefile` 的 `-device loader,addr=`**

```make
-device loader,file=$(UCOREIMG),addr=0x80200000
```

这一处和上面性质完全不同：它是一条**运行时命令**，告诉 QEMU "把 `ucore.img` 里的字节原样铺到客户机物理内存 `0x80200000` 开始的位置"。

为什么必须显式给地址？因为 `ucore.img` 是 `objcopy --strip-all -O binary` 生成的**裸二进制**，它里面**没有地址信息**，只是一个字节流。文件的第 0 个字节该放到哪，只有 `-device loader` 说了算。文件里的第 0 个字节恰好就是内核第一个段的内容，所以这里填 `0x80200000` 才能让字节和①的布局对上。

**④ 运行时的"控制权交接"和"验证"**

- **OpenSBI 跳转目标**：OpenSBI 初始化完必须知道往哪跳。QEMU 用 `fw_dynamic` 方式启动 OpenSBI 时，会在内存里放一个信息结构，其中 `next_addr = 0x80200000`、`next_mode = S`。所以 OpenSBI 最后是"跳到 `0x80200000` 并降到 S-mode"。**这才是真正把 PC 送进内核的那一步。**
- **GDB 里的 `pc = 0x80200000 <kern_entry>`**：这是 CPU 此刻的**运行时状态**，是前面①②③④全部对齐之后**产生的结果**，也是你用来证明启动链正确的证据。它不是任何"设定"，而是"观察"。

## 汇总对比

| 出现位置 | 角色 | 谁决定 | 何时生效 | 性质 |
|---|---|---|---|---|
| `kernel.ld` `BASE_ADDRESS` | 链接布局起点 | 你 | 链接时 | **决策/约定** |
| `kernel.sym` `kern_entry` | 符号的地址值 | ld（据①） | 链接后固定 | 记录（派生） |
| ELF `Entry point address` | 入口记录 | ld（`ENTRY()`） | 写入文件 | 记录（派生） |
| `kernel.asm` `80200000:` | 指令地址标注 | objdump | 分析时 | 记录（派生） |
| `Makefile` `-device loader,addr=` | 把字节放到物理内存 | 你 | QEMU 启动时 | **物理摆放** |
| OpenSBI 的 `next_addr` | 交接后的跳转目标 | QEMU/OpenSBI 约定 | 运行时 | **控制流交接** |
| GDB `pc = 0x80200000` | CPU 实际执行到哪 | CPU 状态 | 调试时 | **观察/验证** |

## 核心结论

**`0x80200000` 在不同地方的区别，本质是"同一条约定在四个环节被兑现"：**

```text
链接器（kernel.ld）  说：内核的地址布局从 0x80200000 开始
装载器（QEMU loader） 做：把 ucore.img 的字节放到 0x80200000
固件（OpenSBI）       做：初始化完跳向 0x80200000
CPU / GDB             证：PC 真的落在 0x80200000 <kern_entry>
```

只有当这四处**全部对齐**时，内核才能跑起来：如果 `kernel.ld` 和 `Makefile` 的地址不一致，代码会"以为"自己在 A 却实际被放在 B，任何基于 PC 的相对跳转（比如 `auipc`、`la`）都会算错；如果 OpenSBI 的跳转目标和装载地址不一致，它一跳过去就是一堆垃圾指令。Lab1 把它们都设成同一个值，不是巧合，而是**必须如此**。

另外别忘了 `0x80000000 + 0x200000 = 0x80200000`：前 2MB 归 OpenSBI，内核紧贴其后。所以 `0x80200000` 同时也是"**固件地盘的边界**"——这个含义在链接脚本里体现为起点，在 OpenSBI 眼里体现为"我的地盘到此结束，接下来是操作系统"。

如果你想，我可以把这份"四处 `0x80200000` 的区别"整理成一个小节补进 [report.md](./report.md) 的 3.3.3 附近，它正好能把链接时地址和运行时地址那条线讲得更透。

> 我在做gdb调试的时候发现`la`一条指令在 GDB 里变成了两条  ，这是为什么？

<details><summary>2 previous messages</summary>

> 让我看看 `la` 在目标文件和反汇编里的实际展开，用真实证据来说明。
>
> <details><summary>Explored a file, ran 3 commands</summary>
>
> - Ran `which riscv64-unknown-elf-objdump riscv64-unknown-elf-gcc riscv64-unknown-elf-ld 2>/dev/null; ls /home/win/oslab/OS-Lab/code/lab1/obj/kern/init/ 2>/dev/null`
>
> <details><summary>Explored a file</summary>
>
> - Read `~/oslab/OS-Lab/code/lab1/obj/kernel.asm`
>
> </details>
>
> - Ran `riscv64-unknown-elf-objdump -dr /home/win/oslab/OS-Lab/code/lab1/obj/kern/init/entry.o`
>
> - Ran `riscv64-unknown-elf-objdump -dr --no-show-raw-insn /home/win/oslab/OS-Lab/code/lab1/obj/kern/init/entry.o 2>/dev/null; echo "=== elf header entry ==="; riscv64-unknown-elf-readelf -h /home/win/oslab/OS-Lab/code/lab1/bin/kernel | grep -i entry`
>
> </details>
</details>

`la` 不是一条真正的 RISC-V 指令，它是一条**汇编伪指令（pseudo-instruction）**，所以"源码一行、GDB 两条"是完全正常的。下面用你 Lab1 里的真实数据说明。

## 1. `la` 为什么要拆成两条

RISC-V 的指令是**定长 32 位**的，一条指令塞不下一个完整地址，也塞不下一个 64 位立即数。所以"把某个符号的地址装进寄存器"这件事，汇编器必须用多条真实指令拼出来。标准做法是**一对 PC 相对的 hi/lo 指令**：

```asm
auipc sp, %pcrel_hi(bootstacktop)   # 取 PC 相对偏移的高 20 位，左移 12 位后加到 PC
addi  sp, sp, %pcrel_lo(...)        # 补上低 12 位
```

- `auipc` 负责偏移的**高位部分**（高 20 位，等效于 `PC + (imm20 << 12)`）；
- `addi` 负责**低 12 位**的有符号立即数。

两条合起来才凑出 `bootstacktop` 的完整地址。这是"拆分"的根本原因——不是工具链啰嗦，而是单条指令的立即数位宽不够。

## 2. 你实验里的具体展开

**链接前**，看目标文件 `entry.o`，可以清楚看到 `la` 被展开并且带重定位信息（因为我这里禁用了原始指令显示，`mv sp,sp` 就是 `addi sp,sp,0` 的别名）：

```text
0:  auipc sp,0x0
        R_RISCV_PCREL_HI20   bootstacktop
0:  mv    sp,sp
        R_RISCV_PCREL_LO12_I .L0
```

- `R_RISCV_PCREL_HI20 bootstacktop` 对应第一条 `auipc`；
- `R_RISCV_PCREL_LO12_I` 对应第二条 `addi`。

**链接后**，重定位被解析，地址变成最终值：

```asm
80200000:  auipc sp,0x3     # sp = 0x80200000 + (0x3<<12) = 0x80203000
80200004:  mv    sp,sp      # 实为 addi sp,sp,0，低 12 位为 0
```

验证一下：`bootstacktop = 0x80203000`（见 `obj/kernel.sym`），`PC = 0x80200000`，两者之差正好是 `0x3000 = 0x3 << 12`。所以高 20 位是 `3`，低 12 位是 `0`。

## 3. 那个奇怪的 `mv sp,sp` 是怎么回事

它不是多余的，也不是错误，而是 **`addi sp, sp, 0` 的显示别名**（`addi rd, rs, 0` 在反汇编里通常显示成 `mv rd, rs`）。

之所以立即数是 `0`，纯粹是因为这个例子里偏移 `0x3000` 的低 12 位恰好为零。换成别的符号或别的地址，第二条就会带上非零的低 12 位修正值。所以 `mv sp,sp` 只是"这次下半段没有修正量"的表现。

## 4. 为什么用 PC 相对，而不是直接写绝对地址

`la` 默认用的是 `%pcrel_hi/%pcrel_lo`，也就是**相对当前 PC 计算**。这样生成的代码是位置无关的：内核将来被链接到哪个地址都能算对。你的编译选项里有 `-mcmodel=medany`，GCC 正是选择这种 PC 相对形式。如果用绝对地址（`lui`/`addi`），代码就会和固定的链接地址绑死。

## 5. 一个有用的对照：`tail kern_init` 也被展开了，但结果不同

紧接着的 `tail kern_init` 同样是伪指令，链接前也是两条：

```text
8:  auipc t1,0x0
        R_RISCV_CALL_PLT  kern_init
c:  jr    t1
```

但**链接之后它变成了一条**：

```asm
80200008:  j  8020000a <kern_init>
```

这是因为内核入口离 `kern_init` 很近，链接器的**松弛（relaxation）**把它优化成了一条 `j`。而 `la` 的两条形式保留了下来——因为它要把一个地址物化进寄存器，标准就要求这样两条，不能简单合并。这也提醒你：伪装指令展开成几条、最终会不会被优化，取决于工具链和地址布局，**不要用源码的行数去数机器指令数**。

## 6. 调试时的实际影响

GDB 反汇编的是**链接后的机器码**，重定位已经解析完，所以你看到的自然是两条真实指令、带最终地址。

- 用 `stepi`（机器指令级单步）走 `la sp, bootstacktop` 这一行，会**停两次**（先 `auipc` 后 `addi`）；
- 用 `next`/`step`（源码行级）则把它当**一行**处理。

想看清楚重定位的原始形式，可以直接查目标文件：

```bash
riscv64-unknown-elf-objdump -dr obj/kern/init/entry.o
```

一句话总结：**`la` 是伪指令，RISC-V 单条 32 位指令装不下完整地址，所以汇编器把它展开成 `auipc`（高 20 位）+ `addi`（低 12 位）两条 PC 相对指令；你看到的 `mv sp,sp` 是这条例子里低 12 位恰好为 0 的 `addi sp,sp,0`。**

> 总结一下gdb调试的基本命令有哪些以及它们的功能

<details><summary>2 previous messages</summary>

> 我先看看你实验里实际用到的 GDB 命令和脚本，这样总结能贴合 Lab1 的场景。
>
> <details><summary>Explored a file, a search, ran a command</summary>
>
> <details><summary>Explored a search</summary>
>
> - Searched for `gdb\|GDB\|stepi\|break\|x/\|info reg\|layout\|target remote` in `report.md`
>
> </details>
>
> - Ran `find /home/win/oslab/OS-Lab -name "*.gdb" -o -name ".gdbinit" -o -name "*.gdb.in" 2>/dev/null | head; echo "--- tools ---"; ls -la /home/win/oslab/OS-Lab/code/lab1/tools/`
>
> <details><summary>Explored 2 files</summary>
>
> - Read `./report.md`
> - Read `./report.md`
>
> </details>
>
> </details>
</details>

下面按"你在 Lab1 里会怎么用"的顺序整理一份 GDB 命令速查表。核心思路是：**GDB 本身不运行你的内核，它只是通过 QEMU 的调试端口，去观察和控制那颗虚拟的 RISC-V CPU。**

## 0. 先搞清两条进入方式

你仓库里 [Makefile](~/oslab/OS-Lab/code/lab1/Makefile) 的 `make gdb` 已经把最常用的三条启动命令封装好了：

```makefile
gdb:
	riscv64-unknown-elf-gdb \
    -ex 'file bin/kernel' \
    -ex 'set arch riscv:rv64' \
    -ex 'target remote localhost:1234'
```

所以通常是：**一个终端 `make debug`（启动 QEMU 并暂停），另一个终端 `make gdb`（连上去）**。

| 命令 | 功能 |
|---|---|
| `file bin/kernel` | 加载 ELF 符号表，之后才能用 `kern_entry` 这类符号名 |
| `set arch riscv:rv64` | 指定目标架构，保证反汇编正确 |
| `target remote localhost:1234` | 通过 QEMU 的 gdbserver 连上目标，建立控制 |
| `disconnect` / `quit` | 断开连接 / 退出 GDB（不会杀掉 QEMU） |

## 1. 查看状态（最常用）

| 命令 | 简写 | 功能 | Lab1 用途 |
|---|---|---|---|
| `info registers` | `info reg` | 显示所有寄存器 | 看 `pc`、`sp`、`ra` 等 |
| `info registers pc sp` | `info reg pc sp` | 只看指定寄存器 | 快速确认 PC、栈指针 |
| `print/x $pc` | `p/x $pc` | 以十六进制打印 PC | 看到 `0x1000` / `0x80000000` / `0x80200000` |
| `print/x $sp` | `p/x $sp` | 打印栈指针 | 验证 `la sp, bootstacktop` 前后变化 |
| `print/x &bootstacktop` | `p/x &bootstacktop` | 打印符号的地址 | 确认新 `sp == 0x80203000` |
| `print/x kern_entry` | — | 打印符号的值 | 确认入口地址 |
| `x/i $pc` | — | 反汇编 PC 处的 1 条指令 | 看当前在跑哪条指令 |
| `x/10i $pc` | — | 反汇编 PC 开始的 10 条 | 看 `kern_entry` 附近代码 |
| `x/8gx $sp` | — | 以 8 字节为单位看内存 | 观察栈内容 |
| `x/8xw 0x80203000` | — | 按 4 字节十六进制看内存 | 看内存/数据 |
| `info symbol 0x80200000` | — | 反查地址属于哪个符号 | 判断 PC 落在哪 |
| `backtrace` | `bt` | 显示调用栈 | 进入 C 后看调用链 |
| `info frame` | — | 显示当前栈帧信息 | 确认 `ra`、`sp` 关系 |

> 记忆点：`$` 开头是寄存器，`&` 取符号地址，`x/` 是"查看内存/指令"。

## 2. 下断点

| 命令 | 简写 | 功能 |
|---|---|---|
| `break kern_entry` | `b kern_entry` | 在符号处下断点 |
| `break *0x80200000` | `b *0x80200000` | 在绝对地址下断点 |
| `break kern_init` | `b kern_init` | 在内核 C 函数入口下断点 |
| `info breakpoints` | `info b` | 列出所有断点 |
| `delete` / `delete 1` | `d` | 删除全部 / 删除 1 号断点 |
| `disable` / `enable` | — | 暂时禁用 / 重新启用断点 |
| `tbreak kern_entry` | — | 临时断点，命中一次后自动删除 |
| `watch var` | — | 变量值变化时停（数据断点） |
| `hbreak *0x80200000` | — | 硬件断点，适用于早期启动阶段 |

**本实验最关键的一条**：

```gdb
b kern_entry
c
```

`c`（continue）让暂停在复位入口的 CPU 继续跑，经过 OpenSBI 后自动停在 `0x80200000`。

## 3. 单步执行

这是 Lab1 里最需要区分的两个层次：

| 命令 | 简写 | 粒度 | 功能 |
|---|---|---|---|
| `stepi` | `si` | **机器指令** | 执行一条 machine code 就停 |
| `nexti` | `ni` | **机器指令** | 同上，但越过子调用 |
| `step` | `s` | **源码行** | 进入被调用函数 |
| `next` | `n` | **源码行** | 越过被调用函数 |
| `finish` | — | 函数级 | 执行到当前函数返回 |
| `until` | `u` | 行/地址 | 跑到指定位置 |
| `continue` | `c` | — | 继续运行到下一个断点 |

注意你在上一个问题里遇到的情况：`la sp, bootstacktop` 是一条源码，但它是**两条机器指令**。所以：

- `si` 会停两次（先 `auipc`，后 `addi`）；
- `n` 会把它当一行跳过。

另外，**CPU 复位后停在 `0x1000` 时没有源码**（那是 QEMU 的复位代码，不是你的内核），这时候只能用 `si` / `x/i $pc` 这种指令级操作，不能用 `step`/`next`。

## 4. 反汇编与界面

| 命令 | 功能 |
|---|---|
| `disassemble` | 反汇编当前函数 |
| `disassemble kern_entry` | 反汇编指定函数 |
| `layout asm` | 打开汇编窗口（TUI） |
| `layout regs` | 打开寄存器窗口 |
| `layout src` / `layout split` | 源码 / 源码+汇编 |
| `tui disable` | 关闭 TUI |
| `list` | 显示源码 |
| `set disassemble-next-line on` | 单步时自动显示下一条汇编 |
| `info line *0x80200000` | 查地址对应的源码行 |

## 5. 控制执行流与其他

| 命令 | 功能 |
|---|---|
| `Ctrl-C` | 中断正在运行的 CPU，让它停下来 |
| `kill` | 终止被调试程序（这里是让 QEMU 停止目标） |
| `monitor info registers` | 直接把命令发给 QEMU monitor |
| `monitor quit` | 通过 monitor 退出 QEMU |
| `set $pc = 0x80200000` | 直接修改 PC（跳转） |
| `jump *0x80200000` | 跳到指定地址继续 |
| `set pagination off` | 关闭分页，方便脚本输出 |
| `help <命令>` | 查看某命令帮助 |
| `-ex '<命令>'` | 启动 GDB 时直接执行一条命令（`make gdb` 就是这么做的） |

## 6. 本实验的一条完整调试主线

把上面命令串起来，就是你 Lab1 报告里那条验证链：

```gdb
# 终端1: make debug      （QEMU 暂停在复位状态）
# 终端2: make gdb        （自动 file + set arch + target remote）

p/x $pc                  # 0x1000，复位入口
x/i $pc                  # 看复位处的指令
si                       # 指令级单步，穿过 QEMU 早期启动代码

b kern_entry             # 在内核入口下断点
c                        # 继续，经过 OpenSBI 后命中

p/x $pc                  # 0x80200000 <kern_entry>
p/x $sp                  # 启动环境留下的栈指针，比如 0x8001bd80
x/i $pc                  # auipc sp,0x3
si                       # 执行第一条
si                       # 执行第二条(addi/mv)
p/x $sp                  # 0x80203000
p/x &bootstacktop        # 也是 0x80203000，证明栈已切到内核自己的栈

si                       # 执行 tail 编译出的一条 j
p/x $pc                  # 0x8020000a <kern_init>，进入 C
bt                       # 可选：看调用栈
```

## 7. 几个容易踩的坑

- 不加载 `bin/kernel` 就没有符号，`b kern_entry` 会失败——这也是为什么 `make gdb` 里要先 `file`。
- `make debug` 的 `-S` 会让 QEMU 一开始就冻住，不按 `c`/`si` 它不会动。
- `p $pc` 少了 `$` 会被当成符号名；看十六进制要 `p/x`。
- 复位阶段（0x1000）和 OpenSBI（0x80000000）**没有你的源码和符号**，只能用 `x/i`、`si` 这类指令级命令观察。
- GDB 反汇编的是**链接后的机器码**，所以看到的是伪指令展开后的真实指令（比如 `la` → `auipc`+`addi`），这和你上一条问题正好呼应。
- `si` 和 `n` 的区别在启动阶段特别重要：没有源码时用 `si`，进了 `kern_init` 有源码后再用 `n` 更省事。

需要的话，我可以把这份速查表整理成一个「GDB 基本命令」小节或附录补进 [report.md](./report.md)，放在练习2或第五部分前面。

> 我在学习init.c的时候发现`kern_init()` 里面这一段  extern char edata[], end[]; memset(edata, 0, end - edata); 这个edata[], end[]是哪里来的数组啊？

<details><summary>2 previous messages</summary>

> 让我用你仓库里的实际符号表确认一下这两个名字的来源和地址。
>
> <details><summary>Explored a search, ran a command</summary>
>
> - Ran `riscv64-unknown-elf-nm /home/win/oslab/OS-Lab/code/lab1/bin/kernel | grep -Ei 'edata| end$|etext|bootstack'; echo "--- sym file ---"; grep -Ei 'edata|end|etext' /home/win/oslab/OS-Lab/code/lab1/obj/kernel.sym`
>
> <details><summary>Explored a search</summary>
>
> - Searched for `PROVIDE\|\.bss\|\.sdata\|\.data\|\.text\|\.rodata\|ALIGN` in `kernel.ld`
>
> </details>
>
> </details>
</details>

好问题——这正是 Lab1 里最容易让人找半天的那种"名字凭空冒出来"的情况。**`edata[]` 和 `end[]` 不是任何 C 文件里定义的数组，它们是链接脚本 [kernel.ld](~/oslab/OS-Lab/code/lab1/tools/kernel.ld) 里定义的两个符号。**

## 1. 它们真正的"定义"在 kernel.ld 里

在 [kernel.ld](~/oslab/OS-Lab/code/lab1/tools/kernel.ld:38) 中：

```ld
    PROVIDE(edata = .);        /* 第 38 行：.data/.sdata 结束处 */

    .bss : {
        *(.bss)
        *(.bss.*)
        *(.sbss*)
    }

    PROVIDE(end = .);          /* 第 46 行：所有数据结束处 */
```

`PROVIDE(名字 = .)` 的意思是：把链接器当前的**位置计数器 `.`（也就是此刻的地址）**导出成一个叫 `edata`（或 `end`）的符号。作用相当于"在这里插一个地址标签"。所以：

- `edata` = 初始化数据段（`.data` + `.sdata`）的结束地址；
- `end` = `.bss` 结束后的地址；
- 两者之间夹着的正好就是 `.bss` 段。

`etext`（第 18 行）也是同样的手法，只不过标的是 `.text` 的结束。

## 2. 为什么声明成 `extern char edata[], end[]`

关键：**我们只关心这两个符号的地址，不关心它们的内容。** 写成"数组"是一个利用 C 语言语法的声明技巧：

- `char` 类型大小是 1 字节，所以两个 `char` 指针相减，结果正好是**字节数**，可以直接当 `memset` 的长度；
- 数组名在表达式里会**自动退化成指针**，所以 `edata` 这个表达式的值就是符号的地址，等价于 `&edata`；
- 因此 `end - edata` 就是 `.bss` 的字节数。

对比一下几种写法，就能看出为什么必须是这个形式：

| 声明 | `edata` 表达式的含义 | 结果 |
|---|---|---|
| `extern char edata[];` | 符号的**地址** | ✅ `memset(edata, ...)` 正确 |
| `extern char edata;` | 符号处的**内容** | ❌ 语义不对，得写 `&edata` |
| `extern char *edata;` | 把符号处的内容**当指针读** | ❌ 完全是另一回事 |

`extern` 这个词的意思是"这个东西的定义不在本文件里，链接时别处会提供"——这里"别处"就是链接脚本。

## 3. 这段代码在干什么

```c
extern char edata[], end[];
memset(edata, 0, end - edata);
```

翻译成人话就是：**把从 `edata` 到 `end` 之间的整段内存（也就是 `.bss`）全部清零。**

`.bss` 段存放的是"未初始化的全局/静态变量"。按照 C 语言语义，它们应该初值为 0。内核必须自己保证这一点。

## 4. 为什么 `.bss` 需要内核自己清零

因为在 `Makefile` 里：

```makefile
$(OBJCOPY) $(kernel) --strip-all -O binary $@
```

`objcopy -O binary` 生成的 `ucore.img` **只包含有实际内容的段**。`.bss` 是 `NOBITS` 类型——它只占地址空间，在文件里不占字节。也就是说，镜像文件里根本没有 `.bss` 的"零"，那段地址在内核启动时的内容是不可信的（在真实硬件上可能是上电残留的垃圾值）。所以必须由 `kern_init()` 开头主动清零。

## 5. 本实验里一个值得注意的细节

用 `nm` 看你实际生成的 `bin/kernel`：

```text
0000000080201000 D bootstack
0000000080203000 D bootstacktop
0000000080203008 D edata
0000000080203008 D end
```

在 Lab1 这一版里，**`edata` 和 `end` 恰好相等，都是 `0x80203008`**。这说明当前内核的 `.bss` 大小是 0——因为还没有任何未初始化的全局变量。所以这条 `memset` 现在实际清的是 0 个字节，是个"暂时没生效但语义正确"的准备动作。等后面实验加入了未初始化全局变量，它就会真正开始工作。

内存布局大致是这样：

```text
0x80200000  .text      kern_entry、kern_init、cprintf …   (etext 在 .text 末尾)
            .rodata    "(THU.CST) os is loading ..." 等只读数据
0x80201000  .data      bootstack（8KB 内核栈）
0x80203000  .sdata     SBI_CONSOLE_PUTCHAR 等小数据
0x80203008  edata  ←── .data/.sdata 到此结束
            .bss       （当前为空，所以下一行地址相同）
0x80203008  end    ←── 所有数据到此结束
```

## 6. 怎么自己验证

```bash
riscv64-unknown-elf-nm bin/kernel | grep -E 'edata|end'
riscv64-unknown-elf-objdump -t bin/kernel | grep -E 'edata|end'
```

或者在 GDB 里：

```gdb
p/x &edata
p/x &end
p/x (long)(end - edata)     # 就是 .bss 的字节数
p/x &etext                  # 顺带看 .text 的结束地址
```

如果哪天把链接脚本里那两行 `PROVIDE` 删掉，链接就会报 `undefined reference to 'edata'`——这能反过来证明它们的定义确实来自链接脚本，而不是某个 `.c` 文件。

一句话总结：**`edata`/`end` 是链接脚本导出的"地址标签"，不是真正的数组；`extern char edata[]` 只是借助数组名会退化成地址这一点，方便地拿到链接器算出的地址，`end - edata` 就是 `.bss` 的长度，`memset` 负责把这段清零。**

> 做 GDB 实验的时候我还有一个问题。`make debug` 和 `make gdb` 是两个终端，那到底是 GDB 在执行程序还是 QEMU 在执行？

这是个非常关键的疑问，很多人第一次用 QEMU + GDB 都会绕进去。直接给结论：**真正执行内核的是 QEMU，不是 GDB。GDB 只是通过一个网络调试接口去"遥控"和"观察"QEMU 里那颗虚拟 CPU。**

## 两个终端里到底跑着什么

```text
终端 1: make debug
  └── qemu-system-riscv64 进程        ← 真正的"机器 + 执行引擎"
        ├── 模拟一颗 RISC-V CPU
        ├── 模拟内存、串口等设备
        ├── 实际执行你的 ucore 机器码
        └── 在 localhost:1234 上开了一个 gdb server（调试接口）

终端 2: make gdb
  └── riscv64-unknown-elf-gdb 进程    ← 调试客户端
        ├── 读入 bin/kernel（只为拿符号/地址，不负责执行）
        └── 通过 TCP 连到 1234，发命令、收状态
```

关键点：**uCore 这个"程序"从来没有在 GDB 里运行过**。它一直活在 QEMU 模拟出来的那台虚拟机器里。GDB 手里只有一份 `bin/kernel` 的符号表，用来把地址翻译成名字（比如把 `0x80200000` 显示成 `<kern_entry>`）。

## `make debug` 里的 `-s -S` 是干什么的

看你 [Makefile](~/oslab/OS-Lab/code/lab1/Makefile:171) 的 debug 规则：

```makefile
debug: $(UCOREIMG) ...
	$(QEMU) -machine virt -nographic -bios default \
		-device loader,file=$(UCOREIMG),addr=0x80200000 \
		-s -S
```

- `-s`：等价于 `-gdb tcp::1234`，让 QEMU 在 1234 端口开一个 **gdbstub**（GDB 远程调试服务端）；
- `-S`：开机后**先不执行**，冻在复位状态，等 GDB 来了再说。

所以 `make debug` 之后 QEMU 是"通着电但没跑"的状态，必须等你 `make gdb` 连上，敲 `continue` 或 `stepi` 它才会动。

## 你敲 `si` 的时候发生了什么

```text
你在 GDB 里输入:  si
        │
        ▼
GDB 把请求打包，通过 TCP 发给 localhost:1234
        │   （"执行一条机器指令"）
        ▼
QEMU 的 gdbstub 收到请求
        ▼
QEMU 里的虚拟 RISC-V CPU 执行一条指令
        ▼
QEMU 停住，把 PC、sp 等寄存器状态回传给 GDB
        ▼
GDB 把结果打印给你看
```

**每一次执行都发生在 QEMU 里，GDB 只是发指令和收结果的外壳。** 你敲的 `continue`、`stepi`、`break`、`x/i`，本质都是"请求"，由 QEMU 侧负责真正实现。

## 为什么你会觉得是 GDB 在执行

因为平时在 Linux 上本地调试是另一个模型：

| | 本地调试 `gdb ./a.out` | 本实验的远程调试 |
|---|---|---|
| GDB 做了什么 | 用 `ptrace` **亲手启动并控制**子进程 | 只连一个 socket，**不启动任何被调试程序** |
| 被调试程序跑在哪 | 同一个宿主机的 CPU 上 | QEMU 模拟的 RISC-V CPU 上 |
| 谁"执行" | 宿主机 CPU 直接执行程序 | QEMU 解释/翻译 RISC-V 指令来执行 |
| `file bin/kernel` 的含义 | 加载可执行文件 | **只加载符号**，不加载执行 |

本实验属于**远程调试**：GDB 连的是 QEMU 提供的 gdbstub，`target remote localhost:1234` 这句就是"远程"二字的来源。所以 GDB 在此完全是个**客户端/前端**。

再往底层说一句：真正被宿主机 x86 CPU 执行的，是 `qemu-system-riscv64` 这个普通 Linux 进程的代码。QEMU 用动态二进制翻译（TCG）把 RISC-V 指令翻译成宿主机指令来跑，于是你的内核"看起来"在 RISC-V 上执行。GDB 完全不在这条执行路径上。

## 一个能自己验证的小实验

1. `make debug`，然后 `make gdb`，敲 `c` 让 QEMU 跑起来；
2. 在 GDB 里敲 `detach` 或直接关掉 GDB；
3. 你会发现 **QEMU 依然在跑，串口还会继续打印**——因为执行内核的本体是 QEMU，GDB 走了它照样活；
4. 反过来，把 QEMU 那个终端关掉，GDB 立刻报连接断开，因为它监控的对象没了。

这个对比最能说明谁才是真正的执行者。

## 一句话总结

**QEMU 是执行者，GDB 是观察者/遥控器。** `make debug` 让 QEMU 把虚拟机器跑起来、开好调试接口并暂停；`make gdb` 让 GDB 连上去发号施令。内核的每一条指令都是 QEMU 那颗虚拟 RISC-V CPU 执行的，GDB 只是在一旁读寄存器、下断点、看内存。

> 那为啥要分两个终端同时运行啊？

核心原因很简单：**QEMU 和 GDB 是两个互相独立的进程，而且都是"交互式"程序——各自都要占用一个键盘和一块屏幕。** 一个终端给"虚拟机器"，一个终端给"调试器"，它们才能各说各话。

## 两个终端分别在扮演什么角色

```text
终端 1: make debug
  ┌────────────────────────────────┐
  │ qemu-system-riscv64（前台运行） │
  │  · 这是"虚拟机器"的屏幕         │
  │  · -nographic 把客户机串口接到  │
  │    这个终端的 stdin/stdout      │
  │  · 内核打印的 "(THU.CST) ..."   │
  │    就出现在这里                 │
  │  · 会一直阻塞，不会回到 shell   │
  └────────────────────────────────┘

终端 2: make gdb
  ┌────────────────────────────────┐
  │ (gdb) 提示符                    │
  │  · 这是"调试器"的键盘           │
  │  · 你在这里敲 b / c / si / x    │
  │  · 退出 GDB 后会回到 shell      │
  └────────────────────────────────┘
```

一个负责**执行和显示客户机输出**，一个负责**发调试命令**，分工清楚。

## 为什么不能挤在一个终端

有三个现实原因：

1. **QEMU 是前台阻塞进程。** `make debug` 里直接调用 `qemu-system-riscv64`，它会一直占着终端不返回。这个终端里没有 shell 可以再敲 `make gdb`。
2. **`-nographic` 把客户机串口接到了当前终端的输入输出。** 也就是说这个终端的键盘属于"虚拟机"。内核启动信息、将来你在客户机里敲的字符，都走这里。
3. **GDB 也需要 stdin/stdout 来显示 `(gdb)` 提示符和接收你的命令。** 如果两者共用一个终端，就等于两台设备抢同一个键盘，输入会打架，输出也会混在一起看不清。

所以 Lab 的 Makefile 干脆把它们拆开：**一个终端当"机器的控制台"，一个终端当"调试器的控制台"。**

## 还有启动顺序的要求

这两个终端不是随便谁先谁后：

```text
终端 1: make debug   → QEMU 启动，在 1234 端口监听，CPU 冻结(-S)
        （必须先起来，因为它是"服务端"）
              ↓
终端 2: make gdb     → GDB 作为客户端，connect 到 localhost:1234
```

如果反了，先跑 `make gdb`，会立刻报 `Connection refused`——因为 1234 端口上还没人在听。这也说明了各自的角色：**QEMU 是服务端（被调试目标），GDB 是客户端（调试器）。**

## 其实不一定非得是两个"窗口"

课程要求两个终端只是为了清晰。你完全可以：

- **用 `tmux` 或 `screen` 分屏**：左边跑 `make debug`，右边跑 `make gdb`，本质上还是两个终端，只是并排显示，看起来最舒服；
- **把 QEMU 放到后台**（`make debug &`），在前台跑 GDB——但这样客户机的串口输出会混进 GDB 的界面，不好读，通常要把 QEMU 输出重定向到文件；
- **在 QEMU 里切换监视器**：`-nographic` 下按 `Ctrl-A` 然后 `c`，可以在客户机串口和 QEMU monitor 之间切换，但这是 QEMU 自己的功能，和 GDB 无关。

Lab1 场景下，最省心的还是两个终端（或 tmux 分屏）：**一个看机器在干什么，一个敲调试命令。**

## 一句话总结

因为 QEMU 是**执行内核的虚拟机**、GDB 是**遥控它的调试器**，两者是独立进程且都要用键盘屏幕，所以拆成两个终端：**终端 1 跑机器（QEMU，前台阻塞、显示串口输出），终端 2 跑调试器（GDB，接受你的命令），并且必须先开 QEMU 再连 GDB。**


# 2号prompt汇总 2412419张祎然

## 一、任务范围与材料要求

本记录依据实验中的交流内容按主题整理，对重复提问和操作反馈作合并表述，保留关键问题、分析依据及修正过程。第一节讨论任务与材料要求，第二节讨论 QEMU 4.1.1 环境中的调试问题。

### 1. 明确分工与实验范围

**学生：** 本人负责 2 号 SBI 输出模块，同时需要完成未单独分配的共同实验内容。报告中如何区分个人模块与共同练习？调试应按什么顺序展开？

**AI：** 个人模块应重点分析格式化输出、控制台接口与 SBI 封装的调用关系，并验证参数传递、异常进入和返回过程。共同练习包括环境检查、编译运行、入口汇编分析及启动流程调试。可先核对代码与工具配置，再验证启动过程，最后跟踪输出函数，使各部分都有对应的源码分析和运行依据。

### 2. 区分 SBI、GDB 与内核 ELF 的作用

**学生：** SBI、GDB 和内核 ELF 都出现在实验流程中，但三者的作用似乎不属于同一层次。它们分别解决什么问题？OpenSBI 与 SBI 又是什么关系？

**AI：** SBI 是内核向执行环境请求服务的接口规范，OpenSBI 是其固件实现。GDB 是调试工具，通过连接 QEMU 设置断点、检查寄存器并控制程序执行。ELF 是文件格式；本次内核 ELF 包含程序结构、符号及调试信息，供 GDB 建立机器地址与源码的对应关系。用于启动加载的原始镜像则由 ELF 转换得到。

**学生：** 因此，SBI 描述运行时的服务接口，GDB 用于观察和控制执行，ELF 用于组织程序及调试信息。报告中还需区分 GDB 加载的 `bin/kernel` 与 QEMU 加载的 `bin/ucore.img`，避免将调试对象和启动镜像混为一谈。

### 3. 区分实验验证与提交准备

**学生：** 如何根据已有运行结果判断实验完成范围？启动消息输出正常，是否足以说明共同练习、个人模块和提交要求均已完成？

**AI：** 需要分别检查。现有结果覆盖编译启动、两个共同练习，以及 SBI 输出模块的参数检查、异常进入与返回、最终字符计数。材料整理阶段还需完成报告、提示词记录、截图整理及小组汇总稿。当前课程代码缺少评分脚本，因此启动成功不能作为官方自动评分或隐藏用例通过的依据。

**学生：** 实验功能验证与提交材料准备应分别说明。该阶段已完成的运行和调试结果可写入验证部分，尚待整理的材料单独列出；自动评分未执行，不将其列为已通过项目。



## 二、QEMU 4.1.1 环境中的调试讨论（2026-10-03）

### 1. QEMU 版本与课程启动参数

**学生：** 小组已明确使用 QEMU 4.1.1。在可能存在隐藏测试的情况下，是否应同时保留课程原始 Makefile，而不仅是保证内核能够启动？

**AI：** 应使用小组指定版本，并核对课程原始启动配置，使验证环境与提交配置一致。启动成功只能证明已检查的执行路径可以运行，隐藏测试是否通过仍需依据相应测试结果判断。

**学生：** 本次将 QEMU 4.1.1 独立安装于 `~/lab1-tools/qemu-4.1.1-install`，内核 C 与汇编源码沿用课程实现，启动参数也按课程原始 Makefile 核对。报告据实际运行结果说明完成范围，不推定隐藏测试已经通过。

### 2. QEMU 源码包的获取与安装

**学生：** WSL 中下载 QEMU 源码包长时间未取得数据。是否可以通过 Windows 浏览器下载，再复制至 WSL 编译？采用这种方式需要检查哪些内容？

**AI：** 可以通过 Windows 浏览器获取官方 `qemu-4.1.1.tar.xz`，再从 `/mnt/d/...` 复制至 WSL。该方式改变的是下载途径，后续构建仍在 WSL 中进行。复制后先检查压缩包完整性，再解压并配置 `riscv64-softmmu` 目标和独立安装目录。

**学生：** 因此，源码包获取与编译环境可以分开处理。本次将压缩包置于 `~/lab1-tools/`，依次执行 `xz -t`、`tar -xf`、配置命令、`make -j4` 和 `make install`。完整配置参数保留在报告 4.1 节。

### 3. 安装版本与实际运行版本的核对

**学生：** 独立安装路径下的版本命令已输出 `QEMU emulator version 4.1.1`。这是否能直接证明 `make qemu` 使用的也是该版本？

**AI：** 该结果只能确认指定路径下的程序版本。实际运行前，还需将其 `bin` 目录置于当前终端 PATH 首位，执行 `hash -r`，再通过 `qemu-system-riscv64 --version` 检查默认命令。新终端也应重新设置并核对，避免调用其他已安装版本。

**学生：** 设置 PATH 后，默认版本命令同样显示 4.1.1，随后在本地 `code/` 目录执行 `make` 和 `make qemu`。安装检查与实际运行检查分别验证了指定程序的版本，以及当前命令是否调用该程序。

对应截图：[安装版本](./images/qemu411-install-version.png)、[实际运行版本与输出](./images/boot-success-qemu411.png)。

### 4. 固件与内核镜像的加载配置

**学生：** 核对 Makefile 时，如何区分默认固件与自行构建的内核镜像？工作目录和待上传目录中的哪些配置需要保持一致？

**AI：** `-bios default` 选择默认固件，`-device loader,file=$(UCOREIMG),addr=0x80200000` 将内核镜像加载到指定地址，二者不是同一文件。`qemu` 和 `debug` 目标的相关参数均应与课程原始 Makefile 一致，工作目录与待上传目录也应同步核对。

**学生：** 两个目录中的 Makefile 已与课程原版核对一致。运行终端显示 QEMU 4.1.1、OpenSBI v0.4、Runtime SBI Version 0.1，以及 `(THU.CST) os is loading ...`。但本次 `make` 提示已有构建产物可用，是否应将该截图描述为构建检查和运行验证，而非重新编译全部源码？

**AI：** 应按该次命令的实际行为描述。`make` 未重新编译全部源文件，后续运行使用已有内核镜像；内核启动消息可以作为执行到输出阶段的依据，但不能将该截图作为完整重新编译过程的记录。

### 5. 区分内核循环与调试暂停

**学生：** 正常启动后，终端持续停留在输出界面；重新启动 `make debug` 后又没有立即输出。如何区分内核进入无限循环与 QEMU 因调试参数暂停这两种状态？

**AI：** 正常启动后，当前内核执行 `while (1)`，不提供交互式 shell。调试启动则因 `-S` 暂停 CPU，需要连接 GDB 后继续执行。若原终端退出操作未生效，可结束该终端并重新建立调试会话；新终端需再次核对 QEMU 4.1.1 的运行路径。

**学生：** 新终端版本检查为 4.1.1。执行 `make debug` 后，另一个终端通过 `make gdb` 连接 `localhost:1234`，读取到 `pc=0x1000`。这一结果表明目标停在复位位置，而不是已经执行至内核中的循环。

**AI：** 该判断与当前 PC 一致。QEMU 终端用于运行模拟机器和显示输出，GDB 终端用于控制执行及检查状态。Bash 命令与 GDB 命令应分别在相应提示符下执行，不能仅依据是否出现新输出判断程序运行阶段。

对应截图：[GDB 连接和复位指令](./images/gdb-reset-qemu411.png)。

### 6. 依据复位指令确定单步次数

**学生：** `x/8i $pc` 显示跳转指令位于 `0x1010`，之前依次为 `auipc`、`addi a1,t0,32`、`csrr a0,mhartid` 和 `ld t0,24(t0)`。若要检查跳转参数，应在何处暂停？后续显示的 `unimp` 是否表示启动出现异常？

**AI：** 本次复位启动序列包含五条指令。执行 `si 4` 后停在尚未执行的 `jr t0`，可检查跳转目标及参数；再执行一次 `si` 观察是否进入固件。后续 `unimp` 来自对填充和地址数据的反汇编，不能据此判断 CPU 已执行非法指令。

**学生：** 跳转前测得 `pc=0x1010`、`t0=0x80000000`、`a0=0`、`a1=0x1020`，单步后 `pc=0x80000000`。这些结果是否分别对应固件入口、硬件线程编号和设备树地址？

**AI：** `t0` 保存固件跳转目标，`a0` 表示 hart 0，`a1` 保存设备树地址；跳转后的 PC 验证了控制权已进入 OpenSBI。因此，单步次数应由本次指令序列确定，而不是套用其他版本的启动步骤。

对应截图：[复位代码](./images/gdb-reset-qemu411.png)、[进入 OpenSBI](./images/gdb-opensbi-qemu411.png)。

### 7. 内核入口与栈初始化的区别

**学生：** `break *kern_entry` 命中 `0x80200000` 时，`sp=0x8001bd80`，而 `&bootstacktop=0x80203000`。既然已经进入内核，栈指针为何尚未指向内核栈顶？

**AI：** 入口断点位于第一条指令执行之前，此时 `sp` 仍保留固件阶段的值。需要执行 `la sp, bootstacktop` 展开的两条机器指令，再检查 `sp` 与栈顶地址是否一致，随后执行跳转进入 C 初始化函数。

**学生：** 执行 `si 2` 后，`sp=0x80203000`，与 `bootstacktop` 一致；再次单步后进入 `kern_init`，`pc=0x8020000a`。此外，GDB 在栈指针地址后显示 `<SBI_CONSOLE_PUTCHAR>`，这一符号标注是否意味着栈与服务编号变量发生冲突？

**AI：** 根据报告中的符号表检查，栈顶标签与该变量恰好同址，但栈空间为 `[0x80201000, 0x80203000)`，不包含上界，并向低地址增长，因此不存在存储范围重叠。符号标注只说明地址对应关系，不表示 `sp` 保存服务编号。栈初始化是否完成，应以执行时刻及地址数值为依据。

对应截图：[内核入口](./images/gdb-entry-qemu411.png)、[栈设置和 C 入口](./images/gdb-stack-qemu411.png)。

### 8. 字符串参数与单字符参数的区别

**学生：** 在 `cprintf` 入口，`a0=0x802004c8`、`a1=0x802004a8`，`x/s` 分别得到 `%s\n\n` 和 `(THU.CST) os is loading ...\n`；到 `sbi_console_putchar` 入口后，`a0=0x28`，`p/c` 显示 `40 '('`。同一寄存器在两个位置的值为何不同？

**AI：** 两个函数的参数含义不同。`cprintf` 接收格式串及消息字符串地址，格式化层逐字符处理后，`sbi_console_putchar` 接收的是单个字符值。`0x28` 对应首字符左括号，与消息内容一致。检查参数时应停在相应函数的机器入口，避免后续指令复用参数寄存器。

**学生：** 因此，参数含义应结合当前函数确定，不能将寄存器数值的变化直接视为传参错误。本次 `sbi_call` 被内联，部分变量显示 `<optimized out>`，但仍能通过入口寄存器和反汇编检查实际值；反汇编同时确定了 `ecall` 位于 `0x8020046c`。

对应截图：[格式串与消息](./images/gdb-cprintf-qemu411.png)、[首字符与反汇编](./images/gdb-sbi-putchar-qemu411.png)。

### 9. ecall 前的服务编号与参数检查

**学生：** 进入 `sbi_console_putchar` 时已确认首字符正确，为何还需要在 `0x8020046c` 的 `ecall` 前设置临时断点？这两处检查是否重复？

**AI：** 两处检查对应不同传参阶段。函数入口用于确认 C 层参数，`ecall` 前用于确认封装已按 SBI 约定设置寄存器。本次调用前 `pc=0x8020046c`、`a7=1`、`a0=0x28`、`a1=a2=0`，分别确认了指令位置、旧版字符输出服务编号、字符值及未使用参数。

**学生：** 两次检查分别验证参数进入封装前后的状态。本代码采用旧版 SBI Console Putchar 约定，`a7=1` 指定字符输出服务，`a0` 保存字符值；`ecall` 发起环境调用，具体服务由寄存器参数确定。

对应截图：[ecall 前参数](./images/gdb-before-ecall-qemu411.png)。

### 10. 单步停点与异常入口的差异

**学生：** 单步执行 `ecall` 后，测得 `pc=0x80000474`、`mtvec=0x80000470`、`mepc=0x8020046c`、`mcause=9`。如果 `mtvec` 指定异常入口，当前 PC 为何比它高 4 字节？

**AI：** `mcause=9` 与 `mepc` 的值符合来自 S-mode 的环境调用。需要进一步检查异常入口指令，而不是预先认定单步停止时必然有 `pc=mtvec`。`mtvec` 表示入口，PC 表示实际停止位置，二者是否相同取决于停止前已执行的指令。

**学生：** `x/4i $mtvec` 显示，`0x80000470` 处为 `csrrw tp,mscratch,tp`，当前停点位于下一条指令 `0x80000474`。这一现象是否需要结合 QEMU 4.1.1 的单步处理机制解释，而不能解释为异常向量编号的偏移？

**AI：** 根据本次对 QEMU 4.1.1 异常分派和单步处理源码的核对，处理 `ecall` 异常后，入口第一条指令执行完毕才返回单步停止状态。因此，异常入口仍为 `0x80000470`，实际停点为 `0x80000474`。报告分别保留寄存器实测结果与源码分析依据，不将这 4 字节差值归因于异常向量编号。

对应截图：[异常寄存器](./images/gdb-trap-qemu411.png)、[补查入口指令](./images/gdb-return-qemu411.png)。

### 11. 返回位置与 CSR 访问限制

**学生：** 最初讨论时，你建议在 `ecall` 后的 `0x80200470` 设置返回断点，并预期返回后能够同时读取 `pc` 和 `mepc`。实际断点已命中，`pc=0x80200470`，当前指令为 `mv a5,a0`，但查询 `mepc` 时显示 `Could not fetch register "mepc"; remote failure reply '14'`。该错误是否说明 SBI 调用未完成？

**AI：** 不能据此判断调用未完成。返回断点、PC 与当前指令已经表明程序恢复到 `ecall` 的下一条指令。先前关于返回后能够直接读取 `mepc` 的预期需要修正，寄存器访问结果与调用是否返回应分别分析。

**学生：** 因此，本次能直接确认返回位置为 `0x80200470`，但没有取得返回后的 `mepc` 数值。该位置与固件异常入口 `0x80000470` 也不同。寄存器读取失败是否与返回后的特权级有关？

**AI：** 根据本次对 QEMU 4.1.1 源码的核对，调试端读取 CSR 时仍检查当前特权级。返回内核后处于 S-mode，读取 M-mode 的 `mepc` 被拒绝，因而出现该错误。本次未在 `mret` 前直接读取 `mepc`，结论应限于已命中的返回断点、PC 和当前指令，不补写未读取到的寄存器值。

对应截图：[返回位置及寄存器读取错误](./images/gdb-return-qemu411.png)。

### 12. 输出计数与循环入口的验证

**学生：** 首字符输出验证完成后，如何检查整条消息的计数？`cprintf` 返回值应在何处读取，才能避免与 SBI 调用的底层返回值混淆？

**AI：** 禁用旧断点，在 `kern/init/init.c:12` 的 `while (1)` 处设置临时断点，继续执行至 `cprintf` 返回之后。本次在该位置测得 `pc=0x8020003a`、`a0=0x1e`，即 30；反汇编显示 `j 0x8020003a`，对应输出完成后的无限循环。

**学生：** 消息正文含 27 个非换行字符，加上消息自带的 1 个换行和格式串追加的 2 个换行，总计为 30，与返回值一致。截图中的相邻 `cputch` 函数不属于循环后续执行路径，因为当前指令始终跳转至自身；截图记录时 CPU 仍因断点暂停在循环入口。

对应截图：[输出计数与无限循环](./images/gdb-loop-qemu411.png)。

### 13. 完整输出与验证范围

**学生：** 寄存器中的计数已与预期一致，是否还需要检查同一次调试运行的终端输出？最终结果应如何表述？

**AI：** 计数反映输出回调的执行次数，仍需结合终端内容判断。本次运行 `make debug` 的终端显示 QEMU 4.1.1、OpenSBI v0.4、Runtime SBI Version 0.1，以及完整的 `(THU.CST) os is loading ...`，可与返回计数共同验证启动消息的输出。

**学生：** 因此，本次结论限定为已验证最小内核启动及该启动消息的 SBI 输出过程。其他格式符、输入接口和其他 SBI 服务未单独测试，官方自动评分与隐藏用例也不列为已通过项目。

对应截图：[同一次调试运行的完整输出](./images/qemu411-debug-console.png)。


# 3号Prompt 汇总 2413788张旗庭

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
