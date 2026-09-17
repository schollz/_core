# zeptocore visualizer — JUCE

A native port of the zeptocore browser visualizer, with a standalone macOS app,
VST3, and Audio Unit. The original `visualizer` kiosk project and device firmware
are unchanged. This directory is an independent CMake project and can be copied
out of the parent repository.

## Build and use

On this laptop, Xcode Command Line Tools, CMake, Ninja, and Make are sufficient:

```sh
cd /Users/zns/Documents/_core/visualizer-juce
make run
```

JUCE **9.0.2** is downloaded once, verified by SHA-256, and cached in `.cache`.
An existing `.cache/JUCE-9.0.2.tar.gz` also supports offline configuration.
No Node, npm, browser, local web server, tape project, or font tooling is needed.

1. Click **CHOOSE FOLDER** and select your reference SD-card copy: the folder
   immediately containing `bank1`, `bank2`, etc.
2. Plug in a zeptocore with visualizer telemetry enabled. A unique matching MIDI
   input/output pair connects automatically; otherwise use **CONNECT/MIDI** to
   select both ports. Device changes are checked every two seconds.
3. Perform on the zeptocore. The app shows the selected waveform, slice,
   estimated playhead, effect state, physical-pad callouts, and source spectrum.

**FOLDER** changes the reference, **RESCAN** reloads changed files, and the MIDI
panel contains a reduced-motion option; macOS Reduce Motion is also respected.
Folder and port preferences persist.
A missing or moved reference prompts you to choose its new location.
The standalone opens no audio device and does not request microphone access.

```sh
make build         # Debug standalone, incremental
make run           # Build and launch
make vst3          # VST3 bundle
make au            # macOS Audio Unit bundle
make plugins       # Both plugins
make install       # Release VST3 -> ~/Documents/vsts
make release       # Release app + VST3 + AU
make test          # Native contracts and real CoreMIDI virtual-port integration
make visual-check  # Render deterministic UI fixtures under test-results/visuals
```

`CONFIG`, `BUILD_DIR`, `JOBS` (default 4), `CMAKE_FLAGS`, and `VST3_INSTALL_DIR`
are overridable. `make install` replaces only this plugin's destination bundle.
AU installation is manual: copy the `.component` into
`~/Library/Audio/Plug-Ins/Components` and restart the host if required.

Release outputs:

```text
build-release/zeptocore_visualizer_artefacts/Release/
  Standalone/zeptocore visualizer.app
  VST3/zeptocore visualizer.vst3
  AU/zeptocore visualizer.component
```

The app and plugins include their fonts and icons, and local builds are ad-hoc
signed and verified. The reference copy stays external and is never modified.
The app can be moved outside the checkout. Developer ID signing, notarization,
installers, cross-platform qualification, and GitHub Actions are deferred.

## Reference and playback contract

Banks are `bank1`–`bank16`; original samples are `0.0.wav`–`15.0.wav` with matching
`.wav.info` metadata. Stretched `.1.wav` companions, settings, and savefiles are
ignored. WAVs must be mono/stereo 16-bit PCM at 44.1 or 88.2 kHz with half-second
padding at both ends. Invalid samples report their own error while other samples
remain usable. A folder rescan is explicit; the app does not watch or write SD files.

Preparation runs off the UI and audio threads, prioritizes the current sample,
and caches derived data in
`~/Library/Caches/com.infinitedigits.zeptocorevisualizer` on macOS. The cache uses
canonical root paths, source sizes and high-resolution modification/change times,
versioned data, checksums, and atomic replacement. Cache failures do not prevent
visualization. Old or corrupt entries regenerate. Preferences live in
`~/Library/com.infinitedigits.zeptocorevisualizer/settings.json`.

The MIDI protocol, 500 ms lease renewal, 1500 ms expiry, legacy fallback, loading
estimates, playback modes, retriggers, reverse, and tempo matching follow the
original visualizer. Only complete `view=1`, `view=2`, and `info` SysEx messages
are accepted. Performance-pad Note On messages on channels 1–3 trigger callouts;
automatic slice changes do not. Effects use the original bit order and glyphs.

This is an **estimated source visualization**, not analysis of device audio.
Pitch processing, effects on the audio signal, and output latency are not modelled.
The spectrum is computed from reference PCM using the original 4096-point Hann
FFT, 32 log bands from 50 Hz–16 kHz, 20 frames/s, and −72–0 dBFS range.
Legacy firmware can show bank/sample information but cannot provide slice tracking.

## Plugins and architecture

Plugins use direct USB MIDI; DAW MIDI routing and host transport are not required.
Mono and stereo input pass through unchanged, including double precision and
bypass, with zero latency and no tail. No file, MIDI, UI, allocation, or lock work
runs in `processBlock`. Each plugin saves its reference folder, MIDI selections,
window size, and motion setting in host state. Multiple instances share a single
MIDI subscription per port pair within a process. Each has its own reference
library. The connection survives editor close/reopen and closes with the last
owning processor.

`Core` owns protocol/analysis/playhead logic; `Library` owns background preparation
and disk caching; `Midi` owns bounded callback queues and shared connections;
`Session` joins those on the message thread. `VisualizerView` paints the native
interface. `PluginProcessor` is a passthrough wrapper; `Main` provides an audio-free
standalone window. No firmware or sibling-project code is linked.

## Diagnostics

The app supports `--reference /absolute/folder`, `--self-test`, `--midi-test`,
`--render-fixtures /output/folder`, and
`--analyse /file.wav /file.wav.info /output.json`.

Native tests embed `Tests/parity.json`, captured from the original TypeScript
implementation. They require no Node or sibling checkout. Maintainers can
regenerate that baseline explicitly with Node 22.13+:

```sh
node Tests/capture-parity.mjs ../visualizer Tests/parity.json
```

An optional external host probe exercises the actual VST3/AU wrappers, audio,
saved state, and repeated editor creation:

```sh
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DVISUALIZER_PLUGIN_PROBE=ON
cmake --build build-debug --target visualizer_plugin_probe --parallel 4
build-debug/visualizer_plugin_probe_artefacts/Debug/visualizer_plugin_probe \
  "$PWD/build-release/zeptocore_visualizer_artefacts/Release/VST3/zeptocore visualizer.vst3"
```

The AU must be registered in the standard Components folder before host probing
or `auval -v aufx ZpVw INFD -strict`. See `VALIDATION.md` for this laptop's results
and the remaining live-device acceptance check.
