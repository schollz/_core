#!/usr/bin/env python3
"""Explicit hardware test: flash paired witness builds and record observer overhead.

This is a programming/test workflow, separate from the read-only debug server.
The caller must first preserve the original device image and supply paired ELFs.
"""
from __future__ import annotations
import argparse
import json
from pathlib import Path
import socket
import subprocess
import time

from zeptocore_debug_protocol import DeviceReader, ElfImage, OpenOcdRpc
from zeptocore_witness import read as witness_read
from zeptocore_debug import summarize
from zeptocore_midi import wait_available


def memory_checkpoint(elf, rpc):
    """Read startup allocator/DSP state in both paired builds without calling firmware."""
    rpc.running()
    total = elf.symbols["__StackLimit"][0]-elf.symbols["__bss_end__"][0]
    address, size = elf.symbols["__malloc_current_mallinfo"]
    if size != 40 or not 0x20000000 <= address <= 0x20040000-size:
        raise RuntimeError("unexpected allocator layout")
    first = rpc.read(address, 10)
    if first != rpc.read(address, 10):
        raise RuntimeError("allocator changed during checkpoint")
    address, size = elf.symbols["freeverb"]
    if size != 4 or not 0x20000000 <= address < 0x20040000:
        raise RuntimeError("unexpected reverb pointer")
    pointer = rpc.read(address, 1)[0]
    counts = 0
    if pointer:
        if not 0x20000000 <= pointer <= 0x20040000-36:
            raise RuntimeError("invalid reverb object")
        # FV_Reverb begins with eight floats, then its two int8_t counts.
        counts = rpc.read(pointer+32, 1)[0]
    checkpoint = ("after controls initialization" if "zeptocore_diag" in elf.symbols else
                  "after reverb, before map arena and controls allocations; not current free heap")
    return {"heap_total_bytes": total, "heap_free_at_last_mallinfo_bytes": total-first[7],
            "checkpoint_stage": checkpoint,
            "reverb_combs": counts & 255, "reverb_allpasses": (counts >> 8) & 255,
            "source": "last startup mallinfo plus current FV_Reverb counts; running SWD reads"}


