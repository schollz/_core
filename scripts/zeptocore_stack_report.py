#!/usr/bin/env python3
"""Explicit post-capture stack inspection: HALTS both cores, dumps, then resets.

Do not run during an audio continuity/timing capture or a firmware SD write.
Painted-stack observations bound usage only for the exercised workload.
"""
import argparse
import json
from pathlib import Path
import struct
import subprocess

from zeptocore_debug_protocol import ElfImage


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", required=True)
    p.add_argument("--out", required=True, type=Path)
    args = p.parse_args()
    elf = ElfImage(args.elf)
    args.out.mkdir(parents=True, exist_ok=False)
    regions = []
    command = ["openocd", "-f", "interface/cmsis-dap.cfg", "-f", "target/rp2040.cfg",
               "-c", "adapter speed 5000", "-c", "init; halt"]
    for core, bottom, top in [(0, "__scratch_y_end__", "__StackTop"),
                               (1, "__StackOneBottom", "__StackOneTop")]:
        start = (elf.symbols[bottom][0] + 3) & ~3
        end = elf.symbols[top][0]
        if not 0x20040000 <= start < end <= 0x20042000:
            raise RuntimeError("unexpected stack region")
        path = (args.out / f"core{core}.bin").resolve()
        regions.append((core, start, end, path))
        command += ["-c", f"dump_image {{{path}}} 0x{start:x} {end-start}"]
    command += ["-c", "reset run; shutdown"]
    with (args.out / "openocd.log").open("w") as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True, timeout=20)
    result = {"elf_sha256": elf.sha256, "cores": [],
              "limitation": "Conservative observed stack use for this run; not proof for unexercised paths."}
    for core, start, end, path in regions:
        raw = path.read_bytes()
        if len(raw) != end-start:
            raise RuntimeError("short stack dump")
        values = struct.unpack(f"<{len(raw)//4}I", raw)
        free = 0
        for value in values:
            if value != 0xA55AA55A:
                break
            free += 4
        declared_bottom = elf.symbols["__StackBottom" if core == 0 else "__StackOneBottom"][0]
        result["cores"].append({"core": core, "available_bytes": end-start,
                                "declared_minimum_bytes": end-declared_bottom,
                                "untouched_low_bytes": free,
                                "observed_used_bytes_upper_bound": end-start-free,
                                "bottom_guard_intact": free >= 32})
    (args.out / "report.json").write_text(json.dumps(result, indent=2)+"\n")
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
