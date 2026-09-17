# Repository documentation

Start here when working on `_core`, the shared firmware and tooling repository
for Zeptocore, Zeptoboard, Ectocore, and Ezeptocore. Run the commands below from
the repository root. Source and artifact paths written in the development notes
also refer to the repository root; Markdown links are relative to each document.

The short orientation note is [readme-starthere.md](readme-starthere.md).

SDK upgrade and validation: [Pico SDK 2.3.1 and connected hardware validation](pico-sdk-upgrade.md).

Current performance controls: [Zeptocore A/B knob mappings](zeptocore-controls.md).

## Reading order

1. Use the repository map and build instructions below to locate the relevant code.
2. Read [seek-map design](seek-map-design.md) for allocation maps, persistence,
   memory limits, and filesystem ownership.
3. Read [implementation status](seek-implementation-status.md) and
   [hardware results](seek-hardware-results.md) for the original validation record,
   together with the [subsequent review notes](#subsequent-review-notes) below.
4. Use [diagnostics](seek-diagnostics.md) when investigating runtime behavior,
   then follow the topic-specific documents in the index.
5. Consult the [implementation brief](seek-implementation-brief.md) for the original requirements and acceptance criteria.
   It is a historical implementation brief, including session-specific hardware
   and completion statements.

## Repository map

| Location | Role |
| --- | --- |
| [main.c](../main.c), [lib/includes.h](../lib/includes.h), [lib/globals.h](../lib/globals.h) | Firmware startup, module integration, and shared state. |
| [lib/](../lib/) | Device wrappers, controls, audio rendering, DSP, media lifecycle, and drivers. Many implementations live in headers. |
| [lib/audio_callback.h](../lib/audio_callback.h), [lib/realtime_stretch.h](../lib/realtime_stretch.h) | Normal playback and time-stretch rendering. |
| [lib/audio_seek_map.c](../lib/audio_seek_map.c), [lib/audio_media.h](../lib/audio_media.h), [lib/audio_media_control.h](../lib/audio_media_control.h), [lib/audio_media_owner.c](../lib/audio_media_owner.c) | Persistent map/cache management, playback file lifecycle, foreground integration, and cross-core filesystem ownership. |
| [lib/sdio/](../lib/sdio/) | Bundled FatFs and SD support; allocation validation and mapped seeking are implemented in [ff.c](../lib/sdio/ff15/source/ff.c). |
| [lib/my_pico_audio/](../lib/my_pico_audio/), [lib/my_pico_audio_i2s/](../lib/my_pico_audio_i2s/) | Audio buffers, format conversion, I2S/DMA output, and restart queue integration. |
| [core/](../core/) | Go application and sample-preparation/web-server tooling. |
| [dev/](../dev/) | Development utilities, audio/header generators, and device tools. |
| [scripts/](../scripts/) | Host-side build comparisons, diagnostics, hardware workloads, and capture analysis. |
| [test/](../test/) | Native firmware tests and Python protocol tests. Older standalone module tests also live in [lib/test/](../lib/test/). |
| [Makefile](../Makefile), [CMakeLists.txt](../CMakeLists.txt), [lib/cmake](../lib/cmake/) `*_compile_definitions*.cmake` | Build targets, feature switches, and device/buffer configurations. |
| [schematics/](../schematics/) | Hardware schematics. |
| [docs/](./) | Development notes and legacy Hugo site sources/assets. The site's `public/` output is generated; top-level development notes are ordinary Markdown. |

## Build and test

The standard Zeptocore build is:

```sh
make zeptocore
```

It produces `zeptocore.uf2` and `build/_core.elf`, using the 441-frame device
configuration. `make zeptocore_256` selects the supported 256-frame configuration.
Other device targets are listed in the [Makefile](../Makefile). These targets
share `build/` and `lib/cmake/target_compile_definitions.cmake`; use separate CMake build
directories and `CORE_COMPILE_DEFINITIONS` for simultaneous configurations, as
shown in the [diagnostics build instructions](seek-diagnostics.md#build-and-run).

The Makefile downloads Pico SDK 2.3.1 and Pico Extras `sdk-2.3.1`, creates a Python
3.11 `.venv` with `uv`, and generates DSP/audio headers when absent. Host tools
include CMake, Make, ARM GCC with newlib, Git, `uv`, `clang-format`, Go, and SoX.
The [firmware CI workflow](../.github/workflows/build.yml) documents its Linux
setup, including the TinyUSB patch used by CI. Building firmware does not flash it.

Make checks existing SDK/extras checkouts against the pinned tags and initialized
submodules. It reports a mismatch without overwriting local changes. For an older
installation, move the old SDK, extras, and build directories into a uniquely
named backup under `artifacts/`, then run `make zeptocore` to fetch the pinned
dependencies and configure a fresh build. Direct CMake builds can select other
SDK/extras paths for comparisons; use a separate build directory for each pair.

Run the relevant native suites after building the generated headers:

```sh
.venv/bin/python test/fatfs_seek/run.py
.venv/bin/python test/seek_diagnostics/run.py
.venv/bin/python test/audio_source/run.py
.venv/bin/python test/dsp_loops/run.py
.venv/bin/python test/midi/run.py
```

These runners invoke host `cc`/`c++` with sanitizers. The FatFs runner uses GNU
linker `--wrap=calloc`, which Apple's linker does not support; the diagnostics
runner also encounters Mach-O section/Clang compatibility issues on macOS.
A successful macOS firmware build does not establish that these native suites
pass unchanged. The earlier review used temporary host-portability adaptations
for some tests; those adaptations are not part of the repository.

On 2026-09-16, `make zeptocore` passed on macOS at `c5f884b` (PR #830 merged),
with three conversion warnings in `lib/mcp23017/mcp23017_lib.c`. No hardware
flashing or playback validation was performed during that build.

## Current seek/audio behavior

The fast-seeking work was merged in PR #830. Maps describe contiguous runs of
filesystem clusters; they do not cache audio data. Startup validates saved
allocation chains, rebuilds missing/changed maps, and persists recoverable index
generations. Every open must attach a current-mount map because FatFs clears
`FIL.cltbl` when opening a file.

- Limits are 256 indexed physical WAVs, two RAM map entries, and 64 DWORDs
  (31 contiguous runs) per map. Excess files or fragmentation use ordinary seeks.
- File duration alone does not determine map size. Allocation validation still
  walks filesystem metadata on each mount, so long files can increase startup work.
- The live file's map is pinned. A runtime cache miss uses ordinary seeking and
  queues work until playback is stopped/muted, effect tails are disabled, bass
  voices are idle, and filesystem ownership has been acknowledged.
- The audio core owns filesystem access during playback. Foreground filesystem
  work uses the shared ownership handoff; optional mapping does not justify an
  uncoordinated SD operation during rendering.
- Normal builds enable seek-map attachment. Diagnostics, detailed timing, test
  controls/fixtures, next-file preparation, and extra output buffering default off.
- Clock restart correction and wake default on for the Ectocore/Ezeptocore path;
  immediate wake is limited to the 256-frame core-1 worker configuration.
- The same series also fixes the pitch/retrigger source-buffer overflow and
  optimizes delay/filter processing. See the documents below for each change.

The constants and feature flags are defined in
[audio_seek_map.h](../lib/audio_seek_map.h), [CMakeLists.txt](../CMakeLists.txt),
and [audio_restart.h](../lib/audio_restart.h).

## Subsequent review notes

The following findings from the follow-up review remain present at `c5f884b`.
They qualify the original documents' completion claims; no fixes were applied
during this documentation consolidation.

- In [audio_seek_map.c](../lib/audio_seek_map.c), `identity()` hashes `fs->bitbase`
  for every filesystem, although FatFs initializes that field for exFAT. The
  native FAT remount test reproduced unnecessary map rebuilds/index writes when
  the unused field differed. Zero-initialized startup storage can mask this.
- In [ff.c](../lib/sdio/ff15/source/ff.c), `ff_clmt_inspect()` checks `obj.n_frag`
  and `obj.stat` without restricting the exFAT allocation-state checks to exFAT.
  The native benchmark reproduced rejection of an otherwise valid FAT file.
- The optional 3784-byte map arena is allocated after reverb initialization but
  before later mandatory DSP/control allocations. The review identified missing
  heap-reserve protection as a risk; a resulting hardware failure was not reproduced.

Temporary copies with the two FAT/exFAT guards passed the FatFs suite after
the host-portability adaptations mentioned above. That result does not mean
the unmodified checkout passed every native suite or that hardware was retested.

## Development document index

All 14 Markdown documents introduced by PR #830 are collected directly in this
folder, including the relocated implementation brief. Keep detailed evidence in
the topic documents and use this index for orientation.

| Document | Purpose |
| --- | --- |
| [MIDI slice triggering](midi-slice-triggering.md) | Channel 1 note mapping, transport/mute behavior, Ableton routing, and native tests. |
| [Implementation brief](seek-implementation-brief.md) | Original scope, requirements, integration checklist, and acceptance/completion record. |
| [Seek-map design](seek-map-design.md) | Memory policy, durable index, allocation validation, filesystem ownership, and mapped execution. |
| [Implementation status](seek-implementation-status.md) | Original integration audit, correctness coverage, hardware evidence, and limitations. |
| [Hardware results](seek-hardware-results.md) | Zeptocore map-on/off comparisons, persistence checks, memory use, and supported buffer sizes. |
| [Diagnostics usage](seek-diagnostics.md) | Build/run instructions, SWD service, captures, metrics, and interpretation. |
| [Diagnostics plan](seek-diagnostics-plan.md) | Original measurement protocol and instrumentation-overhead acceptance gate. |
| [Ectocore hardware](seek-ectocore-hardware.md) | Device-specific validation and follow-up measurements. |
| [Detailed timing](seek-detailed-timing.md) | Callback-stage profiling and remaining starvation cases. |
| [Freeze fix](seek-freeze-fix.md) | Retrigger/pitch stack overflow, bounded source buffer, and playback-rate limits. |
| [DSP optimization](seek-dsp-optimization.md) | Exact fixed-point multiplication and delay/filter loop optimizations. |
| [Clock latency](ectocore-clock-latency.md) | Measurements before the clock restart correction. |
| [Clock restart fix](ectocore-clock-restart-fix.md) | Phase correction, fade, immediate renderer wake, and measured results. |
| [Next-file experiment](seek-next-file-experiment.md) | Staged file preparation experiment; disabled by default. |
| [Output-buffer experiment](seek-output-buffer-experiment.md) | Additional buffering and its latency tradeoff; disabled by default. |

Hardware capture and firmware paths under `artifacts/seek/` are historical
evidence references. `artifacts/` is ignored by Git and may be absent in a fresh
checkout. Verify the relevant build, device, and available evidence before
reusing a measurement or assuming a previously described hardware setup.

Older references include the [Ectocore quickstart](ectocore_quickstart.md) and
[Zeptocore specification](../dev/zeptocore_spec.md). Product guides, downloads,
and licensing are linked from the [root README](../README.md).
