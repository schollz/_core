#!/usr/bin/env python3
"""Explicit flash-and-capture smoke test for one diagnostic firmware build."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time
import threading

from zeptocore_debug import request
from zeptocore_debug_protocol import ElfImage
from zeptocore_midi import wait_available


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    p.add_argument("--seconds", type=int, default=10)
    p.add_argument("--midi-port", default="hw:6,0,0")
    p.add_argument("--audio-device", default="hw:4,0")
    p.add_argument("--skip-stack", action="store_true",
                   help="Leave firmware running after capture; omit the separate halt/reset stack check")
    p.add_argument("--test-controls", action="store_true",
                   help="Use only with SEEK_TEST_CONTROLS firmware; exercises reverse/slices/variation")
    p.add_argument("--modes", nargs="+", choices=["ordinary", "stretch", "sample-switch", "transport",
                                                "reverse", "slices", "variation", "audio-variant", "audio-variant-variation",
                                                "long-reverse", "long-stretch", "long-slices"],
                   help="Restrict this observation to selected workloads")
    p.add_argument("--repeat-switches", action="store_true",
                   help="Alternate the existing MIDI sample selections once per second during sample-switch capture")
    p.add_argument("--preload-maps", action="store_true",
                   help="Before each workload, stop through MIDI and let a queued persisted map load finish")
    p.add_argument("--long-files", action="store_true",
                   help="Also exercise reverse/stretch/slices on the larger existing variation WAV")
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / "firmware.elf").write_bytes(ElfImage(args.elf).data)
    root = Path(__file__).resolve().parents[1]
    socket_path = str((args.out / "debug.sock").resolve())
    if len(socket_path) > 100:
        raise RuntimeError("artifact path too long for Unix socket")
    print(f"Programming {args.elf}", flush=True)
    with (args.out / "flash.log").open("w") as log:
        subprocess.run(["openocd", "-f", "interface/cmsis-dap.cfg", "-f", "target/rp2040.cfg",
                        "-c", "adapter speed 5000", "-c", f"program {{{args.elf.resolve()}}} verify reset exit"],
                       stdout=log, stderr=subprocess.STDOUT, check=True, timeout=60)
    time.sleep(3)
    with (args.out / "server.log").open("w") as log:
        server = subprocess.Popen([sys.executable, str(root / "scripts/zeptocore_debug_server.py"),
                                   "--elf", str(args.elf), "--socket", socket_path,
                                   "--artifacts", str(args.out / "server")],
                                  stdout=log, stderr=subprocess.STDOUT)
        try:
            deadline = time.monotonic()+8
            while not Path(socket_path).exists():
                if server.poll() is not None or time.monotonic() > deadline:
                    raise RuntimeError("debug server failed; inspect server.log")
                time.sleep(.1)
            startup_deadline = time.monotonic()+60
            while True:
                try:
                    info = request(socket_path, "device.info")
                    if info["data"]["stage"] == 3:
                        break
                except RuntimeError as exc:
                    if "snapshot timeout" not in str(exc):
                        raise
                if time.monotonic() > startup_deadline:
                    raise RuntimeError("media preparation did not finish within the host observation window")
                time.sleep(.1)
            print(json.dumps(info["data"]), flush=True)
            (args.out / "device.json").write_text(json.dumps(info, indent=2)+"\n")
            if info["data"]["features"] & 8:
                maps = request(socket_path, "maps.status")
                (args.out / "maps-startup.json").write_text(json.dumps(maps, indent=2)+"\n")
                print("Startup maps: "+json.dumps(maps["data"]), flush=True)
            if "zeptocore_seek_benchmark" in ElfImage(args.elf).symbols:
                benchmark = request(socket_path, "maps.benchmark")
                (args.out / "benchmark.json").write_text(json.dumps(benchmark, indent=2)+"\n")
                if not benchmark.get("data", {}).get("valid"):
                    raise RuntimeError("boot filesystem benchmark failed; inspect benchmark.json")
            if info["data"]["features"] & 1024:
                fixture = request(socket_path, "maps.fixture")
                (args.out / "fixture.json").write_text(json.dumps(fixture, indent=2)+"\n")
                if not fixture.get("data", {}).get("valid"):
                    raise RuntimeError("guarded audio fixture failed; inspect fixture.json")
            wait_available(args.midi_port)
            modes = [("ordinary", "B01200B01600B01800B01900B01040B01400FA"),
                     ("stretch", "B01440FA"), ("sample-switch", "B01400B01540FA")]
            if args.modes and "transport" in args.modes:
                modes += [("transport", "B01200B01600B01800B01900B01040B01400FA")]
            if args.test_controls:
                modes += [("reverse", "B01500B01400B06E7FFA"),
                          ("slices", "B06E00B06F00FA"),
                          ("variation", "B0707FFA")]
                if args.modes and "audio-variant" in args.modes:
                    modes += [("audio-variant", "B01500B01400B07000B07101FA")]
                if args.modes and "audio-variant-variation" in args.modes:
                    modes += [("audio-variant-variation", "B01500B01400B07101B0707FFA")]
                if args.long_files or any(m.startswith("long-") for m in args.modes or []):
                    modes += [("long-reverse", "B01500B01400B0707FB06E7FFA"),
                              ("long-stretch", "B01500B0707FB06E00B01440FA"),
                              ("long-slices", "B01500B0707FB06E00B01400FA")]
            for mode, midi in modes:
                if args.modes and mode not in args.modes:
                    continue
                subprocess.run(["amidi", "-p", args.midi_port, "-S", midi], check=True)
                time.sleep(.5)
                if args.preload_maps:
                    before = request(socket_path, "maps.status")["data"]
                    subprocess.run(["amidi", "-p", args.midi_port, "-S", "FC"], check=True)
                    deadline = time.monotonic()+5
                    while True:
                        after = request(socket_path, "maps.status")["data"]
                        if not after["pending_job"]:
                            break
                        if time.monotonic()>deadline:
                            raise RuntimeError("queued map did not reach a quiet window")
                        time.sleep(.1)
                    if after["builds"]!=before["builds"] or after["index_writes"]!=before["index_writes"]:
                        raise RuntimeError("comparison preload required construction or persistence")
                    (args.out / f"{mode}-preload.json").write_text(json.dumps(
                        {"before": before, "after": after, "midi": ["FC", "FA"]}, indent=2)+"\n")
                    subprocess.run(["amidi", "-p", args.midi_port, "-S", "FA"], check=True)
                    time.sleep(.5)
                if mode == "long-reverse":
                    # Let the variation reopen finish before jumping near its
                    # end; batching both events can use the old variation scale.
                    subprocess.run(["amidi", "-p", args.midi_port, "-S", "B06F0F"], check=True)
                    time.sleep(.25)
                print(f"Capturing {mode}", flush=True)
                stop_controls = threading.Event()
                control_events = []
                def controls():
                    sequence = [0, 7, 2, 15, 4, 11, 1, 8]
                    index = 0
                    interval = 1.0 if mode in ("sample-switch", "transport") else .25
                    while not stop_controls.wait(interval):
                        packet = (["FC", "B01440B01500FA", "FC", "B01400B01540FB"][index % 4] if mode == "transport" else
                                  f"B015{[0,64][index%2]:02X}" if mode == "sample-switch" else
                                  f"B06F{sequence[index % len(sequence)]:02X}")
                        subprocess.run(["amidi", "-p", args.midi_port, "-S", packet], check=True)
                        control_events.append({"host_time": time.time(), "midi_hex": packet})
                        index += 1
                control_thread = threading.Thread(target=controls, daemon=True) if (
                    mode == "transport" or mode.endswith("slices") or
                    (mode == "sample-switch" and args.repeat_switches)) else None
                if control_thread:
                    control_thread.start()
                with (args.out / f"{mode}-audio.log").open("w") as audio_log:
                    recorder = subprocess.Popen(["arecord", "-D", args.audio_device,
                        "-f", "S32_LE", "-r", "48000", "-c", "2", "-d", str(args.seconds+2),
                        "-t", "wav", str(args.out / f"{mode}.wav")], stdout=audio_log, stderr=subprocess.STDOUT)
                    try:
                        with (args.out / f"{mode}.log").open("w") as capture_log:
                            subprocess.run([sys.executable, str(root / "scripts/zeptocore_debug.py"),
                                "--socket", socket_path, "capture", "--duration", str(args.seconds),
                                "--interval", ".25", "--workload", mode, "--out", str(args.out / mode)],
                                stdout=capture_log, stderr=subprocess.STDOUT, check=True, timeout=args.seconds+15)
                        if recorder.wait(timeout=5) != 0:
                            raise RuntimeError("audio recorder failed")
                    finally:
                        stop_controls.set()
                        if control_thread:
                            control_thread.join(timeout=3)
                        if recorder.poll() is None:
                            recorder.terminate()
                            recorder.wait(timeout=5)
                (args.out / f"{mode}-controls.json").write_text(json.dumps({
                    "initial_midi_hex": midi, "events": control_events,
                    "timing": "Host schedule; audio block alignment is not guaranteed."}, indent=2)+"\n")
                summary = json.loads((args.out / mode / "summary.json").read_text())
                print(f"{mode}: valid={summary['valid']} starvation={summary.get('starvation_count')}", flush=True)
                if info["data"]["features"] & 8:
                    maps = request(socket_path, "maps.status")
                    (args.out / f"{mode}-maps.json").write_text(json.dumps(maps, indent=2)+"\n")
            memory = request(socket_path, "memory.status")
            (args.out / "memory.json").write_text(json.dumps(memory, indent=2)+"\n")
            subprocess.run(["amidi", "-p", args.midi_port, "-S", "B01500B01400B06E00B07000B07100FA"], check=True)
            if not args.skip_stack:
                # Stop playback and let deferred media work finish before the
                # separate stack inspector halts/resets the target.
                subprocess.run(["amidi", "-p", args.midi_port, "-S", "FC"], check=True)
                time.sleep(.5)  # allow the foreground loop to consume transport/selection messages
                deadline = time.monotonic() + 10
                idle_samples = 0
                while True:
                    maps = request(socket_path, "maps.status")["data"]
                    idle_samples = idle_samples + 1 if not maps["pending_job"] else 0
                    if idle_samples >= 3:
                        break
                    if time.monotonic() > deadline:
                        raise RuntimeError("media work pending; refusing stack halt/reset")
                    time.sleep(.1)
        finally:
            server.terminate()
            server.wait(timeout=10)
    # Audio recording has ended. This separate operation intentionally stops the CPU.
    if not args.skip_stack:
        subprocess.run([sys.executable, str(root / "scripts/zeptocore_stack_report.py"),
                        "--elf", str(args.elf), "--out", str(args.out / "stacks")], check=True)


if __name__ == "__main__":
    main()
