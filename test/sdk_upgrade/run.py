"""Check TinyUSB patch failure handling and Make device selection without hardware."""
from pathlib import Path
import copy
import json
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
PATCH = ROOT / 'lib/cmake/patch_tinyusb.cmake'
sys.path.insert(0, str(ROOT / 'scripts'))
from zeptocore_cpu_report import report


class TinyUsbPatchTests(unittest.TestCase):
    def test_patch_is_targeted_and_idempotent(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'lib/tinyusb/src/tusb.c'
            source.parent.mkdir(parents=True)
            source.write_text('osal_mutex_lock(mutex, OSAL_TIMEOUT_WAIT_FOREVER);\n' * 2
                              + 'other_lock(OSAL_TIMEOUT_WAIT_FOREVER);\n')
            command = ['cmake', f'-DPICO_SDK_PATH={directory}', '-P', str(PATCH)]
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(source.read_text().count('OSAL_TIMEOUT_NORMAL'), 2)
            self.assertIn('other_lock(OSAL_TIMEOUT_WAIT_FOREVER)', source.read_text())
            timestamp = source.stat().st_mtime_ns
            subprocess.run(command, check=True, capture_output=True)
            self.assertEqual(source.stat().st_mtime_ns, timestamp)

    def test_unknown_layout_is_not_modified(self):
        with tempfile.TemporaryDirectory() as directory:
            source = Path(directory) / 'src/tusb.c'
            source.parent.mkdir()
            original = 'osal_mutex_lock(new_mutex, OSAL_TIMEOUT_WAIT_FOREVER);\n'
            source.write_text(original)
            result = subprocess.run(['cmake', f'-DPICO_TINYUSB_PATH={directory}',
                                     '-P', str(PATCH)], capture_output=True, text=True)
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('Unexpected TinyUSB endpoint lock layout', result.stderr)
            self.assertEqual(source.read_text(), original)


class MakeSelectionTests(unittest.TestCase):
    def test_device_selected_before_configuration(self):
        # Suppress dependencies while asking Make to expand the actual build
        # recipes. No SDK, generated header, compiler, or programmer is run.
        for target, definition in [
            ('zeptocore', 'zeptocore'), ('zeptocore_256', 'zeptocore'),
            ('ezeptocore', 'ezeptocore'), ('ezeptocore_midi', 'ezeptocore_midi'),
            ('ectocore', 'ectocore'), ('zeptoboard', 'zeptoboard'),
        ]:
            suffix = '_256' if target.endswith('_256') else ''
            result = subprocess.run(['make', '-n', target, 'MAKE=echo',
                '-o', 'pico-sdk', '-o', 'pico-extras', '-o', 'ensure_arm_toolchain',
                '-o', 'check-pico-dependencies', '-o', 'lib/fuzz.h',
                '-o', 'lib/transfer_saturate2.h', '-o', 'lib/sinewaves2.h',
                '-o', 'lib/crossfade4_441.h', '-o', 'lib/resonantfilter_data.h',
                '-o', 'lib/cuedsounds.h'], cwd=ROOT, capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            configure = [line for line in result.stdout.splitlines() if 'cmake -S' in line]
            self.assertTrue(configure, target)
            self.assertTrue(all(f'/lib/cmake/{definition}_compile_definitions{suffix}.cmake'
                                in line for line in configure), result.stdout)


class CpuReportTests(unittest.TestCase):
    def snapshots(self):
        metric = dict(count=0, total_us=0, errors=0, short_reads=0, bytes=0,
                      max_us=0, histogram=[0]*16)
        first = dict(session=[1, 0], host_time=100,
                     device=dict(clock_hz=225000000, frames=441,
                                 actual_sample_rate_millihz=44501582))
        for domain in ('audio', 'control', 'irq'):
            first[domain] = dict(timestamp_us=0xffff0000, metrics={}, counters={})
        first['audio']['metrics']['callback'] = metric
        first['audio']['counters']['rendered_frames'] = 0
        first['irq']['counters'].update(starvation_count=0, starvation_frames=0)
        last = copy.deepcopy(first)
        # One device second across uint32 rollover, but deliberately two host
        # seconds: occupancy must use the device clock, not host jitter.
        last['host_time'] = 102
        last['audio']['timestamp_us'] = (0xffff0000 + 1000000) & 0xffffffff
        last['audio']['counters']['rendered_frames'] = 44100
        last['audio']['metrics']['callback'].update(count=100, total_us=250000,
                                                     max_us=2500,
                                                     histogram=[0]*8+[100]+[0]*7)
        return [first, last]

    def evaluate(self, snapshots):
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory)
            (path/'snapshots.jsonl').write_text(''.join(json.dumps(s)+'\n' for s in snapshots))
            return report(path)

    def test_device_time_rollover_and_host_jitter(self):
        result = self.evaluate(self.snapshots())
        self.assertEqual(result['audio_callback_occupancy_percent'], 25)
        self.assertEqual(result['callback_mean_us'], 2500)

    def test_reset_is_rejected(self):
        snapshots = self.snapshots()
        snapshots[-1]['session'] = [2, 0]
        with self.assertRaises(ValueError):
            self.evaluate(snapshots)

    def test_stalled_device_is_rejected(self):
        snapshots = self.snapshots()
        snapshots[-1]['audio']['timestamp_us'] = snapshots[0]['audio']['timestamp_us']
        with self.assertRaises(ValueError):
            self.evaluate(snapshots)


if __name__ == '__main__':
    unittest.main()
