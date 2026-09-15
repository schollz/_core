#!/usr/bin/env python3
"""Local request/response service for running-target zeptocore diagnostics."""
from __future__ import annotations

import argparse
import json
import os
from pathlib import Path
import signal
import socket
import socketserver
import subprocess
import time

from zeptocore_debug_protocol import DiagnosticError, DeviceReader, ElfImage, OpenOcdRpc, SCHEMA

DEFAULT_SOCKET = f"/tmp/zeptocore-debug-{os.getuid()}.sock"
MAX_REQUEST = 4096


class DebugService:
    def __init__(self, reader, artifact_dir: Path):
        self.reader = reader
        self.artifact_dir = artifact_dir
        self.capture = None

    def handle(self, request: dict) -> dict:
        if not isinstance(request, dict):
            raise DiagnosticError("request must be an object")
        identity, command = request.get("id"), request.get("command")
        if request.get("schema") != SCHEMA or request.get("version") != 1:
            raise DiagnosticError("unsupported command schema/version")
        if not isinstance(identity, str) or not 1 <= len(identity) <= 64:
            raise DiagnosticError("id must be 1..64 characters")
        response = {"schema": "zeptocore.debug-response", "version": 1,
                    "id": identity, "command": command, "status": "ok"}
        if not isinstance(command, str) or command not in {"device.info", "seek.status", "audio.status", "memory.status",
                            "maps.status", "maps.layout", "maps.benchmark", "maps.fixture", "media.cached", "capture.start", "capture.stop"}:
            raise DiagnosticError("unsupported command")
        if command == "maps.fixture":
            response["data"] = self.reader.audio_fixture()
            if not response["data"]["available"]:
                response["status"] = "unavailable"
            return response
        if command == "media.cached":
            response["data"] = self.reader.media_cached()
            if not response["data"]["available"]:
                response["status"] = "unavailable"
            return response
        if command == "maps.benchmark":
            response["data"] = self.reader.benchmark()
            if not response["data"]["available"]:
                response["status"] = "unavailable"
            return response
        if command == "maps.status":
            if not self.reader.validate()[4] & 8:
                response.update(status="unavailable", error="firmware has no seek-map diagnostics")
                return response
            snapshot = self.reader.snapshot()
            response["data"] = snapshot["control"]["maps"]
            response["data"].update(sequence=snapshot["sequence"],
                                    timestamp_us=snapshot["control"]["timestamp_us"])
            return response
        if command == "maps.layout":
            if not self.reader.validate()[4] & 128:
                response.update(status="unavailable", error="firmware has no cache-layout response")
                return response
            file_id = request.get("file_id")
            if type(file_id) is not int or not 0 <= file_id <= 65535:
                raise DiagnosticError("file_id must be an integer in 0..65535")
            snapshot = self.reader.snapshot(layout_id=file_id)
            response["data"] = snapshot["control"]
            response["data"].update(sequence=snapshot["sequence"], session=snapshot["session"])
            return response
        if command == "capture.start":
            if self.capture:
                raise DiagnosticError("capture already active")
            if not isinstance(request.get("metadata", {}), dict):
                raise DiagnosticError("capture metadata must be an object")
            capture_id = time.strftime("%Y%m%d-%H%M%S") + f"-{time.time_ns() % 1000000:06d}"
            destination = self.artifact_dir / capture_id
            destination.mkdir(parents=True, exist_ok=False)
            (destination / "firmware.elf").write_bytes(self.reader.elf.data)
            manifest = {"capture_id": capture_id, "elf": str(self.reader.elf.path),
                        "elf_sha256": self.reader.elf.sha256,
                        "retained_elf": "firmware.elf",
                        "started": time.time(), "metadata": request.get("metadata", {}),
                        "unavailable": ["file_allocation_layout", "stack_high_water",
                                        "external_audio_capture_unless_recorded_separately"]}
            # Read-only revision context; retain dirty source fingerprints through build ID.
            for key, args in [("revision", ["git", "rev-parse", "HEAD"]),
                               ("dirty_state", ["git", "status", "--porcelain"])]:
                result = subprocess.run(args, capture_output=True, text=True, check=False)
                manifest[key] = result.stdout.strip() if result.returncode == 0 else None
            (destination / "manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")
            self.capture = {"id": capture_id, "path": destination, "count": 0,
                            "file": (destination / "snapshots.jsonl").open("w")}
        if command == "capture.stop":
            if not self.capture:
                raise DiagnosticError("no active capture")
            capture = self.capture
            self.capture = None
            capture["file"].close()
            response["data"] = {"capture_id": capture["id"], "path": str(capture["path"]),
                                "snapshots": capture["count"]}
            return response
        try:
            snapshot = self.reader.snapshot()
        except (DiagnosticError, OSError):
            if command == "capture.start" and self.capture:
                self.capture["file"].close()
                self.capture = None
            raise
        if self.capture and command == "capture.start":
            manifest_path = self.capture["path"] / "manifest.json"
            manifest = json.loads(manifest_path.read_text())
            manifest.update(device=snapshot["device"], session=snapshot["session"],
                            playback=snapshot["audio"].get("playback"),
                            card=snapshot["control"].get("card"))
            if snapshot["device"]["features"] & 128:
                playback = snapshot["audio"]["playback"]
                file_id = (playback["bank"] << 12) | (playback["sample"] << 8) | (
                    playback["variation"] + playback["audio_variant"]*2)
                try:
                    layout = self.reader.snapshot(layout_id=file_id)["control"]
                except (DiagnosticError, OSError):
                    self.capture["file"].close()
                    self.capture = None
                    raise
                if layout["available"] and (playback["media_withheld"] or
                        layout["file_size"] != playback["file_size"] or
                        layout["starting_cluster"] != playback["starting_cluster"] or
                        layout["mount_generation"] != snapshot["control"]["maps"]["mount_generation"]):
                    layout.update(available=False, reason="cache and playback snapshots describe different file states")
                manifest["file_allocation_layout"] = layout
                if layout["available"]:
                    manifest["unavailable"].remove("file_allocation_layout")
            manifest_path.write_text(json.dumps(manifest, indent=2)+"\n")
        if self.capture:
            self.capture["file"].write(json.dumps(snapshot, separators=(",", ":"))+"\n")
            self.capture["file"].flush()
            self.capture["count"] += 1
        if command == "device.info":
            response["data"] = {**snapshot["device"], "session": snapshot["session"]}
        elif command == "memory.status":
            context = snapshot["control"]["context_words"]
            packed = bool(snapshot["device"]["features"] & 32)
            response["data"] = {"heap_total_bytes": context[0] & 0x00ffffff if packed else context[0], "heap_free_at_startup_bytes": context[1],
                                "heap_free_after_controls_init_bytes": context[2] or None,
                                "reverb_combs": (context[0] >> 24) & 15 if packed else None,
                                "reverb_allpasses": context[0] >> 28 if packed else None,
                                "runtime_stack_headroom": None, "source": "startup mallinfo",
                                "session": snapshot["session"]}
        else:
            response["data"] = snapshot
        if command == "capture.start":
            response["capture"] = {"id": self.capture["id"], "path": str(self.capture["path"])}
        return response


