import sys
import unittest
from pathlib import Path
from unittest.mock import patch
sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
import zeptocore_midi as midi

class Negotiation(unittest.TestCase):
    def fixture(self, kind):
        now, sent = [0.], []
        def receive(timeout):
            now[0] += timeout
            if kind == 'modern':
                return [b'\xf0\x7dcore_caps=1\xf7']
            if kind == 'plain' and midi.management_frame('hello', id=False) in sent:
                return [b'\xf0core_caps=1\xf7']
            if kind == 'legacy' and bytes([0xb0, 1, 0]) in sent:
                return [b'\xf0version=v8.0.4\xf7']
            if kind == 'both' and bytes([0xb0, 1, 0]) in sent:
                return [b'\xf0version=v8.0.4\xf7', b'\xf0core_caps=1\xf7']
            return []
        return now, sent, receive
    def test_protocols(self):
        hellos = [midi.management_frame('hello'), midi.management_frame('hello', id=False)]
        for kind, expected in [('modern', 'sysex'), ('plain', 'sysex-plain'), ('legacy', 'legacy'), ('both', 'sysex-plain')]:
            now, sent, receive = self.fixture(kind)
            self.assertEqual(midi.negotiate_management(sent.append, receive, lambda:now[0]), expected)
            self.assertEqual(sent[0], midi.management_frame('hello'))
            self.assertEqual(sent[0][1], 0x7d)
            if kind == 'plain': self.assertEqual(sent, hellos)
            if kind in ('legacy', 'both'): self.assertEqual(sent, hellos + [bytes([0xb0, 1, 0])])
    def test_timeout_never_resets(self):
        now, sent, receive = self.fixture('silent')
        with self.assertRaisesRegex(RuntimeError, 'unavailable'):
            midi.negotiate_management(sent.append, receive, lambda:now[0])
        self.assertEqual(sent, [midi.management_frame('hello'), midi.management_frame('hello', id=False), bytes([0xb0,1,0])])
    def test_reset_selects_only_negotiated_protocol(self):
        for protocol, expected in [('sysex', midi.management_frame('bootloader')),
                                   ('sysex-plain', midi.management_frame('bootloader', id=False)),
                                   ('legacy', bytes([0xb0,0,0]))]:
            with patch.object(midi.subprocess, 'Popen') as process, patch.object(midi.subprocess, 'run') as run, \
                 patch.object(midi, 'negotiate_management', return_value=protocol), patch.object(midi.time, 'sleep'):
                midi.reset_management('hw:1,0,0')
                self.assertEqual(run.call_args.args[0][-1], expected.hex())
                process.return_value.terminate.assert_called_once()

if __name__ == '__main__': unittest.main()
