#!/usr/bin/env python3
"""Bounded ELF/SWD reader and decoding for the zeptocore diagnostics ABI."""
from __future__ import annotations

import hashlib
import json
from pathlib import Path
import socket
import struct
import time

MAGIC = 0x5A445031
ABI = 1
SCHEMA = "zeptocore.debug-command"
BIN_UPPER_US = [4, 16, 64, 128, 256, 512, 1024, 2048, 2903, 4096,
                5805, 8192, 10000, 20000, 100000, 0xFFFFFFFF]
AUDIO_METRICS = ["seek", "read", "stretch_seek", "stretch_read", "open", "close", "callback"]
CONTROL_METRICS = ["open", "close", "startup"]
IRQ_METRICS = ["dma_interval", "notification_wait"]
AUDIO_COUNTERS = ["producer_misses", "intentional_mute", "error_silence", "seeks_skipped",
                  "callback_overruns", "rendered_frames", "forward_seeks", "backward_seeks",
                  "same_position_seeks", "saturated", "publish_max_us", "last_error",
                  "file_generation", "reserved13", "reserved14", "reserved15"]
IRQ_COUNTERS = ["dma_starts", "starvation_count", "starvation_frames", "priming_count",
                "priming_frames", "notification_timeouts", "consecutive_timeouts", "saturated",
                "publish_max_us"]


class DiagnosticError(RuntimeError):
    pass


def words(data: bytes) -> list[int]:
    if len(data) % 4:
        raise DiagnosticError("unaligned ABI data")
    return list(struct.unpack(f"<{len(data)//4}I", data))


class ElfImage:
    """Only little-endian ARM ELF32, with bounds checks and no external packages."""
    def __init__(self, path: str | Path):
        self.path = Path(path).resolve()
        self.data = self.path.read_bytes()
        if self.data[:7] != b"\x7fELF\x01\x01\x01" or len(self.data) < 52:
            raise DiagnosticError("expected little-endian ELF32")
        header = struct.unpack_from("<HHIIIIIHHHHHH", self.data, 16)
        if header[1] != 40:
            raise DiagnosticError("expected ARM ELF")
        offset, size, count = header[5], header[10], header[11]
        if size != 40 or count == 0 or count > 4096:
            raise DiagnosticError("invalid ELF section table")
        self.sections = [struct.unpack("<10I", self.slice(offset + i*size, size))
                         for i in range(count)]
        self.symbols: dict[str, tuple[int, int]] = {}
        for section in self.sections:
            if section[1] != 2:
                continue
            if section[6] >= count or section[9] != 16:
                raise DiagnosticError("invalid symbol table")
            strings = self.sections[section[6]]
            table = self.slice(strings[4], strings[5])
            for pos in range(0, section[5], 16):
                name, addr, length, _, _, _ = struct.unpack(
                    "<IIIBBH", self.slice(section[4]+pos, 16))
                if name >= len(table):
                    raise DiagnosticError("invalid ELF symbol name")
                symbol = table[name:].split(b"\0", 1)[0].decode("utf-8", errors="replace")
                self.symbols[symbol] = (addr, length)

    def slice(self, offset: int, count: int) -> bytes:
        if offset < 0 or count < 0 or offset + count > len(self.data):
            raise DiagnosticError("truncated ELF")
        return self.data[offset:offset+count]

    def at(self, address: int, count: int) -> bytes:
        for s in self.sections:
            if s[1] != 8 and s[3] <= address and address + count <= s[3]+s[5]:
                return self.slice(s[4]+address-s[3], count)
        raise DiagnosticError("address is not initialized ELF data")

    @property
    def sha256(self) -> str:
        return hashlib.sha256(self.data).hexdigest()


