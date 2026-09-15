#!/usr/bin/env python3
"""Request diagnostics or capture a reproducible host-side measurement window."""
from __future__ import annotations

import argparse
import csv
import json
import math
from pathlib import Path
import socket
import time

from zeptocore_debug_protocol import SCHEMA, BIN_UPPER_US
from zeptocore_debug_server import DEFAULT_SOCKET


def request(path, command, **fields):
    identity = str(time.time_ns())
    msg = {"schema": SCHEMA, "version": 1, "id": identity, "command": command, **fields}
    with socket.socket(socket.AF_UNIX) as sock:
        sock.settimeout(10)
        sock.connect(path)
        sock.sendall((json.dumps(msg)+"\n").encode())
        with sock.makefile("rb") as stream:
            raw = stream.readline(65537)
    if len(raw) > 65536 or not raw.endswith(b"\n"):
        raise RuntimeError("invalid server response")
    response = json.loads(raw)
    if response.get("id") != identity or response.get("command") != command:
        raise RuntimeError("response identity mismatch")
    if response.get("status") != "ok":
        raise RuntimeError(response.get("error", "request failed"))
    return response


def percentile_range(histogram, q):
    count = sum(histogram)
    if not count:
        return None
    rank = max(1, math.ceil(count*q))
    total = 0
    for i, n in enumerate(histogram):
        total += n
        if total >= rank:
            return [0 if i == 0 else BIN_UPPER_US[i-1]+1,
                    None if i == len(histogram)-1 else BIN_UPPER_US[i]]
    raise ValueError("invalid histogram")


def summarize(snapshots):
    if len(snapshots) < 2:
        return {"valid": False, "reason": "fewer than two snapshots"}
    first, last = snapshots[0], snapshots[-1]
    if any(s["session"] != first["session"] or s["device"] != first["device"] for s in snapshots):
        return {"valid": False, "reason": "session or device configuration changed"}
    result = {"valid": True, "snapshots": len(snapshots),
              "elapsed_host_seconds": last["host_time"]-first["host_time"], "domains": {}}
    gaps = [b["host_time"]-a["host_time"] for a, b in zip(snapshots, snapshots[1:])]
    result["effective_snapshot_hz"] = (len(snapshots)-1)/sum(gaps) if sum(gaps) > 0 else None
    result["largest_snapshot_gap_seconds"] = max(gaps)
    for domain in ("audio", "control", "irq"):
        if first[domain]["counters"].get("saturated", first[domain]["counters"].get("counter_9", 0)) or \
                last[domain]["counters"].get("saturated", last[domain]["counters"].get("counter_9", 0)):
            return {"valid": False, "reason": f"{domain} counters saturated"}
        values = {}
        for name, b in last[domain]["metrics"].items():
            a = first[domain]["metrics"][name]
            hist = [y-x for x, y in zip(a["histogram"], b["histogram"])]
            delta = {key: b[key]-a[key] for key in ("count", "total_us", "errors", "short_reads", "bytes")}
            if any(v < 0 for v in [*delta.values(), *hist]) or sum(hist) != delta["count"]:
                return {"valid": False, "reason": "counter regression or inconsistent histogram"}
            delta.update(mean_us=delta["total_us"]/delta["count"] if delta["count"] else None,
                         p50_us_range=percentile_range(hist, .50), p95_us_range=percentile_range(hist, .95),
                         p99_us_range=percentile_range(hist, .99), lifetime_max_us=b["max_us"])
            values[name] = delta
        result["domains"][domain] = {"metrics": values, "counters_final": last[domain]["counters"]}
    # These cumulative DMA counters specifically count missing output buffers.
    result["starvation_count"] = (last["irq"]["counters"]["starvation_count"] -
                                  first["irq"]["counters"]["starvation_count"])
    result["starvation_frames"] = (last["irq"]["counters"]["starvation_frames"] -
                                   first["irq"]["counters"]["starvation_frames"])
    if "switch_completed" in last["irq"]["counters"]:
        measured, missed = [], 0
        for a, b in zip(snapshots, snapshots[1:]):
            old, new = a["irq"]["counters"], b["irq"]["counters"]
            count = new["switch_completed"]-old["switch_completed"]
            if count > 0:
                measured.append(new["switch_last_us"])
                missed += count-1
        ordered = sorted(measured)
        result["sample_switch"] = {
            "observations_us": measured, "unobserved_completions": missed,
            "p50_us": ordered[math.ceil(len(ordered)*.5)-1] if ordered else None,
            "p99_us": ordered[math.ceil(len(ordered)*.99)-1] if ordered else None,
            "max_observed_us": max(ordered) if ordered else None,
            "overlapping_unmeasured_requests": last["irq"]["counters"]["switch_unmeasured_overlap"]-
                first["irq"]["counters"]["switch_unmeasured_overlap"],
            "expired_requests": last["irq"]["counters"]["switch_expired"]-
                first["irq"]["counters"]["switch_expired"],
            "pending": last["irq"]["counters"]["switch_requested_token"] !=
                last["irq"]["counters"]["switch_completed_token"],
            "scope": "MIDI sample-selection handler to first DMA buffer carrying the selected source, "
                     "plus its frame offset at the actual I2S rate; excludes codec/analog delay."}
    return result


