#!/usr/bin/env python3
"""Update core_server and clean old data from one root crontab entry.

    * * * * * /www/ezeptocore/dev/auto_update.py

Optional logging:
    * * * * * /www/ezeptocore/dev/auto_update.py >> /var/log/ezeptocore-update.log 2>&1

The repository is located relative to this script, regardless of the current
directory. Its folder name selects zns.<folder>.service. Requires Linux/systemd,
Python 3.8+, Git, Make, Go, Hugo, uv (with Python 3.11+ for cleanup), existing
build dependencies, and noninteractive Git access for the cron account.

State and the process lock live in <git-directory>/auto-update/. The first run
assumes an existing core_server matches HEAD. Failed deployments retry on the
next run. Cleanup is attempted immediately and every 24 hours: model output
after 30 days, storage after 60 days. No cron entry is installed by this script.
"""

import argparse
from datetime import datetime
import fcntl
import json
import math
import os
from pathlib import Path
import re
import shlex
import subprocess
import sys
import tempfile
import time


CLEANUP_INTERVAL = 24 * 60 * 60
CLEANUP_TARGETS = (
    ("drum_separation_model_output/modelo_final", 30),
    ("storage", 60),
)
FAILURES = (OSError, RuntimeError, ValueError, subprocess.SubprocessError)


def log(message, error=False):
    timestamp = datetime.now().astimezone().isoformat(timespec="seconds")
    print(f"[{timestamp}] {message}", file=sys.stderr if error else sys.stdout, flush=True)


def command_environment():
    env = os.environ.copy()
    home = Path.home()
    paths = env.get("PATH", "").split(os.pathsep) + [
        str(home / ".local/bin"), str(home / "bin"), str(home / "go/bin"),
        str(home / "node/bin"), str(home / ".cargo/bin"),
        "/usr/local/go/bin", "/usr/local/sbin", "/usr/local/bin",
        "/usr/sbin", "/usr/bin", "/sbin", "/bin",
    ]
    env["PATH"] = os.pathsep.join(dict.fromkeys(path for path in paths if path))
    env["GIT_TERMINAL_PROMPT"] = "0"
    env["GIT_ASKPASS"] = "/bin/false"
    env["SSH_ASKPASS_REQUIRE"] = "never"
    env["SSH_ASKPASS"] = "/bin/false"
    return env


def read_json(path):
    try:
        return json.loads(path.read_text())
    except FileNotFoundError:
        return None


def write_json(path, value):
    """Replace state atomically so interruption cannot leave partial JSON."""
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(mode="w", dir=path.parent, delete=False) as handle:
            temporary = Path(handle.name)
            json.dump(value, handle)
            handle.write("\n")
            handle.flush()
            os.fsync(handle.fileno())
        os.replace(temporary, path)
    finally:
        if temporary is not None and temporary.exists():
            temporary.unlink()


