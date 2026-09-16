# zeptocore / waveform

A local, monochrome waveform monitor. The device supplies the selected bank,
sample, and active slice; the browser estimates the playhead between triggers.
The page does not play audio or send performance controls.

IBM Plex Mono (regular, medium, and bold) is vendored as WOFF2 files in
`src/assets`, alongside its SIL Open Font License. UI text and canvas callout
labels use the local font; no font service or CDN is needed.

## Run

Use Node.js 22.12 or newer and Chrome or Edge:

```sh
cd visualizer
make serve
```

This installs the locked npm dependencies when needed, builds the website and
waveform data from the current `reference` files, and serves `dist` locally.
Stop the server with Ctrl-C. Run `make serve` again after changing reference files.
For development with automatic refresh, use `npm run dev` after the first setup.

Open the localhost URL printed by Vite, click **CONNECT**, and allow
MIDI access including system exclusive messages. The Codex embedded preview may
deny MIDI permission; open the same URL in Chrome for hardware use. If several
ports are available, select the zeptocore's input and output.

After MIDI/SysEx permission has been granted, the page automatically connects
on load in browsers supporting MIDI permission queries. It reconnects after
USB disconnection and retries connection errors every two seconds. Returning
to the tab also checks the connection. The first permission approval still
requires **CONNECT**; unsupported permission queries use manual connection.

```sh
npm test
npm run build
npm run preview
```

`dist` is self-contained: it contains the page and derived waveform data, not the
original recordings. Rebuild after replacing the reference library for a
production preview. Development watches reference changes and refreshes the
library automatically. Waveform data is preloaded with three concurrent requests
and cached by its content URL, so switching to a cached sample is immediate.

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
