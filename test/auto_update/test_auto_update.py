"""Exercise maintenance in disposable repositories; never touch a real service."""

import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import time
import unittest


ROOT = Path(__file__).resolve().parents[2]
GIT = shutil.which("git")
UV = shutil.which("uv")
STUB = '''import json, os, sys, time
from pathlib import Path
command = Path(sys.argv[0]).name
control = Path(os.environ["AUTO_UPDATE_TEST_CONTROL"])
with Path(os.environ["AUTO_UPDATE_TEST_LOG"]).open("a") as handle:
    handle.write(json.dumps({"command": command, "args": sys.argv[1:],
        "cwd": os.getcwd(), "path": os.environ["PATH"],
        "git_prompt": os.environ.get("GIT_TERMINAL_PROMPT"),
        "ssh_command": os.environ.get("GIT_SSH_COMMAND"),
        "stdin": sys.stdin.read()}) + "\\n")
if command == "make":
    (control / "build-started").touch()
    while (control / "block-build").exists():
        time.sleep(0.02)
    Path("core_server").write_text("built binary")
    if (control / "fail-build").exists():
        sys.exit(1)
elif command == "systemctl":
    if sys.argv[1] == "restart" and (control / "fail-restart").exists():
        sys.exit(1)
    if sys.argv[1] == "is-active" and (control / "inactive").exists():
        sys.exit(3)
elif command == "uv":
    if sys.argv[-1].endswith("modelo_final") and (control / "fail-cleanup").exists():
        sys.exit(1)
'''


class AutoUpdateTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="core maintenance ")
        self.addCleanup(self.temporary.cleanup)
        self.base = Path(self.temporary.name).resolve()
        self.bin = self.base / "bin"
        self.bin.mkdir()
        self.control = self.base / "control"
        self.control.mkdir()
        self.calls_path = self.base / "calls.jsonl"
        self.env = {key: value for key, value in os.environ.items() if not key.startswith("GIT_")}
        self.env.update({
            "HOME": str(self.base / "home"), "PATH": str(self.bin),
            "GIT_CONFIG_NOSYSTEM": "1", "GIT_CONFIG_GLOBAL": os.devnull,
            "AUTO_UPDATE_TEST_CONTROL": str(self.control),
            "AUTO_UPDATE_TEST_LOG": str(self.calls_path),
        })
        Path(self.env["HOME"]).mkdir()
        (self.bin / "python3").symlink_to(sys.executable)
        (self.bin / "git").symlink_to(GIT)
        for command in ("make", "systemctl", "uv"):
            path = self.bin / command
            path.write_text(f"#!{sys.executable}\n" + STUB)
            path.chmod(0o755)

        self.remote = self.base / "remote.git"
        self.seed = self.base / "seed"
        self.git(self.base, "init", "--bare", "--initial-branch=main", str(self.remote))
        self.git(self.base, "init", "--initial-branch=main", str(self.seed))
        self.configure_author(self.seed)
        (self.seed / "dev").mkdir()
        for name in ("auto_update.py", "delete_old_folders.py"):
            shutil.copy2(ROOT / "dev" / name, self.seed / "dev" / name)
        (self.seed / ".gitignore").write_text("core_server\nstorage/\ndrum_separation_model_output/\n")
        (self.seed / "source.txt").write_text("initial source\n")
        (self.seed / "local-only.txt").write_text("initial local file\n")
        self.git(self.seed, "add", ".")
        self.git(self.seed, "commit", "-m", "initial")
        self.git(self.seed, "remote", "add", "origin", str(self.remote))
        self.git(self.seed, "push", "--quiet", "-u", "origin", "main")
        self.repo = self.clone("ezeptocore")
        self.original = self.git(self.repo, "rev-parse", "HEAD")

    def git(self, directory, *args, check=True):
        result = subprocess.run(
            [GIT, "-C", str(directory), *args], env=self.env,
            text=True, capture_output=True, check=check,
        )
        return result.stdout.strip()

    def configure_author(self, directory):
        self.git(directory, "config", "user.email", "maintenance@example.test")
        self.git(directory, "config", "user.name", "Maintenance Test")

    def clone(self, name):
        repo = self.base / "repos with spaces" / name
        repo.parent.mkdir(exist_ok=True)
        self.git(self.base, "clone", "--quiet", str(self.remote), str(repo))
        self.configure_author(repo)
        (repo / "core_server").write_text("already deployed")
        (repo / "drum_separation_model_output/modelo_final").mkdir(parents=True)
        (repo / "storage").mkdir()
        return repo

    def upstream_commit(self):
        (self.seed / "source.txt").write_text("updated source\n")
        self.git(self.seed, "commit", "-am", "update")
        self.git(self.seed, "push", "--quiet")
        return self.git(self.seed, "rev-parse", "HEAD")

    def run_script(self, success=True, repo=None):
        result = subprocess.run(
            [str((repo or self.repo) / "dev/auto_update.py")], cwd=self.base,
            env=self.env, text=True, capture_output=True, timeout=15,
        )
        self.assertEqual(result.returncode == 0, success, result.stdout + result.stderr)
        return result

    def calls(self, command=None):
        records = [] if not self.calls_path.exists() else [
            json.loads(line) for line in self.calls_path.read_text().splitlines()
        ]
        return records if command is None else [r for r in records if r["command"] == command]

    def state_path(self, name="deployment.json"):
        return self.repo / ".git/auto-update" / name

    def state(self):
        return json.loads(self.state_path().read_text())

    def test_unchanged_then_quiet_with_cron_path_and_absolute_cleanup_paths(self):
        self.run_script()
        self.assertEqual(self.state(), {"deployed_commit": self.original, "pending": False})
        self.assertEqual(self.calls("make"), [])
        self.assertEqual(self.calls("systemctl"), [])
        cleanups = self.calls("uv")
        self.assertEqual(len(cleanups), 2)
        for record, (directory, age) in zip(cleanups, (
            ("drum_separation_model_output/modelo_final", 30), ("storage", 60),
        )):
            self.assertEqual(record["args"], [
                "run", "--quiet", "--script", str(self.repo / "dev/delete_old_folders.py"),
                "--delete", "--yes", "--age", str(age), str(self.repo / directory),
            ])
            self.assertEqual(record["cwd"], str(self.repo))
            self.assertEqual(record["stdin"], "")
            self.assertEqual(record["git_prompt"], "0")
            self.assertEqual(record["ssh_command"], "ssh -oBatchMode=yes -oConnectTimeout=30")
            self.assertIn("/usr/local/go/bin", record["path"].split(os.pathsep))
            self.assertIn(str(Path(self.env["HOME"]) / ".local/bin"), record["path"].split(os.pathsep))
        result = self.run_script()
        self.assertEqual(result.stdout + result.stderr, "")
        self.assertEqual(len(self.calls()), 2)

    def test_fast_forward_build_restart_and_suppress_hook(self):
        target = self.upstream_commit()
        hook = self.repo / ".git/hooks/post-merge"
        hook.write_text("#!/bin/sh\ntouch hook-ran\nexit 1\n")
        hook.chmod(0o755)
        self.run_script()
        self.assertFalse((self.repo / "hook-ran").exists())
        self.assertEqual(self.git(self.repo, "rev-parse", "HEAD"), target)
        self.assertEqual(self.state(), {"deployed_commit": target, "pending": False})
        self.assertEqual([r["command"] for r in self.calls()], ["make", "systemctl", "systemctl", "uv", "uv"])
        self.assertEqual(self.calls("make")[0]["args"], ["core_server"])
        self.assertEqual(self.calls("systemctl")[0]["args"], ["restart", "zns.ezeptocore.service"])
        self.assertEqual(self.calls("systemctl")[1]["args"], ["is-active", "--quiet", "zns.ezeptocore.service"])
        self.assertEqual(self.git(self.repo, "status", "--porcelain"), "")
        self.assertEqual(self.run_script().stdout, "")
        self.assertEqual(len(self.calls("make")), 1)

    def test_conflicting_local_edit_is_preserved_and_cleanup_still_runs(self):
        self.git(self.repo, "config", "merge.autoStash", "true")
        (self.repo / "source.txt").write_text("local edit\n")
        self.upstream_commit()
        self.run_script(success=False)
        self.assertEqual((self.repo / "source.txt").read_text(), "local edit\n")
        self.assertEqual(self.git(self.repo, "rev-parse", "HEAD"), self.original)
        self.assertEqual(self.git(self.repo, "stash", "list"), "")
        self.assertEqual(self.calls("make"), [])
        self.assertEqual(self.calls("systemctl"), [])
        self.assertEqual(len(self.calls("uv")), 2)

    def test_nonconflicting_local_edit_is_preserved(self):
        (self.repo / "local-only.txt").write_text("local edit\n")
        self.upstream_commit()
        self.run_script()
        self.assertEqual((self.repo / "local-only.txt").read_text(), "local edit\n")
        self.assertEqual(len(self.calls("make")), 1)

    def test_divergent_history_is_preserved(self):
        (self.repo / "local-only.txt").write_text("local commit\n")
        self.git(self.repo, "commit", "-am", "local")
        local_head = self.git(self.repo, "rev-parse", "HEAD")
        self.upstream_commit()
        result = self.run_script(success=False)
        self.assertIn("Cannot fast-forward", result.stderr)
        self.assertEqual(self.git(self.repo, "rev-parse", "HEAD"), local_head)
        self.assertEqual(self.calls("make"), [])

    def test_build_failure_retries_without_new_commit(self):
        target = self.upstream_commit()
        failure = self.control / "fail-build"
        failure.touch()
        self.run_script(success=False)
        self.assertEqual(self.git(self.repo, "rev-parse", "HEAD"), target)
        self.assertEqual(self.state(), {"deployed_commit": self.original, "pending": True})
        self.assertEqual(self.calls("systemctl"), [])
        self.assertEqual(len(self.calls("uv")), 2)
        failure.unlink()
        self.run_script()
        self.assertEqual(len(self.calls("make")), 2)
        self.assertEqual(len(self.calls("uv")), 2)
        self.assertEqual(self.state(), {"deployed_commit": target, "pending": False})

    def test_missing_binary_retry_even_if_failed_build_leaves_a_binary(self):
        self.run_script()
        (self.repo / "core_server").unlink()
        failure = self.control / "fail-build"
        failure.touch()
        self.run_script(success=False)
        self.assertTrue((self.repo / "core_server").exists())
        self.assertEqual(self.state(), {"deployed_commit": self.original, "pending": True})
        failure.unlink()
        self.run_script()
        self.assertEqual(len(self.calls("make")), 2)
        self.assertFalse(self.state()["pending"])

    def test_first_run_without_binary_and_service_names(self):
        for name in ("ezeptocore", "ectocore", "zeptocore"):
            with self.subTest(name=name):
                repo = self.repo if name == "ezeptocore" else self.clone(name)
                (repo / "core_server").unlink()
                self.run_script(repo=repo)
                self.assertEqual(self.calls("systemctl")[-2]["args"], ["restart", f"zns.{name}.service"])

    def test_restart_failure_retries(self):
        target = self.upstream_commit()
        failure = self.control / "fail-restart"
        failure.touch()
        self.run_script(success=False)
        self.assertEqual(self.state(), {"deployed_commit": self.original, "pending": True})
        self.assertEqual(len(self.calls("systemctl")), 1)
        failure.unlink()
        self.run_script()
        self.assertEqual(len(self.calls("make")), 2)
        self.assertEqual(self.state(), {"deployed_commit": target, "pending": False})

    def test_inactive_service_does_not_mark_deployment_successful(self):
        target = self.upstream_commit()
        failure = self.control / "inactive"
        failure.touch()
        self.run_script(success=False)
        self.assertEqual(self.state(), {"deployed_commit": self.original, "pending": True})
        failure.unlink()
        self.run_script()
        self.assertEqual(self.state(), {"deployed_commit": target, "pending": False})

    def test_cleanup_error_does_not_block_deployment_or_other_cleanup(self):
        target = self.upstream_commit()
        (self.control / "fail-cleanup").touch()
        self.run_script(success=False)
        self.assertEqual(len(self.calls("uv")), 2)
        self.assertEqual(self.state(), {"deployed_commit": target, "pending": False})
        self.assertEqual(self.run_script().stdout, "")
        self.assertEqual(len(self.calls("uv")), 2)

    def test_daily_schedule(self):
        self.run_script()
        path = self.state_path("cleanup-attempt.json")
        path.write_text(json.dumps(time.time() - 86300))
        self.run_script()
        self.assertEqual(len(self.calls("uv")), 2)
        path.write_text(json.dumps(time.time() - 86401))
        self.run_script()
        self.assertEqual(len(self.calls("uv")), 4)
        self.assertGreater(json.loads(path.read_text()), time.time() - 10)

    def test_missing_cleanup_directories_are_skipped(self):
        (self.repo / "storage").rmdir()
        (self.repo / "drum_separation_model_output/modelo_final").rmdir()
        result = self.run_script()
        self.assertEqual(result.stdout + result.stderr, "")
        self.assertEqual(self.calls(), [])
        self.assertTrue(self.state_path("cleanup-attempt.json").exists())

    def test_fetch_failure_still_allows_cleanup(self):
        self.git(self.repo, "remote", "set-url", "origin", str(self.base / "missing.git"))
        self.run_script(success=False)
        self.assertEqual(len(self.calls("uv")), 2)
        self.assertEqual(self.calls("make"), [])

    def test_deleted_upstream_is_not_treated_as_unchanged(self):
        self.git(self.remote, "config", "receive.denyDeleteCurrent", "ignore")
        self.git(self.seed, "push", "--delete", "origin", "main")
        self.run_script(success=False)
        self.assertEqual(self.calls("make"), [])
        self.assertEqual(len(self.calls("uv")), 2)

    def test_configured_upstream_instead_of_hardcoded_main(self):
        self.git(self.seed, "checkout", "-b", "production")
        self.git(self.seed, "push", "--quiet", "-u", "origin", "production")
        self.git(self.repo, "fetch", "origin")
        self.git(self.repo, "checkout", "--track", "origin/production")
        target = self.upstream_commit()
        self.run_script()
        self.assertEqual(self.git(self.repo, "branch", "--show-current"), "production")
        self.assertEqual(self.state()["deployed_commit"], target)

    def test_detached_head_fails_but_cleanup_runs(self):
        self.git(self.repo, "checkout", "--detach")
        self.run_script(success=False)
        self.assertEqual(self.calls("make"), [])
        self.assertEqual(len(self.calls("uv")), 2)

    def test_overlapping_invocation_exits_quietly(self):
        self.upstream_commit()
        block = self.control / "block-build"
        block.touch()
        process = subprocess.Popen(
            [str(self.repo / "dev/auto_update.py")], cwd=self.base, env=self.env,
            text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
        )
        try:
            deadline = time.monotonic() + 10
            while not (self.control / "build-started").exists():
                if process.poll() is not None or time.monotonic() > deadline:
                    self.fail("First maintenance invocation did not reach the build")
                time.sleep(0.02)
            result = self.run_script()
            self.assertEqual(result.stdout + result.stderr, "")
            self.assertEqual(len(self.calls()), 1)
        finally:
            block.unlink()
            try:
                stdout, stderr = process.communicate(timeout=10)
            except subprocess.TimeoutExpired:
                process.kill()
                process.communicate()
                raise
        self.assertEqual(process.returncode, 0, stdout + stderr)
        self.assertEqual(len(self.calls("make")), 1)
        self.assertEqual(len(self.calls("uv")), 2)
        self.assertEqual(self.run_script().stdout, "")