class OpenOcdRpc:
    def __init__(self, port: int, timeout: float = 2, log=None):
        self.sock = socket.create_connection(("127.0.0.1", port), timeout)
        self.sock.settimeout(timeout)
        self.log = log

    def close(self):
        self.sock.close()

    def command(self, text: str) -> str:
        if self.log:
            self.log.write(json.dumps({"time": time.time(), "command": text}) + "\n")
            self.log.flush()
        self.sock.sendall(text.encode("ascii") + b"\x1a")
        response = bytearray()
        while True:
            chunk = self.sock.recv(4096)
            if not chunk:
                raise DiagnosticError("OpenOCD disconnected")
            response.extend(chunk)
            if len(response) > 65536:
                raise DiagnosticError("oversized OpenOCD response")
            if b"\x1a" in chunk:
                return bytes(response).split(b"\x1a", 1)[0].decode("utf-8")

    def running(self):
        states = []
        for core in range(2):
            # Poll updates OpenOCD's state cache without requesting a halt.
            self.command(f"rp2040.core{core} arp_poll")
            states.append(self.command(f"rp2040.core{core} curstate").strip())
        if states != ["running", "running"]:
            raise DiagnosticError(f"target is not running: {states}")
        return states

    def read(self, address: int, count: int) -> list[int]:
        if count <= 0 or count > 64 or address % 4:
            raise DiagnosticError("invalid SWD read size")
        result = self.command(f"read_memory 0x{address:x} 32 {count}").strip()
        try:
            data = [int(s, 0) for s in result.split()]
        except ValueError as exc:
            raise DiagnosticError(f"SWD read failed: {result}") from exc
        if len(data) != count or any(v < 0 or v > 0xFFFFFFFF for v in data):
            raise DiagnosticError(f"invalid SWD read response: {result}")
        return data

    def write_request(self, address: int, data: list[int]):
        if len(data) > 8 or any(v < 0 or v > 0xFFFFFFFF for v in data):
            raise DiagnosticError("invalid request words")
        values = " ".join(str(v) for v in data)
        result = self.command(f"write_memory 0x{address:x} 32 {{{values}}}").strip()
        if result:
            raise DiagnosticError(f"mailbox write failed: {result}")


def metric(w: list[int]) -> dict:
    if len(w) != 24:
        raise DiagnosticError("invalid metric length")
    return {"count": w[0], "total_us": w[1] | w[2] << 32, "max_us": w[3],
            "errors": w[4], "short_reads": w[5], "bytes": w[6] | w[7] << 32,
            "histogram": w[8:]}


def decode_layout(data: list[int], file_id: int) -> dict:
    if len(data) != 120 or data[0] != 0x314D4C43 or data[1] not in (0, 1) or data[2] != file_id:
        raise DiagnosticError("invalid cache-layout response")
    result = {"available": bool(data[1]), "file_id": file_id,
              "path": f"bank{(file_id >> 12)+1}/{(file_id >> 8)&15}.{file_id&255}.wav",
              "mount_generation": data[3], "source": "current validated RAM cache; no filesystem I/O"}
    if not data[1]:
        return result
    count, cluster_sectors, fat_entries = data[7:10]
    table = data[12:12+count]
    if not 2 <= count <= 64 or count % 2 or table[0] != count or table[-1] != 0 or not cluster_sectors:
        raise DiagnosticError("invalid cached map bounds")
    fragments = []
    for length, start in zip(table[1:-1:2], table[2:-1:2]):
        if not length or start < 2 or start+length > fat_entries:
            raise DiagnosticError("invalid cached extent")
        fragments.append({"starting_cluster": start, "clusters": length})
    size = data[4] | data[5] << 32
    if sum(f["clusters"] for f in fragments) != (size+cluster_sectors*512-1)//(cluster_sectors*512):
        raise DiagnosticError("cached map does not cover file size")
    result.update(file_size=size, starting_cluster=data[6], table_words=table,
                  fragments=fragments, fragment_count=len(fragments), cluster_sectors=cluster_sectors,
                  validation_policy=data[10])
    return result


