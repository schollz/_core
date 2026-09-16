# zeptocore / waveform

A local, monochrome waveform monitor. The device supplies the selected bank,
sample, and active slice; the browser estimates the playhead between triggers.
The page does not play audio or send performance controls.

IBM Plex Mono (regular, medium, and bold) is vendored as WOFF2 files in
`src/assets`, alongside its SIL Open Font License. UI text and canvas callout
labels use the local font; no font service or CDN is needed.

## Firmware opt-in

Visualizer telemetry is excluded from default firmware builds. To build it and
immediately upload to the connected device through the existing picotool workflow:

```sh
make zeptocore-visualizer
```

This enables telemetry, builds `zeptocore_visualizer.uf2`, then uploads that exact
file without rebuilding. Upload runs only after a successful build; existing
`PICOTOOL` and `UPLOAD_*` options still apply (the UF2 path is selected by this target).

To build without uploading, enable it explicitly:

```sh
make zeptocore ZEPTOCORE_VISUALIZER=ON
# Or the 256-frame variant:
make zeptocore_256 ZEPTOCORE_VISUALIZER=ON
```

Both commands produce `zeptocore_visualizer.uf2`. The no-overclock target produces
`zeptocore_nooverclock_visualizer.uf2`. Default builds retain their existing names.
These commands build only; upload the chosen UF2 separately.

`make zeptocore` explicitly configures the option OFF, including after an ON build.
The Make interface accepts `ON` or `OFF`. For an isolated CMake build:

```sh
cmake -S . -B build/visualizer \
  -DCORE_COMPILE_DEFINITIONS="$PWD/zeptocore_compile_definitions.cmake" \
  -DZEPTOCORE_VISUALIZER=ON
cmake --build build/visualizer --parallel
```

Direct CMake invocations retain cached options: pass `-DZEPTOCORE_VISUALIZER=OFF`
to disable it in an existing directory. The option requires Zeptocore with MIDI;
unsupported devices or builds without MIDI fail configuration. When OFF, the
telemetry source, hooks, state buffers, and subscription service are excluded,
and ordinary MIDI clock/start/stop retain their existing path.

The browser app remains a separate, explicit build. Firmware builds need no
Node/npm and do not embed the app. Full sample/slice telemetry requires an enabled
firmware; default firmware only provides the existing legacy status.

## Build and run

Use Node.js 22.12 or newer and Chrome or Edge. Build once:

```sh
cd visualizer
npm ci
npm run build
```

Run the built server, pointing it at the folder containing your `bank1`, `bank2`,
etc. directories:

```sh
node dist/server.mjs /path/to/reference
# Optional port (default 4173):
node dist/server.mjs "/path/with spaces/reference" --port 8080
# Equivalent npm shortcut:
npm start -- /path/to/reference --port 8080
```

Relative paths resolve from the shell's current directory. With no folder
argument, the server uses `reference` beside the `dist` directory. `--help` shows
usage. Missing reference folders and invalid arguments fail with a clear error.

The server caches prepared waveform and spectrum data between runs in
`$XDG_CACHE_HOME/zeptocore-visualizer` (default `~/.cache/zeptocore-visualizer`).
At startup it checks WAV and `.info` file sizes and timestamps, reuses unchanged
samples, and prepares only new or changed samples. Progress lines show the
completed/total count and the sample being prepared, reused or failed. It reports the reused and
prepared counts before printing its localhost URL. The first run of a large
library takes longer. Missing, damaged or outdated caches regenerate automatically;
delete the cache directory to force a full refresh. Reference folders can be read-only.
It serves on `127.0.0.1` only. Stop with Ctrl+C. Restart after editing reference
files or to select another folder; no app rebuild is required. A bad sample is
reported in the terminal and UI while other samples remain available.

`dist` contains the compiled website and standalone `server.mjs`. You can copy
that folder to another machine with Node and launch it against that machine's
reference folder; npm, Vite, and node_modules are not needed to run it. Original
WAVs remain on disk and are not served over HTTP. The server only serves the UI
and derived waveform/spectrum JSON.

For a build-and-run shortcut:

```sh
make serve REFERENCE=/path/to/reference PORT=4173
```

`make serve` installs locked dependencies when needed, builds, and starts the
server. `npm run preview -- /path/to/reference` also runs the built server.
For development with automatic refresh, use `npm run dev`; it watches the local
`visualizer/reference` folder. Run `npm test` for checks.

Open the printed localhost URL, click **CONNECT**, and allow MIDI access including
system exclusive messages. If several ports are available, select the zeptocore's
input and output. After permission has been granted, the app connects automatically
on supported browsers and retries USB reconnections. Waveform data is preloaded
with three concurrent requests and cached by content URL, so switching to a
cached sample is immediate.

## Reference files

Keep the unmodified SD-card copy under `reference`, for example:

```text
reference/bank1/0.0.wav
reference/bank1/0.0.wav.info
reference/bank1/0.1.wav
reference/bank1/0.1.wav.info
```

Banks are numbered 1–16 on disk, samples 0–15. The interface displays both as
1-based numbers. Only original `.0.wav` files and their `.info` metadata are
consumed. `.1.wav` is the time-stretched companion, not another sample. Settings
and savefiles are ignored. No reference file is written, renamed, or deleted.