@unittest.skipUnless(UV, "uv is needed to exercise the existing cleanup utility")
class RealCleanupTests(unittest.TestCase):
    def test_retention_uses_newest_file_and_preserves_empty_hidden_and_root_files(self):
        for days in (30, 60):
            with self.subTest(days=days), tempfile.TemporaryDirectory(prefix="cleanup fixture ") as temporary:
                base = Path(temporary)
                helper = base / "delete_old_folders.py"
                shutil.copy2(ROOT / "dev/delete_old_folders.py", helper)
                data = base / "data"
                data.mkdir()

                def file_at_age(relative, age):
                    path = data / relative
                    path.parent.mkdir(parents=True, exist_ok=True)
                    path.write_text("fixture")
                    timestamp = time.time() - age * 86400
                    os.utime(path, (timestamp, timestamp))

                file_at_age("old/nested/file", days + 1)
                file_at_age("recent/file", days - 1)
                file_at_age("mixed/old", days + 10)
                file_at_age("mixed/nested/recent", 1)
                file_at_age(".hidden/file", days + 10)
                file_at_age("root-file", days + 10)
                file_at_age("forty-days/file", 40)
                (data / "empty").mkdir()
                result = subprocess.run(
                    [UV, "run", "--quiet", "--script", str(helper),
                     "--delete", "--yes", "--age", str(days), str(data)],
                    cwd=base, stdin=subprocess.DEVNULL, text=True,
                    capture_output=True, timeout=60,
                )
                self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                self.assertFalse((data / "old").exists())
                for name in ("recent", "mixed", ".hidden", "empty", "root-file"):
                    self.assertTrue((data / name).exists(), name)
                self.assertEqual((data / "forty-days").exists(), days == 60)


if __name__ == "__main__":
    unittest.main()
