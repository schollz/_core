#!/usr/bin/env python3
"""Build and retain matched zeptocore map-on/map-off ELFs; does not flash."""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
from pathlib import Path
import shutil
import subprocess


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--out", type=Path, required=True)
    p.add_argument("--jobs", type=int, default=4)
    p.add_argument("--parallel", type=int, default=2)
    p.add_argument("--observer", action="store_true",
                   help="Build 441/256-frame paired diagnostics-on/off configurations with the independent timing witness")
    args = p.parse_args()
    root = Path(__file__).resolve().parents[1]
    args.out.mkdir(parents=True, exist_ok=False)

    def build(frames, clock, attach):
        name = f"{frames}-{clock}-{'on' if attach else 'off'}"
        artifact = args.out/name
        artifact.mkdir()
        directory = root/f"build-seek-map-{'observer' if args.observer else 'matrix'}-{name}"
        suffix = "" if frames == 441 else f"_{frames}"
        configure = ["cmake", "-S", str(root), "-B", str(directory),
            f"-DPICO_SDK_PATH={root/'pico-sdk'}", f"-DPICO_EXTRAS_PATH={root/'pico-extras'}",
            f"-DCORE_COMPILE_DEFINITIONS={root/f'zeptocore_compile_definitions{suffix}.cmake'}",
            f"-DSEEK_DIAGNOSTICS={'ON' if not args.observer or attach else 'OFF'}", "-DSEEK_TEST_CONTROLS=ON",
            f"-DSEEK_TIMING_WITNESS={'ON' if args.observer else 'OFF'}",
            "-DSEEK_TEST_FRESH_INDEX=OFF", "-DSEEK_TEST_FRAGMENT_BENCH=OFF",
            "-DSEEK_TEST_AUDIO_FIXTURE=0",
            f"-DSEEK_MAP_ATTACH={'ON' if args.observer or attach else 'OFF'}",
            f"-DCORE_NO_OVERCLOCK={'ON' if clock == 125 else 'OFF'}", "-DCMAKE_BUILD_TYPE=Release"]
        compile_command = ["cmake", "--build", str(directory), f"-j{args.jobs}"]
        for command, log in [(configure, "configure.log"), (compile_command, "build.log")]:
            with (artifact/log).open("w") as output:
                subprocess.run(command, cwd=root, stdout=output, stderr=subprocess.STDOUT, check=True)
        for source, dest in [("_core.elf", "firmware.elf"), ("_core.elf.map", "firmware.map")]:
            shutil.copyfile(directory/source, artifact/dest)
        manifest = {"frames": frames, "clock_mhz": clock, "attachment_enabled": args.observer or attach,
            "diagnostics_enabled": not args.observer or attach, "timing_witness": args.observer,
            "configure": configure, "build": compile_command,
            "elf_sha256": hashlib.sha256((artifact/"firmware.elf").read_bytes()).hexdigest()}
        (artifact/"manifest.json").write_text(json.dumps(manifest, indent=2)+"\n")
        return {"name": name, **manifest}

    results = []
    with ThreadPoolExecutor(max_workers=args.parallel) as pool:
        configurations = [(441, 225), (256, 225)] if args.observer else [
            (frames, clock) for frames in [441, 256] for clock in [225, 125]]
        futures = [pool.submit(build, frames, clock, attach)
                   for frames, clock in configurations for attach in [True, False]]
        for future in as_completed(futures):
            item = future.result();results.append(item)
            print(json.dumps({"built": item["name"], "elf_sha256": item["elf_sha256"]}), flush=True)
    (args.out/"manifest.json").write_text(json.dumps(results, indent=2)+"\n")


if __name__ == "__main__":
    main()
