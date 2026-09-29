# Core Sample Manager 0.1.0 validation

Validated on 2026-09-29 on Apple Silicon, macOS 26.5.2 (25F84), Apple Clang 17.0.0. Both macOS binaries target macOS 11.0. That target is not evidence of runtime acceptance on an older OS or physical Intel Mac.

Integrated Mac acceptance passed before the old standalone/AU/VST3 entrypoints were removed. No commits, pushes, releases, deployments, firmware flashes or physical-device resets were performed. Windows/Linux runtime qualification and deliberate hardware acceptance remain separate steps.

## Builds and execution

| Check | Result |
| --- | --- |
| Native ARM64 Release build | Passed with pinned JUCE 9.0.3 and static vendored Rubber Band |
| Native CTest | 2/2 passed: `manager_contract` and `native_midi`, 13.89 seconds in the final checkout run |
| Independent source copy | Built under `/tmp/CoreSampleManagerSource-20260929` using only copied application sources and the pinned archive; CTest 2/2 passed in 12.34 seconds |
| Intel Release build | Cross-compiled on Apple Silicon; x86_64 architecture/linkage verified; physical Intel execution not qualified |
| Final package outside checkout | Extracted under `/tmp/CoreSampleManagerDelivery-20260929`; embedded `--self-test` passed with all network access denied |
| Dependencies | Both executables link only Apple system libraries/frameworks; no sibling checkout or external audio-processing runtime |
| Bundle audit | Version 0.1.0, exact requested architecture, Mach-O minimum macOS 11.0 agrees with packaged Launch Services metadata |
| Integrity | Strict ad-hoc codesign verification, every payload hash and both archive SHA-256 values passed |

The final offline self-test reported passing audio, import, manager, recovery, protocol, playback, analysis and library groups, plus 57 base contract checks. These are grouped tests with internal assertions, not 57 registered CTest cases.

## Automated evidence

**Format compatibility.** Independent Python fixtures encode the website/firmware layout without calling the application's writer. They cover mono/stereo, 44.1/88.2 kHz, flags, padding/header layout, slices/types, transients, settings and numeric boundaries. Further checks cover companions, the one-shot/no-tempo-match exception, full banks, protected entries and unknown files.

**Storage.** Tests cover adoption, immutable sources, reopen, duplication, ordering/removal/undo, exclusive settings, stable IDs, writer locking, external conflicts, reconciliation/undo, metadata-only saves without changing WAV modification time, stale work and late online results. Closing/reopening with pending work retains edits. An actual child process exits during a partially applied transaction; reopening rolls back to the completed state.

**Failure recovery.** A read-only destination reports failure. Renaming the project away simulates unavailable storage and checks cached visualization, retained edits, prevention of accidental root recreation and retry after restoration. This does not qualify physical SD-card removal. A separate disposable 16 MiB mounted image was filled to actual `ENOSPC`: completed WAV/metadata hashes and revision stayed unchanged, local recovery retained pending state, and freeing space then Retry reached Ready without rerendering unchanged audio. The image was unmounted and removed.

**Imports and audio.** Embedded fixtures cover WAV, AIFF/AIF, FLAC, MP3, Ogg, XRNI, WAV cues/loops, OP-1 and custom AIFF/Ogg metadata. Corrupt files are rejected; source hashes and tampered-cache repair are checked. Audio tests verify:

- Resampling duration and anti-aliasing: a 30 kHz tone downsampled from 96 kHz to 44.1 kHz has residual RMS below 0.001.
- A two-second 440 Hz stereo tone stretched to three seconds has 132,300 frames within two frames, pitch within 2 Hz and aligned channels. Speed conversion produces approximately 660 Hz.
- Canonical 44-byte PCM headers, −6 dB peaks, circular padding, a 13-frame input and exact silence.
- Stretched attacks within 10 ms of mapped anchors; merging current render settings with retained ordered boundaries/types/transients and original sources.
- Cancellation, companion generation and range validation without numeric wrapping.

