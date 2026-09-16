# Clock restart correction and immediate renderer wake

## Behavior

A clock start after stopped/muted transport now publishes a restart request
with its target source phase. The audio core consumes that request only when
applying that phase, starts the new source head without the old-head crossfade,
and applies a 32-frame linear fade from silence (about 0.717 ms at the measured
sample rate). The new read has zero latency-compensation advance for that block.
The first-clock exception is consumed by head 0, never the old crossfade head.
A clock restart after a manual stop also clears the stop's latched button mute.

For the 256-frame ectocore path, clock start sends a nonblocking FIFO wake to
the audio core. The normal DMA notification remains the fallback if the FIFO
is full. Rendering may begin while zero PCM is queued, but the old silence
stays available until the replacement block is complete. Publication atomically
replaces only leading prepared buffers explicitly marked as all-zero PCM.
Consumer-held buffers, DMA-owned buffers and audible effect tails are preserved.

The wake path runs only with exactly one queued silent block. If the consumer
takes that block while the wake render is running, the completed render earns
one credit against the pending DMA notification. Otherwise publication replaces
the silent block one for one. Ordinary DMA rendering keeps its existing queue
policy and recovery headroom.
No output buffer or steady response delay was added. The source correction is
enabled by `AUDIO_CLOCK_RESTART_FIX`; the 256-frame wake path is enabled by
`AUDIO_CLOCK_RESTART_WAKE`. Both default ON. Ezeptocore shares this ectocore
code path, but only ectocore hardware was tested here.

## Clock latency results

Both configurations used 200 MHz, 256 frames, bank 0/sample 0, forward playback,
no stretch and saturation enabled. Each final measurement contains 24 simulated
CV restarts after clock loss.

| Clock edge to resumed block DMA output | Before (ms) | After (ms) |
| --- | ---: | ---: |
| Minimum | 5.866 | 2.673 |
| Median | 8.909 | 5.239 |
| Maximum | 11.393 | 8.005 |

The median fell by about 41%. This compares resumed block boundaries: the old
block crossfaded the previous head and incorrectly advanced the new source by
about 16 ms of equivalent playback time. Correcting those source errors also
preserves the intended beat onset.

Applying the new phase fell from a 3.194 ms median after the edge to 0.144 ms.
The full block was submitted at a 2.506 ms median. All 24 new traces had zero
source advance, no old-head crossfade and zero starvation. The first nonzero
PCM was frame 1, about 22.4 us after the traced block boundary. Manual-stop
restart also succeeded: 5.824 ms to block DMA, compared with no audio output
in the preceding manual-stop test.

These software-triggered measurements exclude electrical CV input, NVIC entry,
remaining PIO serialization and DAC delay. They include test instrumentation.
Different clock arrival phases change the wait for the next output boundary;
the before/after trials are not paired at identical DMA phases. Effects or SD
stalls can increase render time. Queued audible tails are intentionally retained
and may prevent an immediate replacement.

## Validation and evidence

Native ASan/UBSan tests cover phase request consumption, replacement by a newer
request, positive/negative fade samples, exact-zero detection, queue ordering,
tail pointers, lock ordering and consumer ownership. In particular, checking
readiness leaves fallback silence queued, and publication cannot reclaim a
buffer already taken by the consumer. Existing timing, conversion and 22 host
protocol tests also pass.

Ectocore 256-frame normal/test builds, ectocore 441-frame, ezeptocore 256-frame
and zeptocore 256-frame builds pass. No non-ectocore hardware was connected.

Latency evidence: `artifacts/seek/ectocore-clock-restart-v2/`, including
`summary.json`, `clock-loss.jsonl`, `manual/`, native/build logs and audio.
Latency test ELF SHA-256:
`f99ff89208f4f51df83142e39c4354590126c715de7c5d2aa0849bb209b7a928`.

An earlier candidate globally capped the prepared queue at one block. It was
discarded after 24 starvation events in a 90-second stress test, mostly during
stretch/reverse. The final wake credit above replaces that global cap. The
discarded candidate remains under `artifacts/seek/ectocore-clock-restart/` for
comparison and is not the release image.

## Validation before production cleanup

