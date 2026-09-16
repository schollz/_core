# Follow-up: retrigger-pitch stack overflow

The user reported silent, frozen hardware after the initial seek-map acceptance
checks. The existing debug service detected both cores halted. Before resetting,
the investigation preserved all 264 KiB of SRAM, registers, OpenOCD history and
the exact ELF in `artifacts/seek/freeze-incident-1`.

## Cause

Core 0 faulted while calling `ClockInput_update_raw`'s falling-edge callback.
Its stored callback had been overwritten with PCM-looking data; the recorded
fault PC was `0xeb22f6f2`, with caller LR `0x10001489`. Core 1's bottom stack guard
and adjacent heap objects were overwritten with audio data. Core 0 retained
1588 untouched stack bytes.

The dump records ordinary base pitch (index 48), retrigger pitch index 72 (2x),
and a 441-frame stereo source. Normal playback allocated both an interleaved
source array and a deinterleaved channel array on core 1's 4096-byte stack.
At 882 source frames plus interpolation lookahead, those arrays alone required
5298 bytes. The earlier playback matrix did not cover this retrigger-pitch state;
its passing results did not establish safety at higher source rates.

## Correction

Normal playback now shares the existing fixed stretch-source workspace; those
render paths are mutually exclusive. Stereo interpolation reads interleaved
samples directly with a stride, eliminating the second pitch-sized stack array.
The 9-bit interpolation arithmetic and output samples are preserved. No new
per-callback allocation or additional source-buffer RAM is introduced.

Reads reserve the interpolation lookahead within the fixed workspace. Extreme
combined rates are capped at its capacity: approximately 4x for stereo and 8x
for mono. This applies to the combined pitch/tempo/retrigger/oversampling rate.
It prevents those combinations from corrupting memory. The previous stack-based
implementation already overflowed at 2x stereo on the 441-frame build.

The maximum MIDI pitch value also previously computed index 73 for a 73-element
array. It now clamps to the last valid index, 72.

## Verification

`test/audio_source/run.py` runs the actual interpolation helper under ASan/UBSan:
2400 mono/stereo comparisons at 256/441 frames match the previous deinterleave
and interpolation output exactly, preserve input, and check bounded lookahead.
The FatFs/ownership tests and all 22 diagnostic protocol tests also pass.

`freeze-fix-441` retains the patched normal firmware and ordinary/stretch/sample
switch captures: no starvation, 1384 free heap bytes after controls, unchanged
four reverb combs/three allpasses, and intact stack guards. Core 1 retained 1504
untouched bytes across those workloads. `freeze-pitch-441` swept all the way to
MIDI pitch 127 (confirmed index 72) at 170 BPM: zero starvation, valid PCM on both
Scarlett channels, no clipping, and 2888 untouched core-1 stack bytes. Its
initial attempted tempo control used an unused CC; that artifact is pitch-only.
The corrected follow-up uses CC15 and checks the observed 254 BPM explicitly.

The 256-frame build passed all six playback modes. Both supported builds passed
25-second combined sweeps through pitch index 72 at 254 BPM, with zero starvation,
zero map builds/index writes and intact stack guards (`freeze-rate-256` and
`freeze-rate-441-retry`). The audio core retained 3264/2888 untouched stack bytes
respectively in those sweeps. The 256-frame build has 8424 free heap bytes after
controls; reverb resources remain unchanged.

An initial 256-frame stress capture began before startup completed and was
rejected for a changing device stage. An initial 441-frame combined test missed
USB MIDI re-enumeration and sent no controls; assertions rejected it. The harness
now waits for active playback and MIDI availability and verifies the observed
pitch, tempo and all eight control events. Rejected artifacts remain available.

The patched 441-frame firmware is programmed and the on-demand service uses
`artifacts/seek/freeze-fix-441/firmware.elf` (SHA-256
`5e1a2b35242f091d061cfe1b7138f5e4c84f1495b1ad241c3bc476ecc49fc54d`).
The matching UF2 and linker map are alongside it. These are follow-up source
buffer checks; the earlier seek-map timing matrix remains associated with its
original retained ELFs.
