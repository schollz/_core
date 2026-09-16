#!/usr/bin/env python3
"""Read the independent callback witness; never writes to device memory."""
import argparse
import json
import time
from zeptocore_debug_protocol import DiagnosticError, ElfImage, OpenOcdRpc


def read(rpc, address):
    rpc.running()
    for _ in range(20):
        before = rpc.read(address, 9)
        after = rpc.read(address, 1)[0]
        if before[0] == after and not after & 1:
            return {"host_time": time.time(), "sequence": after, "count": before[1],
                    "total_us": before[2] | before[3] << 32, "max_us": before[4],
                    "rendered_count": before[5], "rendered_total_us": before[6] | before[7] << 32,
                    "rendered_max_us": before[8]}
    raise DiagnosticError("witness changed during every bounded read attempt")


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", required=True)
    p.add_argument("--rpc-port", type=int, required=True)
    p.add_argument("--duration", type=float, default=15)
    args = p.parse_args()
    elf = ElfImage(args.elf)
    address, length = elf.symbols["zeptocore_timing_witness"]
    if length != 36 or not 0x20000000 <= address <= 0x20040000-length:
        raise DiagnosticError("invalid witness address/size")
    rpc = OpenOcdRpc(args.rpc_port)
    try:
        first = read(rpc, address)
        time.sleep(args.duration)
        last = read(rpc, address)
        count = last["count"]-first["count"]
        rendered = last["rendered_count"]-first["rendered_count"]
        if count <= 0 or rendered <= 0:
            raise DiagnosticError("witness has not progressed or was reset")
        print(json.dumps({"elf_sha256": elf.sha256, "first": first, "last": last,
                          "mean_us": (last["total_us"]-first["total_us"])/count,
                          "rendered_mean_us": (last["rendered_total_us"]-first["rendered_total_us"])/rendered}, indent=2))
    finally:
        rpc.close()


if __name__ == "__main__":
    main()
