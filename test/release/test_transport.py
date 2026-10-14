"""Intel release transport regressions; no live SSH or release publication."""
import shlex
import subprocess
import sys
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / 'scripts'))
import release_transport as transport


class TransportTests(unittest.TestCase):
    def test_remote_script_is_quoted_and_kept_awake_for_its_entire_lifetime(self):
        script = 'printf "%s\\n" "source path with spaces"\nfalse'
        command = transport.ssh_command('builder', script)
        self.assertEqual(command[:3], ['/usr/bin/caffeinate', '-i', 'ssh'])
        self.assertIn('ServerAliveInterval=15', command)
        self.assertIn('ServerAliveCountMax=4', command)
        self.assertIn('ForwardAgent=no', command)
        remote = shlex.split(command[-1])
        self.assertEqual(remote[:4], ['/usr/bin/caffeinate', '-i', '/bin/bash', '-c'])
        result = subprocess.run(['bash', '-c', remote[-1]], capture_output=True, text=True)
        self.assertEqual(result.stdout, 'source path with spaces\n')
        self.assertEqual(result.returncode, 1)

    @patch.object(transport.time, 'sleep')
    def test_interrupted_transfer_reconnects_with_partial_files(self, sleep):
        runner = Mock()
        runner.run.side_effect = [subprocess.CalledProcessError(255, ['rsync']), 'completed']
        self.assertEqual(transport.rsync(runner, 'input.zip', 'builder:/tmp/output/'), 'completed')
        self.assertEqual(runner.run.call_count, 2)
        first, second = runner.run.call_args_list
        self.assertEqual(first, second)
        args = first.args
        self.assertIn('--partial', args)
        self.assertIn('--timeout=120', args)
        self.assertIn('--rsync-path=/usr/bin/caffeinate -i /usr/bin/rsync', args)
        self.assertNotIn('-z', args)
        self.assertNotIn('-cz', args)
        sleep.assert_called_once_with(2)

    @patch.object(transport.time, 'sleep')
    def test_repeated_network_failures_stop_after_three_attempts(self, sleep):
        runner = Mock()
        runner.run.side_effect = subprocess.CalledProcessError(30, ['rsync'])
        with self.assertRaises(subprocess.CalledProcessError):
            transport.rsync(runner, 'input.zip', 'builder:/tmp/output/')
        self.assertEqual(runner.run.call_count, 3)
        self.assertEqual(sleep.call_count, 2)

    @patch.object(transport.time, 'sleep')
    def test_permanent_transfer_error_is_not_retried(self, sleep):
        runner = Mock()
        runner.run.side_effect = subprocess.CalledProcessError(23, ['rsync'])
        with self.assertRaises(subprocess.CalledProcessError):
            transport.rsync(runner, 'input.zip', 'builder:/tmp/output/')
        self.assertEqual(runner.run.call_count, 1)
        sleep.assert_not_called()


if __name__ == '__main__':
    unittest.main()