class Maintenance:
    def __init__(self, root):
        self.root = root
        self.env = command_environment()
        actual_root = Path(self.git("rev-parse", "--show-toplevel"))
        if actual_root.resolve() != root:
            raise RuntimeError("auto_update.py must be inside the repository's dev directory")
        self.state_dir = Path(self.git("rev-parse", "--absolute-git-dir")) / "auto-update"
        self.state_dir.mkdir(exist_ok=True)

    def run(self, command, quiet=False, check=True, timeout=None):
        result = subprocess.run(
            command, cwd=self.root, env=self.env, stdin=subprocess.DEVNULL,
            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            text=True, errors="replace", timeout=timeout,
        )
        if not quiet or (check and result.returncode):
            for line in result.stdout.splitlines():
                log(line, error=bool(result.returncode))
        if check and result.returncode:
            raise RuntimeError(f"Command exited {result.returncode}: {shlex.join(command)}")
        return result

    def git(self, *args, **kwargs):
        # Existing post-merge hooks also build and restart, without checking success.
        return self.run(
            ["git", "-c", "core.hooksPath=/dev/null", *args], quiet=True, **kwargs,
        ).stdout.strip()

    def update(self):
        branch = self.git("symbolic-ref", "--quiet", "--short", "HEAD")
        remote = self.git("config", "--get", f"branch.{branch}.remote")
        remote_branch = self.git("config", "--get", f"branch.{branch}.merge")
        if not remote or not remote_branch.startswith("refs/heads/"):
            raise RuntimeError(f"Branch {branch} needs a configured upstream branch")
        head = self.git("rev-parse", "HEAD")
        binary = self.root / "core_server"
        state_path = self.state_dir / "deployment.json"
        state = read_json(state_path)
        if state is None:
            # Persist the baseline BEFORE fetching or changing HEAD.
            state = {"deployed_commit": head if binary.is_file() else None, "pending": False}
            write_json(state_path, state)
        if (
            not isinstance(state, dict)
            or not isinstance(state.get("pending"), bool)
            or "deployed_commit" not in state
            or (state["deployed_commit"] is not None and (
                not isinstance(state["deployed_commit"], str)
                or re.fullmatch(r"(?:[0-9a-f]{40}|[0-9a-f]{64})", state["deployed_commit"]) is None
            ))
        ):
            raise RuntimeError(f"Invalid deployment state: {state_path}")

        # Keep configured SSH identities/wrappers, but never prompt cron for a password.
        ssh_command = (
            self.env.get("GIT_SSH_COMMAND")
            or self.git("config", "--get", "core.sshCommand", check=False)
            or shlex.quote(self.env.get("GIT_SSH", "ssh"))
        )
        self.env["GIT_SSH_COMMAND"] = ssh_command + " -oBatchMode=yes -oConnectTimeout=30"
        # Fetch this exact branch so a deleted upstream cannot silently use a stale ref.
        self.git("fetch", "--quiet", "--no-tags", "--", remote, remote_branch, timeout=120)
        target = self.git("rev-parse", "--verify", "FETCH_HEAD^{commit}")
        if head != target:
            ancestor = self.run(
                ["git", "-c", "core.hooksPath=/dev/null", "merge-base", "--is-ancestor", head, target],
                quiet=True, check=False,
            )
            if ancestor.returncode:
                raise RuntimeError(
                    f"Cannot fast-forward {branch} to {remote}/{remote_branch}: "
                    "checkout is ahead, diverged, or ancestry could not be verified"
                )
            log(f"Updating {head[:12]} to {target[:12]} from {remote}/{remote_branch}")
            self.git("merge", "--ff-only", "--no-autostash", "--no-edit", target)

        if state["deployed_commit"] == target and not state["pending"] and binary.is_file():
            return

        # Keep a pending flag even when rebuilding the SAME commit for a missing binary.
        state["pending"] = True
        write_json(state_path, state)
        log(f"Building core_server at {target[:12]}")
        self.run(["make", "core_server"])
        if not binary.is_file():
            raise RuntimeError("make core_server succeeded but core_server is missing")
        if self.git("rev-parse", "HEAD") != target:
            raise RuntimeError("HEAD changed during the build; deployment will retry next run")
        service = f"zns.{self.root.name}.service"
        log(f"Restarting {service}")
        self.run(["systemctl", "restart", service])
        self.run(["systemctl", "is-active", "--quiet", service])
        write_json(state_path, {"deployed_commit": target, "pending": False})
        log(f"Deployed {target[:12]} to {service}")

    def cleanup(self):
        state_path = self.state_dir / "cleanup-attempt.json"
        last_attempt = read_json(state_path)
        now = time.time()
        if last_attempt is not None:
            if (type(last_attempt) not in (int, float) or not math.isfinite(last_attempt)
                    or last_attempt < 0):
                raise RuntimeError(f"Invalid cleanup state: {state_path}")
            if 0 <= now - last_attempt < CLEANUP_INTERVAL:
                return
        # Record the attempt first, including failed or interrupted scans.
        write_json(state_path, now)
        failed = False
        for relative_path, days in CLEANUP_TARGETS:
            directory = self.root / relative_path
            if not directory.exists():
                continue
            log(f"Cleaning {directory}: folders older than {days} days")
            try:
                self.run([
                    "uv", "run", "--quiet", "--script", str(self.root / "dev/delete_old_folders.py"),
                    "--delete", "--yes", "--age", str(days), str(directory),
                ])
            except FAILURES as error:
                log(f"Cleanup failed for {directory}: {error}", error=True)
                failed = True
        if failed:
            raise RuntimeError("Daily cleanup had errors; next attempt is in 24 hours")

    def execute(self):
        with (self.state_dir / "lock").open("a") as lock:
            try:
                fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            except BlockingIOError:
                return 0
            failed = False
            for label, action in (("Update", self.update), ("Cleanup", self.cleanup)):
                try:
                    action()
                except FAILURES as error:
                    log(f"{label} failed: {error}", error=True)
                    failed = True
            return int(failed)


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.parse_args()
    try:
        return Maintenance(Path(__file__).resolve().parents[1]).execute()
    except FAILURES as error:
        log(f"Maintenance failed: {error}", error=True)
        return 1
    except KeyboardInterrupt:
        log("Maintenance interrupted", error=True)
        return 1


if __name__ == "__main__":
    sys.exit(main())