def decode_domain(domain: int, data: list[int], features: int = 0) -> dict:
    names = [AUDIO_METRICS, CONTROL_METRICS, IRQ_METRICS][domain]
    n = len(names)*24
    result = {"metrics": {name: metric(data[i*24:(i+1)*24])
                           for i, name in enumerate(names)}}
    counters = [AUDIO_COUNTERS, [f"counter_{i}" for i in range(16)], IRQ_COUNTERS][domain]
    result["counters"] = dict(zip(counters, data[n:n+16]))
    if domain == 2 and features & 64:
        result["counters"].update(zip([
            "switch_completed", "switch_last_us", "switch_max_us", "switch_requested_token",
            "switch_completed_token", "switch_unmeasured_overlap", "switch_expired"], data[n+9:n+16]))
    if domain == 0 and features & 8:
        for index, name in [(13, "map_hits"), (14, "map_misses"), (15, "media_withheld_callbacks")]:
            result["counters"][name] = result["counters"].pop(f"reserved{index}")
    if domain == 0:
        result["context_words"] = data[n+16:n+32]
        result["worst_seek_words"] = data[n+32:n+44]
        c = result["context_words"]
        result["playback"] = {"bank": c[0], "sample": c[1], "variation": c[2],
                              "audio_variant": c[3], "forward": bool(c[4]),
                              "stretch_q8": c[5], "pitch_index": c[6], "bpm": c[7],
                              "effects_mask": c[8], "file_size": c[9] | c[10] << 32,
                              "starting_cluster": c[11], "file_position": c[12] | c[13] << 32,
                              "mapped": bool(c[14]), "file_open": bool(c[15] & 1),
                              "media_withheld": bool(c[15] & 2)}
    if domain == 1:
        result["context_words"] = data[n+16:n+48]
        c = result["context_words"]
        result["card"] = {"cid_hex": struct.pack("<4I", *c[4:8]).hex(),
                          "sectors": c[8], "fatfs_type": c[9], "cluster_sectors": c[10],
                          "volume_lba": c[11], "fat_lba": c[12], "data_lba": c[13],
                          "fat_entries": c[14]}
        if features & 8:
            counter_names = {0: "mount_generation", 1: "files", 2: "builds", 3: "reused",
                             4: "validated", 5: "invalid", 6: "oversized", 7: "loads",
                             8: "evictions", 11: "index_writes", 13: "commits",
                             14: "failures", 15: "capacity_failures"}
            context_names = {3: "validation_us", 15: "build_us", 21: "load_us",
                             22: "commit_us", 23: "prepare_us", 24: "retained_bytes",
                             25: "reserved_bytes", 26: "index_generation", 27: "pending_job",
                             28: "last_error", 29: "ownership_acquisitions",
                             30: "ownership_timeouts", 31: "ownership_wait_max_us"}
            result["maps"] = {name: data[n+index] for index, name in counter_names.items()}
            result["maps"].update({name: c[index] for index, name in context_names.items()})
            result["maps"]["attachment_enabled"] = bool(features & 16)
    return result


