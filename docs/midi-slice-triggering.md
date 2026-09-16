# MIDI slice triggering

Zeptocore accepts USB MIDI notes on **channel 1** as monophonic slice triggers.
Each Note On with velocity 1–127 jumps immediately to the current sample's
zero-based slice index `note_number % slice_count`. For a 16-slice sample, note
60 selects index 12 (the 13th slice); note 64 wraps to index 0. The mapping uses
raw MIDI note numbers, independent of a DAW's octave naming convention.

Repeated notes retrigger the slice. Later notes replace the previous selection.
Velocity does not change loudness or pitch. Note Off, including Note On with
velocity zero, does nothing: note duration does not gate playback. Existing
playback modes and effects still determine how the audio proceeds after a jump.
The normal jump path resets the gate and records `slice % 16` when the internal
jump sequencer is recording, just as the existing 16-step jump controls do.
This does not add absolute-slice storage to that sequencer.

Notes do not start transport or clear manual mute. Notes received while stopped
or with a pending Stop are ignored. While manually muted, a note can change the
slice without making it audible. Notes are also ignored when no valid sample
is open, media is unavailable/being changed, or slice metadata is missing.
The physical button mode and jump-page selection do not change the MIDI mapping.
Only the first 128 slices can be addressed directly by MIDI's 128 note numbers.

## Ableton Live setup

In Settings → Tempo & MIDI (Link, Tempo & MIDI in some versions), enable
**Track** for the zeptocore output to send notes and CCs. Route a MIDI track's
output to **zeptocore, Ch. 1**. Enable **Sync** on the same output to send clock
and transport, and select **Pattern** as the MIDI Clock Type. Leave input Sync
off when Live is the clock source. Note Off does not stop the device; use Live's
transport Stop.

See [Live's MIDI settings](https://help.ableton.com/hc/en-us/articles/209774205-Live-s-MIDI-Settings)
and [MIDI synchronization](https://help.ableton.com/hc/en-us/articles/209071149-Synchronizing-Live-via-MIDI).

## Firmware and tests

`MIDI_NOTE_KEY` defaults to 1 when `INCLUDE_ZEPTOCORE` is defined, and to 0 for
other devices. An explicit compiler definition `MIDI_NOTE_KEY=0` disables slice
notes without disabling transport or CCs. The callbacks are in
[`midi_note_key.h`](../lib/midi_note_key.h), included by
[`midicallback.h`](../lib/midicallback.h). No held-note state is maintained.

[`midi_comm.h`](../lib/midi_comm.h) consumes one complete USB MIDI event packet
per foreground iteration, preserving message boundaries when clock and notes
arrive together. Channel 1 velocity-zero Note On is dispatched as Note Off.
Other channels retain their existing behavior, including the channel 10 Note
Off command protocol. SysEx input is not interpreted as channel commands.

Run `.venv/bin/python test/midi/run.py` for sanitizer-backed native tests of the
production note callbacks and USB dispatcher, with enabled, explicitly disabled,
and other-device defaults. Build with `make zeptocore` or `make zeptocore_256`.

On macOS Tahoe, if Apple's AddressSanitizer hangs before test startup, use the
installed Homebrew LLVM runtime instead:

```sh
CC=/opt/homebrew/opt/llvm/bin/clang .venv/bin/python test/midi/run.py
```

The 2026-09-16 validation passed all three configurations with this command and
built both 441-frame and 256-frame firmware. The local TinyUSB dependency used
the same `OSAL_TIMEOUT_WAIT_FOREVER` → `OSAL_TIMEOUT_NORMAL` patch as
[release CI](../.github/workflows/build.yml). The standard 441-frame image was
uploaded to the attached RP2040 and flash verification passed. Build logs and
the device's previous flash image are retained under ignored `artifacts/midi/`.
In Live, output Track and Sync were enabled with Pattern clock and channel 1
routing; input Sync was disabled so Live remained the clock source. The user
confirmed audible slice jumps with playback continuing after note release.
The device's existing channel 10 status query confirmed that notes after Stop
left both stopped/muted flags set, and notes during manual mute preserved mute
without stopping transport. The validation ended with Live and Zeptocore stopped.