The revised queue policy completed a 90-second run with 240 observed file
changes, rapid switching, up to five active effects, both directions and
stretch at Q8=1251. The coherent capture recorded one DMA starvation event
(576 inserted silent frames, about 12.9 ms), during combined switching/effects;
stretch/reverse had none. The preceding optimized firmware recorded 1 and 0
starvation events in its two equivalent 90-second runs. This run supports
preserved stability, not a guarantee of zero dropouts. Maximum callback time
was 7.094 ms against a 5.734 ms block budget. No seek maps were built or written.

Scarlett 48 kHz stereo captures were retained for both latency and stress runs;
neither clipped. Silence detection alone cannot distinguish intentional source
silence from a dropout, so the starvation result comes from DMA diagnostics.
Both stack guards remained intact: observed stress use was at most 2,516 bytes
on core 0 and 1,904 bytes on core 1, each within its 4 KiB available stack.

The normal firmware tested at this stage used 200 MHz / 256 frames, diagnostics ON, test controls
and clock tracing OFF, and no extra output buffer or next-file preparation.
Both restart options are ON. Image and programming log:
`artifacts/seek/ectocore-clock-restart-v2/normal/`.
Normal ELF SHA-256:
`1a950e7383a88535e32d7051e5b1bb348ce437d1c79ed20788c56daffa0b0c05`.
Stress ELF SHA-256:
`1ea14ce0f16b4d8424f0f996eb2d00364e3398eaba76eb36601f1a0431df9c3c`.

After verified programming, the normal image reached playback stage 3 with
all 224 maps reused, no map rebuild and zero early starvation. Startup heap
headroom after controls initialization was 1,920 bytes, with one reverb comb
and one allpass. The current installed image is identified in the production cleanup section below.

## Production cleanup

The restart implementation now uses one shared enablement definition in
`audio_restart.h`. The queue wake path additionally requires the 256-frame
connection and core-1 renderer, so disabled or unsupported configurations do
not scan silent PCM or send unused wake messages. Wake publication state is
grouped together and owned only by the audio worker. Comments document phase
publication order, the pending DMA credit, and why queue helpers stay in flash.
The queue test explicitly checks that replacing leading silence preserves both
an audible buffer and any silence behind it.

Normal firmware excludes the clock trace records and simulated-clock generator;
both remain available only in explicitly enabled test builds. Cleanup build,
native-test and hardware evidence is under
`artifacts/seek/ectocore-clock-restart-clean/`.

The cleaned build repeated 24 simulated clock restarts with zero starvation,
zero source advance and the short restart fade on every trial. Clock-to-block
DMA latency was 2.760–9.013 ms, with a 5.085 ms median; phase application had a
0.145 ms median. These retain the software/DMA measurement limits above.
Native tests and ectocore 256/441, ezeptocore 256 and zeptocore 256 builds pass.

The cleaned build's 90-second stress run recorded 239 file changes and one
starvation event (576 silent frames), with no map builds or index writes.
Both stack guards were intact: observed use was at most 2,476 bytes on core 0
and 1,904 bytes on core 1. The result remains comparable to the preceding
0–1 starvation events per 90-second run; heavy effects can still overrun.

Installed normal ELF:
`artifacts/seek/ectocore-clock-restart-clean/normal/firmware.elf`, SHA-256
`6596d89e4bc40a162a78cde194886c1bc675f8971b78706e6500989579a22bf0`.
The image was programmed and verified with both restart options enabled,
200 MHz / 256 frames, diagnostics ON, and test controls/clock tracing OFF.
There is no added output buffer or continuous latency penalty.

## Pre-commit regression pass

All four native suites passed again: restart/diagnostics, bounded resampling,
DSP reference equivalence, and FatFs seek maps/recovery/ownership. Coverage
includes 2,400 resampling comparisons, 4,000 delay and 12,000 filter blocks at
each of two compiler optimization levels, 90 injected index-write failures,
and 20,200 acknowledged media ownership handoffs.

A further 24 clock-loss restarts passed with zero starvation and no source
advance. DMA latency was 3.265–8.083 ms (median 5.796 ms). A manual-stop restart
also passed at 3.590 ms. Arrival phase and rendering variation affect these
unpaired trial medians. Raw evidence: `artifacts/seek/ectocore-regression/`.

The 90-second regression stress run completed 241 observed file changes with
one starvation event (576 silent frames), matching the preceding runs. No
seek maps were built or written. The normal firmware was restored afterward.
