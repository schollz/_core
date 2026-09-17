# Local validation — 2026-09-17

Environment: Apple Silicon (arm64), macOS 26.5.2, AppleClang 17, Xcode,
CMake/Ninja, pinned JUCE 9.0.2. These are local builds; older macOS versions,
Intel, Windows, and Linux are not qualified in this phase.

## Passed

- Debug standalone and Release standalone, VST3, and AU builds.
- `make test`: both native contract and native MIDI integration suites pass.
- Protocol validation, version-2 effects, physical-pad filtering, incomplete SysEx
  rejection, stale expiry, heartbeats, retriggers, serial wrap, direction, tempo
  changes, stop, all five playback modes, and bounded loading transitions.
- Browser parity baselines captured from the unchanged TypeScript source: exact
  PCM peak hashes and slice metadata; all spectrum bytes within one quantization
  step, for mono/stereo at both 44.1 and 88.2 kHz.
- Metadata and WAV error rejection, independent stereo power, spectrum
  interpolation, persistent-cache reuse, corrupt-cache regeneration, changed
  metadata, sample deletion, missing folders, and cancellation/teardown.
- Float/double, mono/stereo passthrough and bypass are bit exact at 1, 64, 257,
  and 1024 frames. Zero latency/tail, host state restoration, malformed-state
  rejection, repeated editor creation, and saved-size restoration pass.
- Actual CoreMIDI virtual endpoints exercise incoming SysEx/pads, outgoing lease
  and legacy queries, shared-instance subscription ownership, disconnect,
  reconnect, and shutdown. The test waits for asynchronous device enumeration.
- Objective-C++ implementation symbols are hidden, preventing JUCE symbol
  collisions when a Release plugin is loaded by a Debug JUCE host.
- The actual Release VST3 and AU load in a separate JUCE host probe. Audio,
  state round-trip, and repeated editor opening pass through the real wrappers.
- Apple's `auval -v aufx ZpVw INFD -strict` succeeds. The temporary AU installation
  used by validation was removed afterward.
- `make install` installs and verifies the Release VST3 at
  `~/Documents/vsts/zeptocore visualizer.vst3`.
- A fresh source copy outside `_core` configures and builds using only its own
  cached JUCE archive. The resulting app also runs its embedded tests after being
  copied outside that source/build directory, with `/tmp` as its working directory.
- Ad-hoc bundle signatures verify. `otool -L` shows only macOS system frameworks
  and system libraries, with no Homebrew, Node, or checkout-local runtime libraries.
- Native window, MIDI setup controls, and asynchronous folder picker inspected.
  Rendered fixtures cover playing, reverse, muted, stopped, stale, effect masks,
  pad callouts, compact layout, MIDI setup, and missing-reference presentation.
  Screenshots are overwritten cleanly on repeated diagnostic runs.

Local logs and rendered images are in ignored `test-results/`. The optional
`VISUALIZER_PLUGIN_PROBE` target is a test host, not a distributed dependency.

## Remaining live-device acceptance

A physical zeptocore with telemetry firmware and a matching SD-card reference
copy were not available during this implementation. Check the following together
before declaring hardware performance parity:

1. Choose the matching reference root; verify real-bank preparation and reuse.
2. Connect the device and exercise bank/sample changes and all playback modes.
3. Check repeated pad presses, tempo, reverse, mute/stop, and effect highlights
   against the original kiosk visualizer.
4. Unplug/replug USB and reopen the plugin editor while performing.

The automated virtual-device tests cover the native MIDI transport but do not
substitute for this hardware comparison. The display remains an estimate of
reference-source position and spectrum, not a measurement of device audio.
