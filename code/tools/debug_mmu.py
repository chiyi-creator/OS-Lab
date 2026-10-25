#!/usr/bin/env python3
"""Drive two real GDB processes. Host GDB launches QEMU (no ptrace override).
Raw PTY transcripts are stored separately from concise milestone logs.
"""
from pathlib import Path
import json
import os
import re
import subprocess
import time
import pexpect

ROOT = Path(__file__).resolve().parents[1]
LOG = ROOT.parent / "report" / "logs"
LOG.mkdir(parents=True, exist_ok=True)
QEMU = Path.home() / "lab1-tools/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64"
GUEST_GDB = Path.home() / "riscv-elf-toolchains/bin/riscv64-unknown-elf-gdb"
SRC = QEMU.parent.parent
PORT = "12345"
children = []
records = {}


class GDB:
    def __init__(self, name, executable, args):
        self.name = name
        self.file = (LOG / f"gdb-{name}-raw.log").open("w")
        self.p = pexpect.spawn(str(executable), args, cwd=str(ROOT), encoding="utf-8",
                               timeout=45, dimensions=(50, 180), echo=False)
        self.p.logfile = self.file
        children.append(self.p)
        self.p.expect_exact("(gdb) ")
        self.prompt = f"({name}) "
        self.p.sendline("set prompt " + self.prompt)
        self.p.expect_exact(self.prompt)
        for command in ["set pagination off", "set confirm off", "set width 0",
                        "set print pretty off", "set style enabled off"]:
            self.cmd(command)

    def cmd(self, text, wait=True):
        self.p.sendline(text)
        if not wait:
            return ""
        self.p.expect_exact(self.prompt)
        out = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "", self.p.before.replace("\r", ""))
        print(self.name, text, "=>", out[-170:].strip(), flush=True)
        return f"({self.name}) {text}\n{out}"

    def pending(self):
        self.p.expect_exact(self.prompt)
        out = re.sub(r"\x1b\[[0-9;?]*[A-Za-z]", "", self.p.before.replace("\r", ""))
        return f"({self.name}) {out}"

    def interrupt(self):
        self.p.sendcontrol("c")
        return self.pending()

    def section(self, key, commands):
        text = "".join(self.cmd(c) for c in commands)
        records[key] = records.get(key, "") + text
        (LOG / (key + ".log")).write_text(records[key])
        return text


