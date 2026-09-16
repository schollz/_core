#!/usr/bin/env python3
"""Exercise deferred cache loads using normal MIDI stop/start; never flash or halt."""
import argparse
import json
from pathlib import Path
import subprocess
import sys
import time

from zeptocore_debug import request
from zeptocore_debug_protocol import ElfImage
from zeptocore_midi import wait_available


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--elf", required=True, type=Path)
    p.add_argument("--out", required=True, type=Path)
    p.add_argument("--midi-port", default="hw:6,0,0")
    args = p.parse_args()
    args.out.mkdir(parents=True, exist_ok=False)
    (args.out / "firmware.elf").write_bytes(ElfImage(args.elf).data)
    socket = str((args.out / "debug.sock").resolve())
    root = Path(__file__).resolve().parents[1]
    events = []

    def midi(packet):
        events.append({"host_time": time.time(), "midi_hex": packet})
        subprocess.run(["amidi", "-p", args.midi_port, "-S", packet], check=True)

    def snapshot(label):
        data = request(socket, "seek.status")["data"]
        (args.out / f"{label}.json").write_text(json.dumps(data, indent=2)+"\n")
        return data

    with (args.out / "server.log").open("w") as log:
        server = subprocess.Popen([sys.executable, str(root / "scripts/zeptocore_debug_server.py"),
            "--elf", str(args.elf), "--socket", socket, "--artifacts", str(args.out / "server")],
            stdout=log, stderr=subprocess.STDOUT)
        results = []
        try:
            deadline = time.monotonic()+10
            while not Path(socket).exists():
                if server.poll() is not None or time.monotonic()>deadline:
                    raise RuntimeError("server did not start")
                time.sleep(.1)
            wait_available(args.midi_port)
            midi("B01200B01600B01800B01900B01040B01500B01400B06E00B07000FA")
            time.sleep(.5)
            initial = snapshot("initial")
            if not initial["device"]["features"] & 16:
                raise RuntimeError("requires map attachment enabled")
            for step, value in enumerate([64, 32, 64, 0]):
                midi(f"B015{value:02X}FA")
                time.sleep(.5)
                playing = snapshot(f"{step}-playing")
                before = playing["control"]["maps"]
                playback = playing["audio"]["playback"]
                file_id = (playback["bank"]<<12)|(playback["sample"]<<8)|(
                    playback["variation"]+2*playback["audio_variant"])
                midi("FC")
                deadline = time.monotonic()+5
                while True:
                    stopped = request(socket, "seek.status")["data"]
                    if not stopped["control"]["maps"]["pending_job"]:
                        break
                    if time.monotonic()>deadline:
                        (args.out / f"{step}-timeout.json").write_text(json.dumps(stopped, indent=2)+"\n")
                        raise RuntimeError("queued load did not reach a quiet media window")
                    time.sleep(.1)
                (args.out / f"{step}-stopped.json").write_text(json.dumps(stopped, indent=2)+"\n")
                after = stopped["control"]["maps"]
                assert after["builds"]==before["builds"] and after["index_writes"]==before["index_writes"]
                assert after["loads"]-before["loads"] == (1 if before["pending_job"]==1 else 0)
                layout = request(socket, "maps.layout", file_id=file_id)["data"]
                assert layout["available"] and layout["file_size"]==playback["file_size"]
                (args.out / f"{step}-layout.json").write_text(json.dumps(layout, indent=2)+"\n")
                midi("FA");time.sleep(.3)
                resumed = snapshot(f"{step}-resumed")
                assert resumed["audio"]["playback"]["mapped"]
                assert resumed["irq"]["counters"]["starvation_count"]==initial["irq"]["counters"]["starvation_count"]
                results.append({"step": step, "path": layout["path"], "fragments": layout["fragment_count"],
                    "was_mapped_while_playing": playback["mapped"], "loaded": after["loads"]-before["loads"],
                    "builds": after["builds"]-before["builds"], "index_writes": after["index_writes"]-before["index_writes"],
                    "layout_publish_us": layout["publish_us"]})
                print(json.dumps(results[-1]), flush=True)
            (args.out / "summary.json").write_text(json.dumps({"valid": True, "steps": results}, indent=2)+"\n")
        finally:
            try:
                midi("B01500B01400B06E00B07000FA")
            finally:
                server.terminate();server.wait(timeout=10)
                (args.out / "controls.json").write_text(json.dumps(events, indent=2)+"\n")


if __name__ == "__main__":
    main()