class RequestHandler(socketserver.StreamRequestHandler):
    def handle(self):
        self.request.settimeout(5)
        raw = self.rfile.readline(MAX_REQUEST+1)
        request = {}
        try:
            if len(raw) > MAX_REQUEST or not raw.endswith(b"\n"):
                raise DiagnosticError("request too long or missing newline")
            request = json.loads(raw)
            response = self.server.service.handle(request)
        except (DiagnosticError, ValueError, OSError) as exc:
            response = {"schema": "zeptocore.debug-response", "version": 1,
                        "id": request.get("id") if isinstance(request, dict) else None,
                        "command": request.get("command") if isinstance(request, dict) else None,
                        "status": "error", "error": str(exc)}
        try:
            self.wfile.write((json.dumps(response, separators=(",", ":"))+"\n").encode())
        except (BrokenPipeError, ConnectionResetError):
            pass  # The target never waits for client reception.


def main():
    def interrupted(signum, frame):
        raise KeyboardInterrupt
    signal.signal(signal.SIGTERM, interrupted)
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--elf", required=True)
    parser.add_argument("--socket", default=DEFAULT_SOCKET)
    parser.add_argument("--openocd", default="openocd")
    parser.add_argument("--probe-serial", default="E6632891E3889E30")
    parser.add_argument("--adapter-khz", type=int, default=1000)
    parser.add_argument("--artifacts", type=Path, default=Path("artifacts/seek/captures"))
    parser.add_argument("--rpc-port", type=int, help="Connect to an already-running OpenOCD")
    args = parser.parse_args()
    if not 100 <= args.adapter_khz <= 5000:
        parser.error("adapter speed must be 100..5000 kHz")
    if Path(args.socket).exists():
        parser.error("socket already exists; check the owning server before removing it")
    elf = ElfImage(args.elf)
    args.artifacts.mkdir(parents=True, exist_ok=True)
    process, rpc, server = None, None, None
    logs = []
    try:
        port = args.rpc_port
        if port is None:
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", 0))
                port = reservation.getsockname()[1]
            output = (args.artifacts / "openocd.log").open("a")
            logs.append(output)
            command = [args.openocd, "-f", "interface/cmsis-dap.cfg", "-f", "target/rp2040.cfg",
                       "-c", "bindto 127.0.0.1", "-c", "gdb_port disabled",
                       "-c", "telnet_port disabled", "-c", f"tcl_port {port}",
                       "-c", f"adapter speed {args.adapter_khz}"]
            if args.probe_serial:
                if not args.probe_serial.isalnum():
                    parser.error("invalid probe serial")
                command += ["-c", f"adapter serial {args.probe_serial}"]
            command += ["-c", "init"]
            output.write(json.dumps({"command": command})+"\n")
            output.flush()
            process = subprocess.Popen(command, stdout=output, stderr=subprocess.STDOUT)
        rpc_log = (args.artifacts / "rpc.jsonl").open("a")
        logs.append(rpc_log)
        deadline = time.monotonic()+5
        while True:
            try:
                rpc = OpenOcdRpc(port, log=rpc_log)
                break
            except OSError:
                if process and process.poll() is not None:
                    raise DiagnosticError("OpenOCD exited; see openocd.log")
                if time.monotonic() > deadline:
                    raise DiagnosticError("OpenOCD RPC connection timed out")
                time.sleep(0.1)
        reader = DeviceReader(elf, rpc)
        reader.validate()
        server = socketserver.UnixStreamServer(args.socket, RequestHandler)
        os.chmod(args.socket, 0o600)
        server.service = DebugService(reader, args.artifacts)
        print(f"Ready: {args.socket}", flush=True)
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        pass
    finally:
        if server:
            if server.service.capture:
                server.service.capture["file"].close()
            server.server_close()
            Path(args.socket).unlink(missing_ok=True)
        if rpc:
            rpc.close()
        if process:
            process.terminate()
            try:
                process.wait(timeout=3)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        for log in logs:
            log.close()


if __name__ == "__main__":
    main()