try:
    symbols = subprocess.check_output(
        [str(GUEST_GDB).replace("gdb", "nm"), "-n", "bin/buddy-probe/kernel"],
        cwd=ROOT, text=True)
    symbol = {row.split()[2]: int(row.split()[0], 16)
              for row in symbols.splitlines() if len(row.split()) == 3}
    va = symbol["mmu_probe_data"]
    host = GDB("host", "gdb", ["-q", "--args", str(QEMU),
        "-machine", "virt", "-m", "128M", "-display", "none", "-monitor", "none",
        "-serial", "file:" + str(LOG / "debug-qemu.log"), "-bios", "default",
        "-device", "loader,file=bin/buddy-probe/ucore.img,addr=0x80200000",
        "-S", "-gdb", "tcp:127.0.0.1:" + PORT])
    host.cmd("handle SIGPIPE nostop noprint pass")
    host.cmd("handle SIGUSR1 nostop noprint pass")
    host.cmd("run", wait=False)
    time.sleep(1)
    guest = GDB("guest", GUEST_GDB, ["-q", "bin/buddy-probe/kernel"])
    guest.section("debug-01-guest", [
        "set architecture riscv:rv64", "target remote 127.0.0.1:" + PORT,
        "break *lab2_probe_first", "continue", "x/3i $pc",
        "info registers pc satp", "p/x &mmu_probe_data",
        "x/gx ((char *)&boot_page_table_sv39)+4088"])
    host.interrupt()
    host.section("debug-02-host-ready", [
        "set $fills = 0",
        f"break riscv_cpu_tlb_fill if address == {va:#x} && access_type == 0",
        "commands 1\nsilent\nset $fills = $fills + 1\nend"])
    host.cmd("continue", wait=False)
    guest.cmd("si", wait=False)
    first = host.pending()
    (LOG / "debug-03-fill.log").write_text(first)
    host.section("debug-03-fill", [
        "info args", "bt 6",
        "set $rvcpu = (RISCVCPU *)cs",
        "set $cpuenv = &$rvcpu->env",
        f"set $va = {va:#x}",
        "set $idx = ($va >> 12) & ($rvcpu->neg.tlb.f[1].mask >> 5)",
        "p $idx", "p/x $rvcpu->neg.tlb.f[1].table[$idx].addr_read",
        "p/x $cpuenv->satp",
        "tbreak get_physical_address", "continue", "info args",
        "set $result_pa = physical", "set $result_prot = prot"])
    host.section("debug-04-walk", [
        "next", "next", "next",
        f"tbreak {SRC}/target/riscv/cpu_helper.c:254", "continue",
        "p levels", "p ptidxbits", "p ptesize", "p i", "p idx",
        "p/x base", "p/x pte_addr", "p/x pte", "p ptshift",
        "next", "next", "next", "finish",
        "p/x *$result_pa", "p/x *$result_prot"])
    host.section("debug-05-tlb", [
        f"tbreak {SRC}/target/riscv/cpu_helper.c:474", "continue",
        "p/x $rvcpu->neg.tlb.f[1].table[$idx].addr_read",
        "p/x $rvcpu->neg.tlb.f[1].table[$idx].addend",
        "x/gx (void *)($va + $rvcpu->neg.tlb.f[1].table[$idx].addend)",
        "p $fills"])
    host.cmd("continue", wait=False)
    guest_first = guest.pending()
    (LOG / "debug-06-first-load.log").write_text(guest_first)
    guest.section("debug-06-first-load", ["x/i $pc", "info registers a0 a1 a2 a3 a4 a5"])
    # Keep the fill breakpoint active during the second real load.
    guest.section("debug-07-second-load", ["si", "x/i $pc",
        "info registers a0 a1 a2 a3 a4 a5"])
    host.interrupt()
    host.section("debug-08-hit-count", ["p $fills", "info breakpoints",
        "p/x $rvcpu->neg.tlb.f[1].table[$idx].addr_read"])
    host.cmd("delete breakpoints")
    # Controlled comparison: the exact same ld at its physical address, S-mode
    # with SATP=Bare. Do not continue the kernel in this temporary register state.
    pa = va - 0xffffffff40000000
    pc_pa = symbol["lab2_probe_first"] - 0xffffffff40000000
    host.cmd(f"break riscv_cpu_tlb_fill if address == {pa:#x} && access_type == 0")
    host.cmd("continue", wait=False)
    guest.section("debug-09-bare-setup", ["delete breakpoints",
        "set $satp = 0", f"set $pc = {pc_pa:#x}", f"set $a1 = {pa:#x}",
        "info registers pc satp a1", "x/i $pc"])
    guest.cmd("si", wait=False)
    host.pending()
    host.section("debug-10-bare-walk", ["info args", "bt 4",
        "tbreak get_physical_address", "continue", "info args",
        "set $result_pa = physical", "p/x env->satp", "next", "next",
        f"tbreak {SRC}/target/riscv/cpu_helper.c:197", "continue",
        "p vm", "p/x addr", "next", "finish", "p/x *$result_pa"])
    host.cmd("delete breakpoints")
    host.cmd("continue", wait=False)
    guest.pending()
    guest.section("debug-11-bare-result", ["info registers pc satp a1 a2"])
    host.interrupt()
    host.cmd("kill")
    host.cmd("quit", wait=False)
    guest.cmd("quit", wait=False)
    (LOG / "debug-symbols.json").write_text(json.dumps(
        {s: hex(symbol[s]) for s in ["mmu_probe_data", "lab2_probe_first",
                                    "lab2_probe_second", "lab2_probe_done"]}, indent=2))
    checks = {
        "sv39_root_pte": "0x200000cf" in records["debug-04-walk"],
        "translated_pa": hex(pa) in records["debug-04-walk"],
        "tlb_initial_invalid": "0xffffffffffffffff" in records["debug-03-fill"],
        "tlb_filled": hex(va) in records["debug-05-tlb"],
        "second_load_data": "0x1122334455667788" in records["debug-07-second-load"],
        "one_demand_fill": "breakpoint already hit 1 time" in records["debug-08-hit-count"],
        "bare_identity": hex(pa) in records["debug-10-bare-walk"],
        "bare_load_data": "0x1122334455667788" in records["debug-11-bare-result"],
        "no_gdb_errors": not any(s in "".join(records.values()) for s in
            ["There is no member", "Cannot access memory", "No symbol ", "not defined."])
    }
    (LOG / "debug-checks.json").write_text(json.dumps(checks, indent=2))
    assert all(checks.values()), checks
    print("DOUBLE GDB SESSION COMPLETED", flush=True)
finally:
    for p in children:
        if p.isalive():
            p.close(force=True)
