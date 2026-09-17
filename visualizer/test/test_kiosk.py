"""Configuration safety checks; never writes to system or desktop settings."""
import importlib.util
from pathlib import Path
import shlex
import subprocess
import tempfile
import unittest

SCRIPT = Path(__file__).resolve().parents[1] / "scripts/kiosk-config.py"
spec = importlib.util.spec_from_file_location("kiosk_config", SCRIPT)
config = importlib.util.module_from_spec(spec)
spec.loader.exec_module(config)


class KioskConfigTests(unittest.TestCase):
    def test_migrate_existing_multiline_kiosk_and_rerun(self):
        original = """# desktop startup
panel &
xset s off
sleep 3 && chromium \\
  --kiosk \\
  --no-first-run \\
  http://localhost:4173/ &
another-app &
"""
        result = config.autostart(original, "/home/zns/.local/bin/kiosk", 8080)
        self.assertEqual(result.count(config.BEGIN), 1)
        self.assertNotIn("sleep 3", result)
        self.assertNotIn("--kiosk", result)
        self.assertIn("panel &\nxset s off\n", result)
        self.assertIn("another-app &", result)
        self.assertEqual(result, config.autostart(result, "/home/zns/.local/bin/kiosk", 8080))

    def test_preserve_other_sites_and_quoted_desktop_code(self):
        original = 'echo "one\ntwo"\nchromium --kiosk https://example.com/ &\n'
        self.assertTrue(config.autostart(original, "/launcher", 4173).startswith(original))

    def test_refuse_ambiguous_migration_and_broken_markers(self):
        for original in (
            config.BEGIN + "\nold launcher &\n",
            config.END + "\n" + config.BEGIN,
            "chromium --kiosk http://localhost:4173/ & another-command\n",
            "if ready; then chromium --kiosk http://localhost:4173/; fi\n",
        ):
            with self.subTest(original=original), self.assertRaises(ValueError):
                config.autostart(original, "/launcher", 4173)

    def test_systemd_and_shell_quoting(self):
        strange = '/path with "quotes"/100%/$value/\\test'
        self.assertEqual(config.unit_quote(strange, executable_argument=True),
                         '"/path with \\"quotes\\"/100%%/$$value/\\\\test"')
        with self.assertRaises(ValueError):
            config.unit_quote("/path\nExecStart=bad")
        launch = "/home/some one's files/launch"
        updated = config.autostart("", launch, 4173)
        self.assertEqual(shlex.split(updated.splitlines()[-2]), [launch, "&"])

    def test_render_nvm_installation_with_spaces(self):
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            autostart = root / "existing"
            autostart.write_text("panel &\n")
            output = root / "rendered"
            result = subprocess.run([
                "python3", "-B", str(SCRIPT), "--app", "/home/zns/my app",
                "--reference", "/media/SD copy", "--home", "/home/zns",
                "--node", "/home/zns/.nvm/versions/node/v22.14.0/bin/node",
                "--user", "zns", "--browser", "/usr/bin/chromium", "--port", "4173",
                "--autostart", str(autostart), "--output", str(output),
            ], capture_output=True, text=True)
            self.assertEqual(result.returncode, 0, result.stderr)
            service = (output / "visualizer.service").read_text()
            self.assertIn('WorkingDirectory=/home/zns/my app\n', service)
            self.assertIn('ExecStart="/home/zns/.nvm/versions/node/v22.14.0/bin/node"', service)
            self.assertIn('"/media/SD copy" "--port" "4173"', service)
            self.assertIn("Restart=always", service)
            self.assertEqual(autostart.read_text(), "panel &\n")
            self.assertIn("KIOSK_URL=http://localhost:4173/", (output / "kiosk.conf").read_text())


if __name__ == "__main__":
    unittest.main()
