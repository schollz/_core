"""Standalone-only native releases; Python 3.9+, no third-party packages.

Like Tape: fresh source/build directories, local Keychain signing, notarization,
platform-specific archives/manifests/checksums, and gh publication. These scripts
do not execute the application or tests. Assets attach to the latest existing release.
"""
from __future__ import annotations

import argparse
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import platform
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile

APP_ROOT = Path(__file__).resolve().parents[1]
ROOT = APP_ROOT.parent
PRODUCT = "zeptocore visualizer"
IDENTITY = "Developer ID Application: Zackary Scholl (KF253X8W3N)"


def run(args):
    # Do not echo arguments: notarytool may receive an app-specific password.
    result = subprocess.run([str(a) for a in args], cwd=ROOT, text=True,
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    output = result.stdout
    for key in ("APPLE_PASSWORD", "GH_TOKEN", "GITHUB_TOKEN"):
        if os.environ.get(key):
            output = output.replace(os.environ[key], "<redacted>")
    if result.returncode:
        raise RuntimeError(f"{args[0]} failed ({result.returncode}):\n{output}")
    return output


def sha(path):
    with path.open("rb") as stream:
        digest = hashlib.sha256()
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def snapshot(destination):
    # Enumerate only application inputs; never transfer the repository or keys.
    destination.mkdir()
    inputs = [APP_ROOT / name for name in ("CMakeLists.txt", "LICENSE", "README.md")]
    for folder in ("Source", "Resources", "Tests"):
        inputs.extend(p for p in (APP_ROOT / folder).rglob("*") if not p.is_dir())
    manifest = {}
    for path in inputs:
        if path.is_symlink() or any(p.is_symlink() for p in path.parents if p != ROOT):
            raise RuntimeError(f"Source symlink is not supported: {path}")
        relative = path.relative_to(APP_ROOT)
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        manifest[str(relative)] = sha(target)
    cached = APP_ROOT / ".cache/JUCE-9.0.2.tar.gz"
    if cached.is_file():
        (destination / ".cache").mkdir()
        shutil.copy2(cached, destination / ".cache" / cached.name)
    return manifest


def release_info(repo):
    # Fail if no latest release exists; never create a release or infer its tag.
    release = json.loads(run(["gh", "api", f"repos/{repo}/releases/latest"]))
    if (not release.get("tag_name") or release.get("draft") or release.get("immutable")):
        raise RuntimeError("The latest release is missing, draft or immutable")
    return release


def publish(repo, assets, pinned):
    current = release_info(repo)
    if current["id"] != pinned["id"] or current["tag_name"] != pinned["tag_name"]:
        raise RuntimeError("Release changed while building; artifacts retained locally")
    run(["gh", "release", "upload", "--repo", repo, "--clobber", "--", current["tag_name"], *assets])
    current = json.loads(run(["gh", "api", f"repos/{repo}/releases/{pinned['id']}"]))
    uploaded = {a["name"]: a for a in current["assets"]}
    for asset in assets:
        remote = uploaded.get(asset.name, {})
        if remote.get("size") != asset.stat().st_size or remote.get("digest") != "sha256:" + sha(asset):
            raise RuntimeError("Uploaded asset size/hash mismatch: " + asset.name)


def notarize(archive, profile):
    credentials = ["--keychain-profile", profile] if profile else [
        "--apple-id", os.environ["APPLE_ID"], "--team-id", os.environ["TEAM_ID"],
        "--password", os.environ["APPLE_PASSWORD"]]
    result = json.loads(run(["xcrun", "notarytool", "submit", archive, *credentials,
                             "--wait", "--output-format", "json"]))
    if result.get("status") != "Accepted":
        raise RuntimeError("Notarization was not accepted: " + str(result.get("id")))


def main(system, architecture, minimum=None, remote_intel=False):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--version", default="1.0.0")
    parser.add_argument("--repo", default="schollz/_core")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--no-upload", action="store_true")
    parser.add_argument("--notary-profile", default=os.environ.get("NOTARY_PROFILE"))
    if remote_intel:
        import remote_intel as intel
        parser.add_argument("remote", nargs="?", default=intel.DEFAULT_REMOTE)
        parser.add_argument("--keep-remote", action="store_true")
    args = parser.parse_args()
    output = None
    remote_dir = None
    complete = False
    try:
        if platform.system() != system or (not remote_intel and platform.machine() != architecture):
            raise RuntimeError(f"Run natively on {system} {architecture}")
        if remote_intel and not re.fullmatch(r"(?:[A-Za-z0-9_.-]+@)?[A-Za-z0-9][A-Za-z0-9_.-]*", args.remote):
            raise ValueError("Invalid SSH host")
        if not re.fullmatch(r"(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)", args.version):
            raise ValueError("Version must be major.minor.patch")
        if any(int(v) > 255 for v in args.version.split(".")) or not 1 <= args.jobs <= 32:
            raise ValueError("Version components must be 0..255; jobs must be 1..32")
        if not re.fullmatch(r"[\w.-]+/[\w.-]+", args.repo):
            raise ValueError("Invalid GitHub owner/repository")
        if system == "Darwin" and not args.notary_profile:
            for key in ("APPLE_ID", "TEAM_ID", "APPLE_PASSWORD"):
                if not os.environ.get(key):
                    raise ValueError(f"Set {key} or NOTARY_PROFILE before releasing")
        if remote_intel:
            for tool in ("ssh", "rsync", "ditto", "codesign", "security", "lipo", "otool", "xcrun", "spctl"):
                if not shutil.which(tool):
                    raise RuntimeError("Missing local tool: " + tool)
            identity = os.environ.get("SIGN_IDENTITY", IDENTITY)
            if not identity.startswith("Developer ID Application:") or identity not in run(
                    ["security", "find-identity", "-v", "-p", "codesigning"]):
                raise RuntimeError("Developer ID Application identity/private key unavailable on this Mac")
        pinned = None if args.no_upload else release_info(args.repo)
        tag = pinned["tag_name"] if pinned else None
        commit = run(["git", "rev-parse", "HEAD"]).strip()
        dirty = bool(run(["git", "status", "--porcelain", "--", "visualizer-juce"]).strip())
        label = ("macOS" if system == "Darwin" else "Linux") + "-" + architecture
        parent = APP_ROOT / "release-output" / label
        parent.mkdir(parents=True, exist_ok=True)
        output = Path(tempfile.mkdtemp(prefix=args.version + "-", dir=parent))
        source, build = output / "source", output / "build"
        print("Release working directory: " + str(output), flush=True)
        inputs = snapshot(source)
        configure = ["cmake", "-S", source, "-B", build, "-G",
                     "Ninja" if shutil.which("ninja") else "Unix Makefiles",
                     "-DCMAKE_BUILD_TYPE=Release", "-DVISUALIZER_STANDALONE_ONLY=ON",
                     "-DVISUALIZER_VERSION=" + args.version]
        if minimum:
            configure += ["-DCMAKE_OSX_ARCHITECTURES=" + architecture,
                          "-DCMAKE_OSX_DEPLOYMENT_TARGET=" + minimum]
        print("Configuring and building standalone", flush=True)
        if remote_intel:
            remote_dir = intel.ssh(run, args.remote, "mktemp -d /tmp/zeptocore-release-intel.XXXXXX").strip()
            if not intel.REMOTE_PATH.fullmatch(remote_dir):
                raise RuntimeError("Unexpected remote temporary directory")
            print("Intel builder: " + args.remote + ":" + remote_dir, flush=True)
            built = intel.build(run, args.remote, remote_dir, source, output, args.version, args.jobs)
        else:
            run(configure)
            run(["cmake", "--build", build, "--config", "Release", "--parallel", str(args.jobs),
                 "--target", "zeptocore_visualizer_Standalone"])
            built = build / "zeptocore_visualizer_artefacts/Release/Standalone"
        payload = output / "payload"
        payload.mkdir()
        app = payload / (PRODUCT + (".app" if minimum else ""))
        if minimum:
            shutil.copytree(built / app.name, app, symlinks=True)
        else:
            shutil.copy2(built / app.name, app)
        if remote_intel:
            intel.check_bundle(run, app, args.version)
        notices = payload / "Notices"
        notices.mkdir()
        for name, path in {
            "LICENSE.txt": source / "LICENSE",
            "IBM-Plex-LICENSE.txt": source / "Resources/Fonts/IBM-Plex-LICENSE.txt",
            "Font-Awesome-LICENSE.txt": source / "Resources/Fonts/Font-Awesome-LICENSE.txt",
            "JUCE-LICENSE.md": (output / "JUCE-LICENSE.md" if remote_intel else source / ".cache/deps/juce-src/LICENSE.md"),
        }.items():
            shutil.copy2(path, notices / name)
        (payload / "README.txt").write_text(
            f"Zeptocore Visualizer {args.version} ({label})\n\n"
            "Extract this archive and open zeptocore visualizer. Choose the reference\n"
            "folder containing bank1, bank2, etc., then connect your device via MIDI.\n"
            "Visualizer-enabled firmware is required for slice tracking.\n"
            + ("Copy the .app to /Applications if desired.\n" if minimum else
               "Requires the system libraries used by JUCE (ALSA, X11, FreeType, Fontconfig).\n"
               "Run ./\"zeptocore visualizer\" on a graphical Linux desktop.\n")
        )
        prefix = f"zeptocore-visualizer-{args.version}-{label}"
        archive = output / (prefix + "-Standalone.zip")
        if minimum:
            print("Signing and notarizing standalone", flush=True)
            run(["codesign", "--force", "--options", "runtime", "--timestamp", "--sign",
                 os.environ.get("SIGN_IDENTITY", IDENTITY), app])
            run(["codesign", "--verify", "--deep", "--strict", app])
            submission = output / "notarization.zip"
            run(["ditto", "-c", "-k", "--keepParent", app, submission])
            notarize(submission, args.notary_profile)
            run(["xcrun", "stapler", "staple", app])
            run(["xcrun", "stapler", "validate", app])
            run(["spctl", "--assess", "--type", "execute", app])
            run(["ditto", "-c", "-k", payload, archive])
        else:
            with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as bundle:
                for path in sorted(payload.rglob("*")):
                    if path.is_file():
                        bundle.write(path, path.relative_to(payload))
        manifest = output / (prefix + "-manifest.json")
        manifest.write_text(json.dumps({
            "version": args.version, "tag": tag, "commit": commit, "dirty": dirty,
            "source_sha256": inputs, "system": system, "architecture": architecture,
            "remote_builder": args.remote if remote_intel else None,
            "minimum_macos": minimum, "signed": bool(minimum), "notarized": bool(minimum),
            "built_at": datetime.now(timezone.utc).isoformat(), "tests_run": False,
            "assets": {archive.name: sha(archive)},
        }, indent=2) + "\n")
        sums = output / (prefix + "-SHA256SUMS.txt")
        sums.write_text("".join(sha(p) + "  " + p.name + "\n" for p in (archive, manifest)))
        assets = [archive, manifest, sums]
        if not args.no_upload:
            print("Publishing " + tag, flush=True)
            publish(args.repo, assets, pinned)
        (output / "complete.json").write_text(json.dumps({
            "uploaded": not args.no_upload, "assets": [p.name for p in assets]}, indent=2) + "\n")
        print("Standalone release ready: " + str(output), flush=True)
        complete = True
        return 0
    except (OSError, ValueError, RuntimeError) as error:
        print(str(error), file=sys.stderr)
        if output:
            print("Incomplete output retained: " + str(output), file=sys.stderr)
        return 1
    finally:
        if remote_intel and remote_dir and intel.REMOTE_PATH.fullmatch(remote_dir):
            if complete and not args.keep_remote:
                try:
                    intel.ssh(run, args.remote, "rm -rf -- " + remote_dir)
                except (OSError, RuntimeError) as error:
                    print("Remote cleanup failed: " + str(error), file=sys.stderr)
            else:
                print(f"Remote build retained: {args.remote}:{remote_dir}", file=sys.stderr)
