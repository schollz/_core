#!/usr/bin/env -S uv --quiet run --script
# /// script
# requires-python = ">=3.12"
# dependencies = [
#   "rich",
# ]
# ///

import os
import plistlib
import shutil
import subprocess
import sys
import time
from pathlib import Path

from rich.console import Console
from rich.panel import Panel
from rich.progress import (
    BarColumn,
    FileSizeColumn,
    Progress,
    SpinnerColumn,
    TextColumn,
    TimeElapsedColumn,
    TransferSpeedColumn,
)

if sys.platform != "darwin":
    print("This script is intended for macOS.")
    raise SystemExit(1)

if os.geteuid() != 0:
    print("Re-launching with sudo...")
    subprocess.execvp("sudo", ["sudo", sys.executable, *sys.argv])

DISK = os.environ.get("EZCORE_DISK", "disk4")
RAW_DISK = f"/dev/{DISK}"
LABEL = "EZEPTOCORE"
SRC = Path.home() / "Downloads/ezeptocore-data"
MAX_SIZE = 33 * 1024 * 1024 * 1024

console = Console()


def run(cmd: list[str], capture: bool = False) -> str:
    if capture:
        return subprocess.check_output(cmd, text=True).strip()
    subprocess.run(cmd, check=True)
    return ""


def run_quiet(cmd: list[str]) -> None:
    subprocess.run(cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)


def disk_info(device: str) -> dict | None:
    try:
        out = subprocess.check_output(["diskutil", "info", "-plist", device])
        return plistlib.loads(out)
    except subprocess.CalledProcessError:
        return None


def disk_present() -> bool:
    return disk_info(RAW_DISK) is not None


def disk_size() -> int:
    info = disk_info(RAW_DISK)
    if not info:
        return 0
    return int(info.get("TotalSize", 0))


def disk_ro() -> bool:
    info = disk_info(RAW_DISK)
    if not info:
        return True
    return bool(info.get("ReadOnlyMedia", False))


def status_line() -> str:
    info = disk_info(RAW_DISK)
    if not info:
        return f"{DISK}: not present"

    size = int(info.get("TotalSize", 0))
    size_gb = size / (1024 ** 3)
    writable = "RO" if info.get("ReadOnlyMedia", False) else "RW"
    prot = info.get("Protocol", "?")
    media = info.get("MediaName") or info.get("IORegistryEntryName") or "unknown"
    return f"{DISK} {writable} {size_gb:.2f} GiB {prot} {media}"


def first_partition_node() -> str:
    out = subprocess.check_output(["diskutil", "list", "-plist", RAW_DISK])
    data = plistlib.loads(out)

    disks = data.get("AllDisksAndPartitions", [])
    if not disks:
        raise RuntimeError(f"No partition table found for {RAW_DISK}")

    partitions = disks[0].get("Partitions", [])
    if not partitions:
        raise RuntimeError(f"No partitions found on {RAW_DISK}")

    ident = partitions[0].get("DeviceIdentifier")
    if not ident:
        raise RuntimeError("Unable to determine partition identifier")

    return f"/dev/{ident}"


def mount_point_for(device: str) -> Path:
    info = disk_info(device)
    if not info:
        raise RuntimeError(f"Unable to read info for {device}")

    mount_point = info.get("MountPoint")
    if not mount_point:
        raise RuntimeError(f"No mount point for {device}")

    return Path(mount_point)


if not SRC.exists():
    console.print(f"[bold red]Source folder missing:[/bold red] {SRC}")
    raise SystemExit(1)

console.print(Panel.fit("[bold cyan]EZEPTOCORE Media Preparation Utility (macOS)[/bold cyan]"))
console.print(f"[cyan]Target disk:[/cyan] {RAW_DISK} (override with EZCORE_DISK)")

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

with console.status("[yellow]Unmounting current volumes..."):
    run_quiet(["diskutil", "unmountDisk", "force", RAW_DISK])

with console.status("[yellow]Erasing and formatting FAT32 filesystem..."):
    run(["diskutil", "eraseDisk", "MS-DOS", LABEL, "MBRFormat", RAW_DISK])

part = first_partition_node()

with console.status("[yellow]Mounting filesystem..."):
    run(["diskutil", "mount", part])
    mount_dir = mount_point_for(part)

with console.status("[yellow]Flushing caches..."):
    run(["sync"])

files = sorted(
    [p for p in SRC.rglob("*") if p.is_file()],
    key=lambda f: f.stat().st_size,
    reverse=True,
)

total_bytes = sum(f.stat().st_size for f in files)

console.print(f"[cyan]Files:[/cyan] {len(files)}")
console.print(f"[cyan]Total Size:[/cyan] {total_bytes / (1024 ** 3):.2f} GB")
console.print(f"[cyan]Mount Point:[/cyan] {mount_dir}")

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
        dest = mount_dir / file.relative_to(SRC)
        dest.parent.mkdir(parents=True, exist_ok=True)

        with file.open("rb") as src_f, dest.open("wb") as dst_f:
            shutil.copyfileobj(src_f, dst_f, length=1024 * 1024)
            progress.update(task, advance=file.stat().st_size)

with console.status("[yellow]Final sync..."):
    run(["sync"])

with console.status("[yellow]Unmounting device..."):
    run(["diskutil", "unmountDisk", RAW_DISK])

console.print(Panel.fit("[bold green]✔ Operation Completed Successfully[/bold green]"))
