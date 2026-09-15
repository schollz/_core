#!/usr/bin/env -S /home/zns/.local/bin/uv --quiet run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#   "rich",
# ]
# ///

import os
import sys
import subprocess
import time
from pathlib import Path
from rich.console import Console
from rich.progress import (
    Progress,
    SpinnerColumn,
    BarColumn,
    TextColumn,
    TimeElapsedColumn,
    FileSizeColumn,
    TransferSpeedColumn,
)
from rich.panel import Panel

if os.geteuid() != 0:
    print("Re-launching with sudo...")
    subprocess.execvp(
        "sudo",
        ["sudo", sys.executable, *sys.argv],
    )

DISK = "sdd"
PART = "/dev/sdd1"
MOUNT = "/mnt/ezcore"
LABEL = "EZEPTOCORE"
SRC = Path("/home/zns/Downloads/ezeptocore-data")
MAX_SIZE = 33 * 1024 * 1024 * 1024

console = Console()


def run(cmd, capture=False):
    if capture:
        return subprocess.check_output(cmd, shell=True, text=True).strip()
    subprocess.run(cmd, shell=True, check=True)


def disk_present():
    return DISK in run("lsblk -o NAME", True)


def disk_size():
    out = run(f"lsblk -b -o NAME,SIZE | grep '^{DISK} '", True)
    return int(out.split()[1])


def disk_ro():
    out = run(f"lsblk -o NAME,RO | grep '^{DISK} '", True)
    return out.split()[1] == "1"


def status_line():
    return run(f"lsblk -o NAME,RO,SIZE,TYPE | grep '^{DISK}'", True)


console.print(Panel.fit("[bold cyan]EZEPTOCORE Media Preparation Utility[/bold cyan]"))

with console.status("[yellow]Waiting for device..."):
    while not disk_present():
        time.sleep(1)

console.print("[green]✔ Device detected[/green]")

while True:
    console.print(f"[cyan]{status_line()}[/cyan]")

    size = disk_size()
    ro = disk_ro()

    if size == 0:
        console.print("[red]Drive reports 0B — waiting for replug[/red]")
        while True:
            time.sleep(1)
            size = disk_size()
            if size != 0:
                break
        continue

    if size > MAX_SIZE:
        console.print("[bold red]PANIC: Drive exceeds safe size limit[/bold red]")
        raise SystemExit(1)

    if ro:
        console.print("[yellow]Drive is read-only — waiting for replug[/yellow]")
        while True:
            time.sleep(1)
            if not disk_ro():
                break
        continue

    break

console.print("[green]✔ Drive validation complete[/green]")

with console.status("[yellow]Unmounting previous mounts..."):
    subprocess.run(
        f"sudo umount {PART}",
        shell=True,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

with console.status("[yellow]Formatting FAT32 filesystem..."):
    run(f"sudo mkfs.fat -F 32 -s 128 -S 512 -n {LABEL} {PART}")

with console.status("[yellow]Mounting filesystem..."):
    run(f"sudo mkdir -p {MOUNT}")
    run(f"sudo mount -o noatime,nodiratime {PART} {MOUNT}")

with console.status("[yellow]Flushing caches..."):
    run("sync")
    run("echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null")

files = sorted(
    [p for p in SRC.rglob("*") if p.is_file()],
    key=lambda f: f.stat().st_size,
    reverse=True,
)

total_bytes = sum(f.stat().st_size for f in files)

console.print(f"[cyan]Files:[/cyan] {len(files)}")
console.print(f"[cyan]Total Size:[/cyan] {total_bytes / (1024 ** 3):.2f} GB")

progress = Progress(
    SpinnerColumn(),
    TextColumn("[bold blue]{task.description}"),
    BarColumn(),
    FileSizeColumn(),
    TransferSpeedColumn(),
    TimeElapsedColumn(),
)

with progress:
    task = progress.add_task("Copying data", total=total_bytes)

    for file in files:
        dest = Path(MOUNT) / file.relative_to(SRC)
        dest.parent.mkdir(parents=True, exist_ok=True)

        with open(file, "rb") as src_f, open(dest, "wb") as dst_f:
            while True:
                buf = src_f.read(1024 * 1024)
                if not buf:
                    break
                dst_f.write(buf)
                progress.update(task, advance=len(buf))

with console.status("[yellow]Final sync..."):
    run("sync")

with console.status("[yellow]Unmounting device..."):
    run(f"sudo umount {MOUNT}")

console.print(Panel.fit("[bold green]✔ Operation Completed Successfully[/bold green]"))
