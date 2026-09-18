"""Build the standalone on Tape's Intel Mac; keep signing on the caller."""
import plistlib
import re
import shlex
import tarfile

DEFAULT_REMOTE = "zns@192.168.0.44"
REMOTE_PATH = re.compile(r"/tmp/zeptocore-release-intel\.[A-Za-z0-9]+")
SSH = ["ssh", "-o", "BatchMode=yes", "-o", "ForwardAgent=no", "-o", "ConnectTimeout=10"]


def ssh(run, remote, script):
    return run([*SSH, remote, "/bin/bash -c " + shlex.quote("set -euo pipefail\n" + script)])


def build_script(directory, version, jobs):
    source, build = directory + "/source", directory + "/build"
    commands = [
        ["cmake", "-S", source, "-B", build, "-G", "Unix Makefiles",
         "-DCMAKE_BUILD_TYPE=Release", "-DVISUALIZER_STANDALONE_ONLY=ON",
         "-DVISUALIZER_VERSION=" + version, "-DCMAKE_OSX_ARCHITECTURES=x86_64",
         "-DCMAKE_OSX_DEPLOYMENT_TARGET=11.6"],
        ["cmake", "--build", build, "--config", "Release", "--parallel", str(jobs),
         "--target", "zeptocore_visualizer_Standalone"],
    ]
    return (
        "export PATH=/usr/local/bin:/opt/homebrew/bin:/usr/bin:/bin:/usr/sbin:/sbin\n"
        'test "$(uname -s)" = Darwin\ntest "$(uname -m)" = x86_64\n'
        "mkdir " + shlex.quote(source) + "\n"
        "tar -xzf " + shlex.quote(directory + "/source.tar.gz") + " -C " + shlex.quote(source) + "\n"
        "/usr/bin/caffeinate -i /bin/bash -c "
        + shlex.quote("set -euo pipefail\n" + "\n".join(shlex.join(c) for c in commands)) + "\n"
        # A ZIP avoids remote shell quoting issues with the app's spaces and
        # preserves bundle symlinks and permissions through ditto on both Macs.
        "ditto -c -k --keepParent "
        + shlex.quote(build + "/zeptocore_visualizer_artefacts/Release/Standalone/zeptocore visualizer.app")
        + " " + shlex.quote(directory + "/app.zip") + "\n"
        "cp " + shlex.quote(source + "/.cache/deps/juce-src/LICENSE.md")
        + " " + shlex.quote(directory + "/JUCE-LICENSE.md") + "\n"
    )


def build(run, remote, directory, source, output, version, jobs):
    archive = output / "source.tar.gz"
    with tarfile.open(archive, "w:gz") as bundle:
        bundle.add(source, arcname=".")
    transport = ["rsync", "-cz", "--no-times", "-e", shlex.join(SSH)]
    run([*transport, archive, f"{remote}:{directory}/source.tar.gz"])
    ssh(run, remote, build_script(directory, version, jobs))
    for name in ("app.zip", "JUCE-LICENSE.md"):
        run([*transport, f"{remote}:{directory}/{name}", output / name])
    built = output / "remote-app"
    built.mkdir()
    run(["ditto", "-x", "-k", output / "app.zip", built])
    return built


def check_bundle(run, app, version):
    with (app / "Contents/Info.plist").open("rb") as stream:
        info = plistlib.load(stream)
    if info.get("CFBundleShortVersionString") != version:
        raise RuntimeError("Returned Intel app has the wrong version")
    executable = info.get("CFBundleExecutable", "")
    if not executable or "/" in executable or executable in (".", ".."):
        raise RuntimeError("Invalid bundle executable")
    binary = app / "Contents/MacOS" / executable
    if run(["lipo", "-archs", binary]).strip() != "x86_64":
        raise RuntimeError("Returned app must contain only x86_64 code")
    loads = run(["otool", "-l", binary])
    minimums = re.findall(r"\bminos\s+(\d+(?:\.\d+)+)", loads)
    if not minimums:
        minimums = re.findall(r"cmd LC_VERSION_MIN_MACOSX\s+cmdsize \d+\s+version (\d+(?:\.\d+)+)", loads)
    if minimums not in (["11.6"], ["11.6.0"]):
        raise RuntimeError("Returned app must target macOS 11.6")
    for line in run(["otool", "-L", binary]).splitlines()[1:]:
        library = line.strip().split(" (", 1)[0]
        if not library.startswith(("/usr/lib/", "/System/Library/", "@rpath/", "@loader_path/", "@executable_path/")):
            raise RuntimeError("Nonportable runtime dependency: " + library)