def show(snapshot):
    d = snapshot["device"]
    print(f"frames={d['frames']} clock={d['clock_hz']} Hz "
          f"I2S={d['actual_sample_rate_millihz']/1000:.3f} Hz stage={d['stage']}")
    for name, m in snapshot["audio"]["metrics"].items():
        print(f"{name:15} count={m['count']:8} max={m['max_us']:8} us errors={m['errors']}")
    print("output:", json.dumps(snapshot["irq"]["counters"], sort_keys=True))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--socket", default=DEFAULT_SOCKET)
    sub = parser.add_subparsers(dest="action", required=True)
    status = sub.add_parser("status")
    status.add_argument("--once", action="store_true")
    status.add_argument("--json", action="store_true")
    watch = sub.add_parser("watch")
    watch.add_argument("--interval", type=float, default=1)
    capture = sub.add_parser("capture")
    capture.add_argument("--duration", type=float, default=60)
    capture.add_argument("--interval", type=float, default=1)
    capture.add_argument("--out", type=Path, required=True)
    capture.add_argument("--workload", default="manual/exploratory")
    args = parser.parse_args()
    if args.action == "status":
        data = request(args.socket, "seek.status")["data"]
        if args.json:
            print(json.dumps(data, indent=2))
        else:
            show(data)
        return
    if args.interval < .25 or args.interval > 60:
        parser.error("interval must be between 0.25 and 60 seconds; default is 1 Hz")
    if args.action == "watch":
        while True:
            show(request(args.socket, "seek.status")["data"])
            time.sleep(args.interval)
    if args.duration <= 0:
        parser.error("duration must be positive")
    args.out.mkdir(parents=True, exist_ok=False)
    start = request(args.socket, "capture.start", metadata={"workload": args.workload,
                    "poll_interval": args.interval, "duration_requested": args.duration})
    snapshots = [start["data"]]
    deadline = time.monotonic()+args.duration
    next_poll = time.monotonic()+args.interval
    failure = None
    try:
        with (args.out / "snapshots.jsonl").open("w") as output:
            output.write(json.dumps(snapshots[0])+"\n")
            while time.monotonic() < deadline:
                time.sleep(max(0, min(next_poll, deadline)-time.monotonic()))
                data = request(args.socket, "seek.status")["data"]
                snapshots.append(data)
                output.write(json.dumps(data)+"\n")
                output.flush()
                next_poll += args.interval
                if next_poll < time.monotonic():
                    next_poll = time.monotonic()+args.interval
    except (RuntimeError, OSError) as exc:
        failure = str(exc)
    finally:
        try:
            stopped = request(args.socket, "capture.stop")
        except (RuntimeError, OSError) as exc:
            stopped = {"error": str(exc)}
    result = summarize(snapshots)
    if failure:
        result.update(valid=False, error=failure)
    (args.out / "summary.json").write_text(json.dumps(result, indent=2)+"\n")
    (args.out / "capture.json").write_text(json.dumps({"start": start["capture"], "stop": stopped}, indent=2)+"\n")
    source_manifest = Path(start["capture"]["path"]) / "manifest.json"
    (args.out / "manifest.json").write_bytes(source_manifest.read_bytes())
    retained_elf = source_manifest.with_name("firmware.elf")
    if retained_elf.exists():
        (args.out / "firmware.elf").write_bytes(retained_elf.read_bytes())
    with (args.out / "metrics.csv").open("w", newline="") as output:
        writer = csv.writer(output)
        writer.writerow(["domain", "operation", "count", "mean_us", "p99_us_range", "lifetime_max_us", "errors"])
        for domain, v in result.get("domains", {}).items():
            for name, metric in v["metrics"].items():
                writer.writerow([domain, name, metric["count"], metric["mean_us"], metric["p99_us_range"],
                                 metric["lifetime_max_us"], metric["errors"]])
    print(json.dumps(result, indent=2))
    if not result["valid"]:
        raise SystemExit(1)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt:
        pass