def run_case(elf_path, mode, interval, destination, seconds, audio_device, midi_port, midi):
    destination.mkdir(parents=True, exist_ok=False)
    elf = ElfImage(elf_path)
    (destination / "firmware.elf").write_bytes(elf.data)
    print(f"{mode}: programming {elf_path}", flush=True)
    with (destination / "flash.log").open("w") as log:
        subprocess.run(["openocd", "-f", "interface/cmsis-dap.cfg", "-f", "target/rp2040.cfg",
                        "-c", "adapter speed 5000", "-c",
                        f"program {{{elf.path}}} verify reset exit"],
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
    time.sleep(3)
    with socket.socket() as reserve:
        reserve.bind(("127.0.0.1", 0))
        port = reserve.getsockname()[1]
    log = (destination / "openocd.log").open("w")
    rpc_log = (destination / "rpc.jsonl").open("w")
    process = subprocess.Popen(["openocd", "-f", "interface/cmsis-dap.cfg", "-f", "target/rp2040.cfg",
                                "-c", "bindto 127.0.0.1", "-c", "gdb_port disabled",
                                "-c", "telnet_port disabled", "-c", f"tcl_port {port}",
                                "-c", "adapter speed 1000", "-c", "init"],
                               stdout=log, stderr=subprocess.STDOUT)
    rpc, audio = None, None
    try:
        deadline = time.monotonic()+5
        while rpc is None:
            try:
                rpc = OpenOcdRpc(port, log=rpc_log)
            except OSError:
                if process.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("OpenOCD did not start")
                time.sleep(.1)
        rpc.running()
        address, size = elf.symbols["zeptocore_timing_witness"]
        if size != 36:
            raise RuntimeError("unexpected witness size")
        starvation_address, starvation_size = elf.symbols["zeptocore_starvation_witness"]
        if starvation_size != 4:
            raise RuntimeError("unexpected starvation witness size")
        reader = DeviceReader(elf, rpc) if mode != "compiled-out" else None
        if reader:
            reader.validate()
        # Existing musical controls only; no state writes through SWD.
        wait_available(midi_port)
        subprocess.run(["amidi", "-p", midi_port, "-S", midi], check=True)
        time.sleep(1)
        memory = memory_checkpoint(elf, rpc)
        with (destination / "arecord.log").open("w") as audio_log:
            audio = subprocess.Popen(["arecord", "-D", audio_device, "-f", "S32_LE", "-r", "48000",
                                      "-c", "2", "-d", str(int(seconds)+2), "-t", "wav",
                                      str(destination / "output.wav")],
                                     stdout=audio_log, stderr=subprocess.STDOUT)
            snapshots = []
            if reader:
                snapshots.append(reader.snapshot())
            first = witness_read(rpc, address)
            starvation_first = rpc.read(starvation_address, 1)[0]
            deadline = time.monotonic()+seconds
            next_poll = time.monotonic()+(interval or seconds+1)
            print(f"{mode}: recording for {seconds} seconds", flush=True)
            while time.monotonic() < deadline:
                if interval and time.monotonic() >= next_poll:
                    snapshots.append(reader.snapshot())
                    next_poll += interval
                    if next_poll < time.monotonic():
                        next_poll = time.monotonic()+interval
                time.sleep(min(.05, max(0, deadline-time.monotonic())))
            last = witness_read(rpc, address)
            starvation_last = rpc.read(starvation_address, 1)[0]
            if reader:
                snapshots.append(reader.snapshot())
            if audio.wait(timeout=5) != 0:
                raise RuntimeError("audio capture failed")
        count = last["count"]-first["count"]
        rendered = last["rendered_count"]-first["rendered_count"]
        if count <= 0 or rendered <= 0:
            raise RuntimeError("witness stopped or reset")
        result = {"mode": mode, "poll_interval": interval, "elf_sha256": elf.sha256,
                  "memory": memory,
                  "first": first, "last": last, "audio_device": audio_device, "midi_port": midi_port,
                  "midi_hex": midi, "starvation_first": starvation_first,
                  "starvation_last": starvation_last,
                  "independent_starvation_delta": starvation_last-starvation_first,
                  "mean_us": (last["total_us"]-first["total_us"])/count,
                  "rendered_mean_us": (last["rendered_total_us"]-first["rendered_total_us"])/rendered,
                  "diagnostics": summarize(snapshots) if snapshots else None}
        (destination / "result.json").write_text(json.dumps(result, indent=2)+"\n")
        (destination / "snapshots.jsonl").write_text("".join(json.dumps(s)+"\n" for s in snapshots))
        print(f"{mode}: rendered mean {result['rendered_mean_us']:.2f} us", flush=True)
        return result
    finally:
        if audio and audio.poll() is None:
            audio.terminate()
            audio.wait(timeout=5)
        if rpc:
            rpc.close()
        process.terminate()
        process.wait(timeout=5)
        log.close()
        rpc_log.close()


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--on-elf", required=True)
    p.add_argument("--off-elf", required=True)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--duration", type=float, default=20)
    p.add_argument("--repeats", type=int, default=3)
    p.add_argument("--audio-device", default="hw:4,0")
    p.add_argument("--midi-port", default="hw:6,0,0")
    p.add_argument("--midi-hex", default="B01200B01600B01800B01900B01040B01400FA")
    p.add_argument("--modes", nargs="+", choices=["compiled-out", "collect-only", "poll-1hz", "poll-4hz"],
                   help="Optional focused repeat using the same paired builds")
    args = p.parse_args()
    if args.duration < 5 or args.duration > 50 or args.repeats < 1:
        p.error("use 5..50 seconds and at least one repeat")
    args.out.mkdir(parents=True, exist_ok=False)
    results = []
    cases = [("compiled-out", args.off_elf, None), ("collect-only", args.on_elf, None),
             ("poll-1hz", args.on_elf, 1), ("poll-4hz", args.on_elf, .25)]
    if args.modes:
        cases = [case for case in cases if case[0] in args.modes]
    for repeat in range(args.repeats):
        # Alternate order to expose warm-up/order effects.
        for mode, elf, interval in cases if repeat % 2 == 0 else list(reversed(cases)):
            results.append(run_case(elf, mode, interval, args.out / f"{repeat}-{mode}",
                                    args.duration, args.audio_device, args.midi_port, args.midi_hex))
            (args.out / "results.json").write_text(json.dumps(results, indent=2)+"\n")


if __name__ == "__main__":
    main()
