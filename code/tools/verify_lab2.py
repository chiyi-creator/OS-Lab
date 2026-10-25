#!/usr/bin/env python3
"""Run real builds/boots and preserve command/output/exit status for the report."""
import datetime
import json
import os
from pathlib import Path
import shlex
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
LOG = ROOT.parent / "report" / "logs"
LOG.mkdir(parents=True, exist_ok=True)
QEMU = os.environ.get("QEMU", str(Path.home() / "lab1-tools/qemu-4.1.1/riscv64-softmmu/qemu-system-riscv64"))
PREFIX = str(Path.home() / "riscv-elf-toolchains/bin/riscv64-unknown-elf-")
results = []


def run(name, cmd, timeout=90, required=(), expected=(0,), allow_idle=False):
    started = datetime.datetime.now(datetime.timezone.utc).isoformat()
    p = subprocess.Popen(cmd, cwd=ROOT, stdin=subprocess.DEVNULL, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    expired = False
    try:
        data, _ = p.communicate(timeout=timeout)
    except subprocess.TimeoutExpired:
        expired = True
        p.terminate()
        data, _ = p.communicate(timeout=10)
    out = data.decode("utf-8", "replace")
    good = ((expired and allow_idle) or
            (not expired and p.returncode in expected)) and all(s in out for s in required)
    good = good and "kernel panic" not in out and "FAIL" not in out
    text = f"UTC: {started}\n$ {shlex.join(cmd)}\n\n{out}"
    text += f"\n[runner] exit={p.returncode}; stopped_after_timeout={expired}; verified={good}\n"
    (LOG / f"{name}.log").write_text(text)
    results.append({"name": name, "command": cmd, "exit": p.returncode,
                    "timeout": expired, "verified": good})
    (LOG / "verification.json").write_text(json.dumps(results, indent=2))
    print(f"{name}: {'PASS' if good else 'FAIL'}", flush=True)
    if not good:
        print(out[-5000:], flush=True)
        raise SystemExit(1)


if __name__ == "__main__":
    run("environment", ["bash", "-c",
        f"uname -a; {shlex.quote(PREFIX+'gcc')} --version; "
        f"{shlex.quote(PREFIX+'gdb')} --version; gdb --version; "
        f"{shlex.quote(QEMU)} --version; file {shlex.quote(QEMU)}"])
    for mode in ["buddy", "first_fit", "best_fit"]:
        # -B records a complete build even if an earlier exploratory build exists.
        run(f"build-{mode}", ["make", "-B", "-j4", f"PMM={mode}"])
        expected = ["check_alloc_page() succeeded!", f"memory management: {'default' if mode == 'first_fit' else mode}_pmm_manager"]
        if mode == "buddy":
            expected += ["Buddy self-test: ALL PASS"] + [f"[Buddy {n}]" for n in range(1, 9)]
        run(f"boot-{mode}", [QEMU, "-machine", "virt", "-m", "128M", "-nographic",
            "-bios", "default", "-device", f"loader,file=bin/{mode}/ucore.img,addr=0x80200000"],
            timeout=8, required=expected, allow_idle=True)
    for ram in ["64M", "256M"]:
        run(f"boot-buddy-{ram}", [QEMU, "-machine", "virt", "-m", ram, "-nographic",
            "-bios", "default", "-device", "loader,file=bin/buddy/ucore.img,addr=0x80200000"],
            timeout=8, required=["Buddy self-test: ALL PASS", "check_alloc_page() succeeded!"],
            allow_idle=True)
    run("grade", ["make", "grade"], timeout=90, required=["Total Score: 20/20"])
    run("build-probe", ["make", "-B", "-j4", "PMM=buddy", "MMU_PROBE=1"])
    run("boot-probe", [QEMU, "-machine", "virt", "-nographic", "-bios", "default",
        "-device", "loader,file=bin/buddy-probe/ucore.img,addr=0x80200000"],
        timeout=8, required=["MMU probe:", "first=0x1122334455667788 second=0x1122334455667788"],
        allow_idle=True)
    run("build-default-final", ["make", "-j4"])
    # make itself starts QEMU, so use an outer timeout to terminate the group.
    run("boot-default-make", ["timeout", "--signal=TERM", "30s", "make", "qemu"],
        timeout=45, expected=(124,), required=[
            "memory management: best_fit_pmm_manager", "check_alloc_page() succeeded!"])
    print("ALL BUILD/BOOT CHECKS PASSED", flush=True)