**Visualization and device tools.** Migrated protocol, estimated-playback, analysis, library and browser parity fixtures pass. New checks cover Preview transport, absence of invented effects/pads, committed-revision refresh, missing-folder cache use, source switching and disable/re-enable. Actual macOS virtual-MIDI endpoints test the shared service, SysEx/pads, leases, connection loss/reconnection and teardown. Synthetic UF2 tests check family/product/block handling without flashing. Online parsing and stale-revision rejection run locally; no real service request was sent.

**Preferences/UI construction.** Migration imports usable old MIDI/motion/reference-folder preferences, preserves the old file and respects existing new preferences. All three settings views construct. No-op edits preserve revisions and undo history.

## Native UI acceptance

Packaged apps were launched outside the checkout. Native UI checks covered:

- Empty-shell folder chooser, opening a portable folder, file-picker import, reopening and clean Command-Q shutdown.
- Ezeptocore gray, Zeptocore purple, Ectocore blue/logo/runes, product-specific settings, brightness and the seven-by-sixteen effect matrix.
- Whole preview and slice audition, BPM editing/conversion, even/automatic slicing, added/dragged boundaries, zoom and saving to Ready.
- Docked, detached and fullscreen visualization, Device/Preview switching, preview playhead/spectrum, disable/re-enable and no microphone prompt.
- Minimum 1120 × 840 layout, settings and detached/fullscreen controls.
- Shared Device window and disabled flashing until a valid local file/bootloader selection. No device command or flash was executed.

Real macOS file-URL drag/drop used the optional `--drag-drop-acceptance` fixture. Its chip starts an OS external drag of a synthetic Ogg; dropping it enters JUCE's native platform drop handler and the normal import queue. It never calls `filesDropped()` directly. Finder-origin dragging was not separately qualified. The fixture is absent in normal launch.

A native UI session ran with all networking denied: drag import, local slicing, Render BPM conversion, saving to Ready, preview and Preview visualization worked, followed by clean shutdown. The final packaged self-test was repeated under the same network restriction.

## Reproduce

From the repository root on Apple Silicon:

```sh
cmake --preset macos-arm64 -S sample-manager
cmake --build sample-manager/build/macos-arm64 --parallel 6
ctest --test-dir sample-manager/build/macos-arm64 --output-on-failure
python3 sample-manager/Release/package.py --platform macos-arm64 --skip-build
```

After extracting the package, substitute its executable path:

```sh
APP='/path/to/Core Sample Manager.app/Contents/MacOS/Core Sample Manager'
"$APP" --self-test
"$APP" --midi-test
/usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' "$APP" --self-test
python3 sample-manager/Tests/macos-disk-full.py "$APP"
"$APP" --drag-drop-acceptance
```

The disk-full runner creates a bounded disposable image; its test command refuses ordinary folders. Self-tests use temporary isolated state and embedded fixtures. MIDI tests use temporary virtual endpoints. Fixture generators are test utilities, not runtime dependencies.

Ignored local logs in `.cache/`: `ctest-final.log`, `independent-build.log`, `offline-self-test-final.log`, `disk-full.log`, and the final ARM/Intel build and package logs.

## Local packages

These are ad-hoc-signed local builds, not notarized public releases. Each `dist/` directory includes `manifest.json`, `SHA256SUMS.txt` and `complete.json`.

| Platform | Archive under `dist/` | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Apple Silicon | `macos-arm64-0.1.0-20260929T164634Z/Core-Sample-Manager-0.1.0-macos-arm64.zip` | 6,113,995 | `84897b6f2ae61fc69da4078110811049fc2af9cc19c2800546b0a979ed710d0a` |
| Intel | `macos-x86_64-0.1.0-20260929T164634Z/Core-Sample-Manager-0.1.0-macos-x86_64.zip` | 6,760,354 | `7aab20bed0ac5bb1734866577ac4689e1e587b40613bab0b91ce13528dc1b789` |

## Deferred qualification

- Physical Intel execution and runtime acceptance on macOS 11.
- Native Windows/Linux builds and runtime, including static-CRT and bundled-library acceptance. Presets, packaging and a manual Windows artifact workflow are prepared; Mac compilation is not cross-platform verification.
- Actual SD-card playback/removal timing, physical MIDI hardware and deliberate local UF2 copying.
- An authorized live drum-analysis request, service availability and cancellation under real network conditions.
- Developer ID/notarization, public distribution and large-library performance qualification.