class DeviceReader:
    def __init__(self, elf: ElfImage, rpc, timeout=2):
        self.elf, self.rpc, self.timeout = elf, rpc, timeout
        if "zeptocore_diag" not in elf.symbols:
            raise DiagnosticError("ELF has no diagnostics; build with SEEK_DIAGNOSTICS=ON")
        self.base, self.size = elf.symbols["zeptocore_diag"]
        if not (0x20000000 <= self.base < self.base + self.size <= 0x20040000):
            raise DiagnosticError("diagnostic mailbox is outside main SRAM")
        self.expected = words(elf.at(self.base, 128))
        if self.expected[:4] != [MAGIC, ABI, self.size, 128]:
            raise DiagnosticError("unsupported ELF diagnostic ABI")
        # Fixed ABI v1 layout; never trust device-provided arbitrary write addresses.
        self.offsets = [160, 1040, 1552]
        if self.size != 1840 or self.expected[24:31] != [128, *self.offsets, 848, 480, 256]:
            raise DiagnosticError("invalid ELF mailbox layout")
        self.session = None
        self.last_header = None
        self.pending_sequence = None

    def validate(self) -> list[int]:
        self.rpc.running()
        h = self.rpc.read(self.base, 32)
        indices = [0, 1, 2, 3, 4, 6, 7, *range(8, 16), *range(24, 31)]
        if any(h[i] != self.expected[i] for i in indices):
            raise DiagnosticError("firmware/ELF identity mismatch; no mailbox writes issued")
        if h[21] == 0:
            raise DiagnosticError("diagnostics not initialized")
        current = tuple(h[16:18])
        if self.session is not None and self.session != current:
            self.session = current
            self.pending_sequence = None
            raise DiagnosticError("device rebooted; capture deltas must start a new session")
        self.session = current
        self.last_header = h
        return h

    def audio_fixture(self) -> dict:
        h = self.validate()
        if not h[4] & 1024:
            return {"available": False, "reason": "firmware has no audio fixture"}
        address, size = self.elf.symbols.get("zeptocore_audio_fixture", (0, 0))
        if size != 32 or address % 4 or not 0x20000000 <= address < address+size <= 0x20040000:
            raise DiagnosticError("invalid audio fixture symbol")
        data = self.rpc.read(address, 8)
        if data != self.rpc.read(address, 8):
            raise DiagnosticError("audio fixture changed during read")
        self.validate()
        if data[0] != 0x31544641:
            return {"available": False, "reason": "audio fixture has not completed"}
        if data[2] not in (1, 2, 3, 4) or data[3] > 4 or data[7]:
            raise DiagnosticError("invalid audio fixture report")
        return {"available": True, "valid": data[1] == 0 and data[3] != 0,
                "result": data[1], "mode": data[2],
                "operation": ["failed", "created", "reused", "removed", "absent"][data[3]],
                "bytes": data[4], "starting_cluster": data[5], "elapsed_us": data[6],
                "path": "bank1/0.3.wav" if data[2] > 2 else "bank1/0.2.wav",
                "source": "completed automatic test boot work; no request-triggered I/O"}

    def media_cached(self) -> dict:
        """Read a fixed set of existing cached UI globals, never the filesystem."""
        self.validate()
        fields = {"audio_variant_num": 1, "total_number_samples": 2,
                  "savefile_current": 1, "savefile_has_data": 16}
        def read():
            values = {}
            for name, size in fields.items():
                symbol = self.elf.symbols.get(name)
                if symbol is None:
                    return None
                address, actual = symbol
                if actual != size or not 0x20000000 <= address < address+size <= 0x20040000:
                    raise DiagnosticError("invalid cached media symbol")
                aligned = address & ~3
                data = self.rpc.read(aligned, (address-aligned+size+3)//4)
                raw = b"".join(v.to_bytes(4, "little") for v in data)[address-aligned:address-aligned+size]
                values[name] = list(raw) if size == 16 else int.from_bytes(raw, "little")
            return values
        first, second = read(), read()
        self.validate()
        if first != second:
            raise DiagnosticError("cached media state changed during read; retry")
        return {"available": first is not None, "values": first, "session": list(self.session),
                "source": "existing cached UI state, not a card scan"}

    def benchmark(self) -> dict:
        """Read the test-only immutable boot report; never request device work."""
        self.validate()
        symbol = self.elf.symbols.get("zeptocore_seek_benchmark")
        if symbol is None:
            return {"available": False, "reason": "firmware has no boot benchmark"}
        address, size = symbol
        if size != 512 or address % 4 or not 0x20000000 <= address < address+size <= 0x20040000:
            raise DiagnosticError("invalid benchmark report symbol")
        if self.rpc.read(address, 1) != [0x31424D53]:
            return {"available": False, "reason": "boot benchmark has not completed"}
        data = self.rpc.read(address, 64) + self.rpc.read(address+256, 64)
        repeated = self.rpc.read(address, 64) + self.rpc.read(address+256, 64)
        self.validate()
        if data != repeated or data[:2] != [0x31424D53, 1] or any(data[16:32]):
            raise DiagnosticError("incoherent or unsupported benchmark report")
        names = ["result", "cluster_sectors", "file_size", "fragment_count", "table_words",
                 "pairs", "created", "build_us", "setup_us", "ordinary_fat_visits",
                 "mapped_fat_visits", "verified_bytes", "saturation", "starting_cluster"]
        result = dict(zip(names, data[2:16]))
        result.update(available=True, valid=data[2] == 0 and data[7] == 768 and data[14] == 0,
                      scope="boot filesystem microbenchmark; no synthetic audio playback",
                      session=list(self.session), elf_sha256=self.elf.sha256,
                      metrics={name: metric(data[32+i*24:56+i*24]) for i, name in enumerate(
                          ["ordinary_seek", "ordinary_read", "mapped_seek", "mapped_read"])})
        return result

    def snapshot(self, *, layout_id: int | None = None) -> dict:
        h = self.validate()
        if layout_id is not None and (type(layout_id) is not int or not 0 <= layout_id <= 65535 or not h[4] & 128):
            raise DiagnosticError("unsupported cache-layout request")
        desired = [1, 0, 0, 0, 0, 0, 0] if layout_id is None else [2, layout_id, 0, 0, 0, 0, 0]
        previous = self.rpc.read(self.base + 128, 1)[0]
        # Recover an outstanding snapshot after a timeout or host disconnect.
        # Never overwrite a request while a slow foreground writer still owes
        # its response. Each writer keeps accumulating while the host is absent.
        outstanding = previous and (self.pending_sequence == previous or any(
            self.rpc.read(self.base+offset, 1)[0] != previous for offset in self.offsets))
        if outstanding:
            actual = self.rpc.read(self.base+132, 7)
            if actual != [1, 0, 0, 0, 0, 0, 0] and not (
                    h[4] & 128 and actual[0] == 2 and actual[1] <= 65535 and actual[2:] == [0]*5):
                raise DiagnosticError("unrecognized outstanding request; target left unchanged")
            seq = previous
        else:
            # Commit only after payload, never write live statistics or reset counters.
            seq = (previous + 1) & 0xFFFFFFFF or 1
            actual = desired
            self.rpc.write_request(self.base+132, actual)
            self.rpc.write_request(self.base+128, [seq])
        self.pending_sequence = seq
        deadline = time.monotonic() + self.timeout
        pending = set(range(3))
        heads = {}
        while pending:
            for domain in list(pending):
                header = self.rpc.read(self.base+self.offsets[domain], 8)
                if header[0] == seq:
                    if header[1] != 1 or header[4] != h[28+domain] or header[5] != seq:
                        raise DiagnosticError(f"invalid domain {domain} acknowledgement")
                    heads[domain] = header
                    pending.remove(domain)
            if not pending:
                break
            if time.monotonic() >= deadline:
                raise DiagnosticError(f"snapshot timeout; pending domains={sorted(pending)}; "
                                      f"last progress={h[18:21]}, stage={h[21]}")
            time.sleep(0.01)
        if actual != desired:
            # Finish a timed-out request of the other kind before issuing this
            # request. Domain payloads remain frozen until the next request.
            self.pending_sequence = None
            return self.snapshot(layout_id=layout_id)
        domains = {}
        for domain in range(3):
            address = self.base+self.offsets[domain]
            count = heads[domain][4]//4
            data = []
            for start in range(0, count, 64):
                data.extend(self.rpc.read(address+32+start*4, min(64, count-start)))
            after = self.rpc.read(address, 8)
            if after != heads[domain]:
                raise DiagnosticError("snapshot changed during retrieval")
            item = (decode_layout(data, layout_id) if domain == 1 and layout_id is not None else
                    decode_domain(domain, data, features=h[4]))
            item.update(timestamp_us=after[2], progress=after[3], publish_us=after[6])
            domains[["audio", "control", "irq"][domain]] = item
        final = self.validate()
        if final[16:18] != h[16:18]:
            raise DiagnosticError("device reset during snapshot")
        self.pending_sequence = None
        return {"sequence": seq, "session": list(self.session), "host_time": time.time(),
                "device": {"build_id": "".join(f"{v:08x}" for v in h[8:16]),
                           "clock_hz": h[5], "nominal_sample_rate": h[6], "frames": h[7],
                           "features": h[4],
                           "actual_sample_rate_millihz": h[22], "pio_divider": h[23],
                           "block_budget_us": h[31], "stage": h[21], "abi": h[1]},
                "histogram_upper_us": BIN_UPPER_US, **domains}
