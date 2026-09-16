import copy
import json
from pathlib import Path
import struct
import sys
import tempfile
import socket
import socketserver
import threading
from concurrent.futures import ThreadPoolExecutor
import unittest

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "scripts"))
from zeptocore_debug_protocol import DeviceReader, DiagnosticError, decode_domain, decode_layout, ElfImage, MAGIC, words
from zeptocore_debug import percentile_range, summarize
from zeptocore_debug_server import DebugService, RequestHandler


class FakeElf:
    path = Path("fake.elf")
    sha256 = "test"
    data = b"fake ELF retained by test"
    symbols = {"zeptocore_diag": (0x20001000, 1840)}
    header = [MAGIC, 1, 1840, 128, 1, 0, 44100, 441,
              *range(1, 9), *([0]*8), 128, 160, 1040, 1552, 848, 480, 256, 0]

    def at(self, address, count):
        return struct.pack("<32I", *self.header)[:count]


class FakeRpc:
    def __init__(self):
        self.data = FakeElf.header + [0]*(1840//4-32)
        self.data[5] = 225000000
        self.data[16:18] = [123, 456]
        self.data[21] = 3
        self.data[22:24] = [44501582, 79]
        self.data[31] = 9910
        self.writes = []
        self.pending = False
        self.tear = False
        self.read_sizes = []

    def running(self):
        return ["running", "running"]

    def read(self, address, count):
        self.read_sizes.append(count)
        offset = (address-0x20001000)//4
        if self.tear and offset == 40 and self.data[40]:
            self.data[46] += 1
        return self.data[offset:offset+count]

    def write_request(self, address, values):
        self.writes.append((address, values))
        offset = (address-0x20001000)//4
        self.data[offset:offset+len(values)] = values
        if offset == 32 and not self.pending:
            seq = values[0]
            for domain, start in enumerate([40, 260, 388]):
                self.data[start:start+8] = [seq, 1, 100, 100, [848, 480, 256][domain], seq, 2, 0]
            if self.data[33] == 2:
                self.data[268:388] = [0]*120
                self.data[268:280] = [0x314D4C43, 1, self.data[34], 1, 655360, 0, 300, 4, 128, 10000, 1, 0]
                self.data[280:284] = [4, 10, 300, 0]


class ProtocolTests(unittest.TestCase):
    def test_audio_fixture_is_a_completed_read_only_boot_report(self):
        elf,rpc=FakeElf(),FakeRpc()
        elf.header=list(elf.header);elf.header[4]=rpc.data[4]=1|1024
        elf.symbols=dict(elf.symbols,zeptocore_audio_fixture=(0x20001730,32))
        rpc.data.extend([0x31544641,0,1,1,1234,100,500,0])
        reader=DeviceReader(elf,rpc);data=reader.audio_fixture()
        self.assertTrue(data['valid']);self.assertEqual(data['operation'],'created')
        self.assertEqual(rpc.writes,[])
        rpc.data[-5]=9
        with self.assertRaises(DiagnosticError):reader.audio_fixture()

    def test_cached_media_uses_fixed_typed_read_only_symbols(self):
        elf,rpc=FakeElf(),FakeRpc()
        reader=DeviceReader(elf,rpc)
        self.assertFalse(reader.media_cached()['available'])
        elf.symbols=dict(elf.symbols,audio_variant_num=(0x20001730,1),
            total_number_samples=(0x20001732,2),savefile_current=(0x20001734,1),
            savefile_has_data=(0x20001738,16))
        rpc.data.extend([96<<16|2,0,0,0,0,0x01000000])
        data=reader.media_cached()
        self.assertEqual(data['values']['audio_variant_num'],2)
        self.assertEqual(data['values']['total_number_samples'],96)
        self.assertEqual(data['values']['savefile_has_data'][-1],1)
        self.assertEqual(rpc.writes,[])
        elf.symbols['audio_variant_num']=(0x10000000,1)
        with self.assertRaises(DiagnosticError):reader.media_cached()

    def test_boot_benchmark_is_bounded_and_read_only(self):
        elf, rpc = FakeElf(), FakeRpc()
        reader = DeviceReader(elf, rpc)
        self.assertFalse(reader.benchmark()["available"])
        elf.symbols = dict(elf.symbols, zeptocore_seek_benchmark=(0x20001000+1840,512))
        report = [0x31424D53,1,0,128,2031616,31,64,768,1,12,500,9000,0,786432,0,300] + [0]*112
        rpc.data += report
        result = reader.benchmark()
        self.assertTrue(result["valid"])
        self.assertEqual(result["ordinary_fat_visits"],9000)
        self.assertEqual(result["verified_bytes"],786432)
        self.assertEqual(rpc.writes,[])
        self.assertLessEqual(max(rpc.read_sizes),64)
        rpc.data[-128] = 0
        self.assertFalse(reader.benchmark()["available"])
        elf.symbols["zeptocore_seek_benchmark"]=(0x10000000,512)
        with self.assertRaises(DiagnosticError):reader.benchmark()

    def test_layout_request_and_bounds(self):
        elf, rpc = FakeElf(), FakeRpc()
        elf.header = list(elf.header)
        elf.header[4] = rpc.data[4] = 1 | 128
        reader = DeviceReader(elf, rpc)
        result = reader.snapshot(layout_id=0x100)["control"]
        self.assertEqual(result["path"], "bank1/1.0.wav")
        self.assertEqual(result["fragments"], [{"starting_cluster": 300, "clusters": 10}])
        self.assertEqual(rpc.writes[0], (0x20001084, [2, 256, 0, 0, 0, 0, 0]))
        malformed = rpc.data[268:388].copy()
        malformed[7] = 65
        with self.assertRaises(DiagnosticError):
            decode_layout(malformed, 256)
        old = len(rpc.writes)
        with self.assertRaises(DiagnosticError):
            reader.snapshot(layout_id=65536)
        self.assertEqual(len(rpc.writes), old)

    def test_pending_other_request_finishes_before_next_command(self):
        elf, rpc = FakeElf(), FakeRpc()
        elf.header = list(elf.header)
        elf.header[4] = rpc.data[4] = 1 | 128
        reader = DeviceReader(elf, rpc)
        rpc.pending = True
        reader.timeout = .01
        with self.assertRaisesRegex(DiagnosticError, "timeout"):
            reader.snapshot(layout_id=0x100)
        rpc.pending = False
        rpc.write_request(0x20001080, [1])  # simulate the pending firmware response
        self.assertEqual(reader.snapshot()["sequence"], 2)
        self.assertEqual(rpc.writes[-2], (0x20001084, [1, 0, 0, 0, 0, 0, 0]))

    def test_snapshot_and_bounded_memory_writes(self):
        rpc = FakeRpc()
        reader = DeviceReader(FakeElf(), rpc)
        snap = reader.snapshot()
        self.assertEqual(snap["sequence"], 1)
        self.assertEqual(snap["session"], [123, 456])
        self.assertEqual(snap["audio"]["publish_us"], 2)
        self.assertLessEqual(max(rpc.read_sizes), 64)
        self.assertEqual(rpc.writes, [(0x20001084, [1, 0, 0, 0, 0, 0, 0]), (0x20001080, [1])])

    def test_stale_elf_never_writes(self):
        rpc = FakeRpc()
        rpc.data[8] ^= 1
        with self.assertRaisesRegex(DiagnosticError, "identity mismatch"):
            DeviceReader(FakeElf(), rpc).snapshot()
        self.assertFalse(rpc.writes)

    def test_torn_snapshot_is_rejected(self):
        rpc = FakeRpc()
        rpc.tear = True
        with self.assertRaisesRegex(DiagnosticError, "changed during"):
            DeviceReader(FakeElf(), rpc).snapshot()

    def test_stalled_writer_times_out_without_reset(self):
        rpc = FakeRpc()
        rpc.pending = True
        with self.assertRaisesRegex(DiagnosticError, "pending domains"):
            DeviceReader(FakeElf(), rpc, timeout=0).snapshot()
        self.assertEqual(len(rpc.writes), 2)

    def test_timeout_recovery_does_not_overwrite_request(self):
        rpc = FakeRpc()
        rpc.pending = True
        reader = DeviceReader(FakeElf(), rpc, timeout=0)
        for _ in range(2):
            with self.assertRaisesRegex(DiagnosticError, "pending domains"):
                reader.snapshot()
        self.assertEqual(len(rpc.writes), 2)
        # A reconnect also recovers the same request with no write.
        reconnected = DeviceReader(FakeElf(), rpc, timeout=0)
        with self.assertRaisesRegex(DiagnosticError, "pending domains"):
            reconnected.snapshot()
        self.assertEqual(len(rpc.writes), 2)
        for domain, start in enumerate([40, 260, 388]):
            rpc.data[start:start+8] = [1, 1, 100, 100, [848, 480, 256][domain], 1, 2, 0]
        self.assertEqual(reader.snapshot()["sequence"], 1)
        self.assertEqual(len(rpc.writes), 2)

    def test_sequence_wrap_skips_zero(self):
        rpc = FakeRpc()
        rpc.data[32] = 0xffffffff
        for start in [40, 260, 388]:
            rpc.data[start] = 0xffffffff
        self.assertEqual(DeviceReader(FakeElf(), rpc).snapshot()["sequence"], 1)

    def test_reboot_is_not_merged_into_capture(self):
        rpc = FakeRpc()
        reader = DeviceReader(FakeElf(), rpc)
        reader.snapshot()
        rpc.data[16] += 1
        old_writes = len(rpc.writes)
        with self.assertRaisesRegex(DiagnosticError, "rebooted"):
            reader.snapshot()
        self.assertEqual(len(rpc.writes), old_writes)

    def test_bad_elf_layout_is_rejected(self):
        elf = FakeElf()
        elf.header = list(elf.header)
        elf.header[24] = 0
        with self.assertRaisesRegex(DiagnosticError, "layout"):
            DeviceReader(elf, FakeRpc())

    def test_elf_parser_rejects_truncation(self):
        with tempfile.TemporaryDirectory() as d:
            p = Path(d) / "bad.elf"
            p.write_bytes(b"\x7fELF\x01\x01\x01")
            with self.assertRaises(DiagnosticError):
                ElfImage(p)

    def test_histograms_provide_ranges_not_fake_exact_percentiles(self):
        self.assertIsNone(percentile_range([0]*16, .99))
        self.assertEqual(percentile_range([0, 100]+[0]*14, .99), [5, 16])
        self.assertEqual(percentile_range([0]*15+[1], .99), [100001, None])

    def test_summary_rejects_reset_and_saturation(self):
        a = DeviceReader(FakeElf(), FakeRpc()).snapshot()
        b = copy.deepcopy(a)
        b["host_time"] += 1
        self.assertTrue(summarize([a, b])["valid"])
        b["session"][0] += 1
        self.assertFalse(summarize([a, b])["valid"])
        b = copy.deepcopy(a)
        b["audio"]["counters"]["saturated"] = 1
        self.assertFalse(summarize([a, b])["valid"])

    def test_read_only_command_allowlist(self):
        with tempfile.TemporaryDirectory() as d:
            service = DebugService(DeviceReader(FakeElf(), FakeRpc()), Path(d))
            for command in ["reset", "halt", "flash", "write_memory", "playback.seek"]:
                with self.assertRaisesRegex(DiagnosticError, "unsupported"):
                    service.handle({"schema": "zeptocore.debug-command", "version": 1,
                                    "id": "test", "command": command})
            response = service.handle({"schema": "zeptocore.debug-command", "version": 1,
                                       "id": "test", "command": "maps.status"})
            self.assertEqual(response["status"], "unavailable")

    def test_maps_status_decodes_snapshot_without_filesystem_command(self):
        elf, rpc = FakeElf(), FakeRpc()
        elf.header = list(elf.header)
        elf.header[4] = rpc.data[4] = 1 | 8 | 16
        # Control payload: three metrics, sixteen counters, then context.
        counters = 260 + 8 + 72
        rpc.data[counters + 1] = 42
        rpc.data[counters + 2] = 7
        rpc.data[counters + 16 + 25] = 4492
        with tempfile.TemporaryDirectory() as d:
            service = DebugService(DeviceReader(elf, rpc), Path(d))
            response = service.handle({"schema": "zeptocore.debug-command", "version": 1,
                                       "id": "maps", "command": "maps.status"})
        self.assertEqual(response["status"], "ok")
        self.assertEqual(response["data"]["files"], 42)
        self.assertEqual(response["data"]["builds"], 7)
        self.assertEqual(response["data"]["reserved_bytes"], 4492)
        self.assertTrue(response["data"]["attachment_enabled"])
        self.assertEqual(rpc.writes, [(0x20001084, [1, 0, 0, 0, 0, 0, 0]),
                                     (0x20001080, [1])])

    def test_memory_reports_heap_and_reverb_counts(self):
        elf, rpc = FakeElf(), FakeRpc()
        elf.header = list(elf.header)
        elf.header[4] = rpc.data[4] = 1 | 8 | 16 | 32
        context = 260 + 8 + 72 + 16
        rpc.data[context] = 195000 | (4 << 24) | (3 << 28)
        with tempfile.TemporaryDirectory() as d:
            service = DebugService(DeviceReader(elf, rpc), Path(d))
            data = service.handle({"schema": "zeptocore.debug-command", "version": 1,
                                   "id": "memory", "command": "memory.status"})["data"]
        self.assertEqual(data["heap_total_bytes"], 195000)
        self.assertEqual(data["reverb_combs"], 4)
        self.assertEqual(data["reverb_allpasses"], 3)

    def test_switch_summary_reports_missed_observations(self):
        elf, rpc = FakeElf(), FakeRpc()
        elf.header = list(elf.header)
        elf.header[4] = rpc.data[4] = 1 | 64
        a = DeviceReader(elf, rpc).snapshot()
        b = copy.deepcopy(a)
        b["host_time"] += 1
        b["irq"]["counters"].update(switch_completed=1, switch_last_us=21000,
                                   switch_requested_token=1, switch_completed_token=1)
        c = copy.deepcopy(b)
        c["host_time"] += 1
        c["irq"]["counters"].update(switch_completed=4, switch_last_us=19000,
                                   switch_requested_token=5, switch_completed_token=4)
        result = summarize([a, b, c])["sample_switch"]
        self.assertEqual(result["observations_us"], [21000, 19000])
        self.assertEqual(result["unobserved_completions"], 2)
        self.assertEqual(result["p99_us"], 21000)
        self.assertTrue(result["pending"])

    def test_capture_failure_releases_host_capture_and_preserves_pending_request(self):
        with tempfile.TemporaryDirectory() as d:
            rpc = FakeRpc()
            rpc.pending = True
            service = DebugService(DeviceReader(FakeElf(), rpc, timeout=0), Path(d))
            with self.assertRaisesRegex(DiagnosticError, "timeout"):
                service.handle({"schema": "zeptocore.debug-command", "version": 1,
                                "id": "capture", "command": "capture.start"})
            self.assertIsNone(service.capture)
            self.assertEqual(service.reader.pending_sequence, 1)
            self.assertEqual(len(rpc.writes), 2)

    def test_unix_server_serializes_clients_and_survives_disconnect(self):
        with tempfile.TemporaryDirectory() as d:
            address = str(Path(d) / "debug.sock")
            rpc = FakeRpc()
            with socketserver.UnixStreamServer(address, RequestHandler) as server:
                server.service = DebugService(DeviceReader(FakeElf(), rpc), Path(d))
                thread = threading.Thread(target=server.serve_forever, daemon=True)
                thread.start()
                def call(command="seek.status", drop=False):
                    # Duplicate IDs are correlation labels for stateless snapshots,
                    # and must neither reset counters nor interleave publications.
                    message = {"schema": "zeptocore.debug-command", "version": 1,
                               "id": "duplicate", "command": command}
                    with socket.socket(socket.AF_UNIX) as client:
                        client.settimeout(3)
                        client.connect(address)
                        client.sendall((json.dumps(message)+"\n").encode())
                        if drop:
                            return None
                        return json.loads(client.makefile("rb").readline())
                try:
                    call(drop=True)
                    with ThreadPoolExecutor(max_workers=4) as pool:
                        responses = list(pool.map(lambda _: call(), range(8)))
                    sequences = [r["data"]["sequence"] for r in responses]
                    self.assertEqual(len(set(sequences)), 8)
                    self.assertTrue(all(r["id"] == "duplicate" for r in responses))
                    self.assertEqual(call([])["status"], "error")
                    self.assertEqual(call()["status"], "ok")
                    self.assertTrue(all(len(values) in (1, 7) for _, values in rpc.writes))
                finally:
                    server.shutdown()
                    thread.join(timeout=2)


if __name__ == "__main__":
    unittest.main()