The loader mirrors `core/src/zeptocore/zeptocorebinary.go`: an 11-byte
little-endian header followed by start, stop, and type arrays. Boundaries are
PCM byte offsets relative to the unpadded audio. It supports mono/stereo 16-bit
PCM at 44.1/88.2 kHz and removes 0.5 seconds of padding at either end. Each
channel retains its own minimum/maximum envelope, including transients and
opposite stereo polarities. Missing or inconsistent metadata produces an
unavailable state rather than guessed slices.

## Firmware protocol

This repository adds opt-in reporting to MIDI-enabled zeptocore builds. Older
firmware still supplies bank/sample status but the page explicitly displays
**SLICE TRACKING UNAVAILABLE**.

The page sends `[0x89, 5, 0]` (channel 10, Note Off, note 5, velocity 0) every
500 ms. This requests a snapshot and renews a two-second lease. Existing
`[0x89, 4, 0]` status queries are used until new telemetry arrives.

Snapshots are ASCII between `F0` and `F7`:

```text
view=2,bank,sample,slice,trigger,bpm,forward,stopped,muted,valid,effects
```

`2` is the protocol version; the webpage also accepts version 1 without effect
state. `effects` is a 16-bit mask in firmware effect order. The background grid
uses the bundled Font Awesome solid font: inactive icons have 9% opacity,
active icons have full opacity. Fresh version-2 firmware telemetry is required
for accurate effect highlights; old firmware leaves icons dim. The additional
Grimoire slow-down, speed-up, and retrigger icons remain dim because these are
not active effect slots in this zeptocore firmware.

Bank, sample, and slice are zero-based. Flags are
0/1. `trigger` is a wrapping unsigned 16-bit serial; every slice trigger increments
it, including a repeated trigger of the same slice. Snapshot identity and serial
are published together across cores. Invalid media or a sample transition clears
`valid`; an invalid snapshot's slice is not authoritative.

Changed states are transmitted at most 60 times/second; idle states have a 250 ms
heartbeat. Serialization occurs in the foreground, using bounded USB event
packets, not in audio/timer callbacks. Zeptocore's outgoing clock and transport
also use a bounded foreground queue: TinyUSB's transmit mutex cannot safely be
entered from the timer IRQ while foreground telemetry is writing. Partial transmissions resume under USB
backpressure; a 50 ms stalled-host cutoff prevents indefinite control starvation.
Unfinished frames must never be interpreted as complete snapshots.

When an invalid snapshot first identifies a different bank/sample, the browser
provisionally starts at slice 0 using its reported tempo and direction. This
estimate lasts at most 1.5 seconds from the transition; loading heartbeats do not
extend it. The first valid snapshot replaces the estimate and reanchors the cursor,
even if the trigger number is unchanged. Initial invalid snapshots and invalidity
within an already playing sample do not start speculative playback. The display
cannot anticipate a sample choice before the device reports its identity.

The browser expires telemetry after 1.5 seconds. The cursor anchors at the
reported slice edge, follows direction, and scales source-time progression by
current/source BPM when tempo matching is enabled. Heartbeats preserve the
anchor; retriggers replace it. Normal playback continues across slice boundaries
and wraps at the file end, including while waiting for a variable slice trigger.
Slice-stop, slice-loop, sample-stop, and sample-loop modes use their respective
boundaries. The cursor stops when transport stops or the connection becomes stale. The display combines stereo into one mirrored peak envelope with at most 640 columns and continuous amplitude.
The active slice is brighter; stopped or muted playback leaves the waveform dim.
Animated numbered callouts respond only to performance-pad Note On messages
(notes 0–15 on MIDI channels 1–3), not automatic slice changes. The number is
the physical pad (01–16); its pointer uses the first playback snapshot after
the press. Modifier buttons A–D do not emit these notes.
Pitch modulation, effects, and audio output latency are not modelled: the cursor
is deliberately labelled **ESTIMATED**.

Firmware and MIDI tests (from the repository root):

```sh
CC=/opt/homebrew/opt/llvm/bin/clang .venv/bin/python test/midi/run.py
```

The compiler override avoids the Apple AddressSanitizer startup issue on this
machine. Other platforms can omit it. Both normal 441-frame and 256-frame builds
support the protocol; other device targets compile the feature out.

Verify the firmware flag (441/256 frames, default → ON → OFF, symbol/map checks,
and rejection of unsupported devices or missing MIDI):

```sh
python3 test/visualizer_build/run.py
```

This builds under `artifacts/visualizer-build`, saves logs and size reports, and
never flashes hardware. CI runs these checks without publishing opt-in firmware
as a default release artifact.

## Source spectrum

The lower panel shows 32 logarithmic frequency bands from 50 Hz to 16 kHz.
Hann-windowed 4096-point FFTs are generated at 20 frames per second from the
unpadded reference WAV. Stereo powers are combined without phase cancellation.
Levels use a fixed −72 to 0 dBFS display range and compact 8-bit storage.

The bars interpolate at the same estimated source position as the playback line,
including sample changes, retriggers, reverse, and tempo changes. They clear when
playback stops, mutes, or telemetry expires. Jumps reset animation smoothing;
reduced-motion preferences disable smoothing. This is a source spectrum, not a
measurement of device output: pitch processing and device effects are not included.
No microphone access, audio playback, or additional firmware telemetry is needed.
