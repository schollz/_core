# Zeptocore seek-map hardware results

The currently programmed firmware includes a subsequent
[retrigger-pitch stack-overflow fix](seek-freeze-fix.md). The original acceptance
matrix below did not exercise the failing retrigger-pitch state. Its measurements
remain tied to the retained ELFs; the follow-up report records the corrected
firmware, source-rate limits and additional stress captures.

The shared implementation, native checks, supported playback matrix, observer
gate, fragmented benchmark and physical reinsertion check passed. GOAL.md is
complete for the supported configurations. Measurements below identify their
exact retained builds. No SD clock or checksum setting changed.

## Final playback comparison

`artifacts/seek/maps-final-builds/supported-manifest.json` identifies eight matched
ELFs and linker maps: 441/256 producer frames at 225/125 MHz, with attachment
on/off. The user removed 128 frames from the supported build/validation matrix;
earlier 128-frame measurements remain historical. Both members
prepare and load the same cache; only attachment differs. Six workloads exercise
forward playback, stretching, repeated selection, reverse, scheduled slices and
file variation. Each capture includes simultaneous 4 Hz diagnostics and stereo
Scarlett audio; control schedules and exact file layouts are retained.

All 48 five-second observations were coherent, with zero DMA starvation, I/O
errors, short reads, runtime map builds or index writes. Every boot reused all
192 records. Results are in `maps-final-matrix-*` and
`maps-final-matrix-summary.json`. These supersede the earlier `maps-matrix-*`
measurements because the mapped seek body now runs from SRAM, reuses a known
cluster for local seeks, and avoids general 64-bit division.

| Frames / MHz | Heap free after controls, bytes | Core 1 untouched stack, bytes | Starvation, mapped / ordinary |
| --- | ---: | ---: | ---: |
| 441 / 225 | 1176 | 972 | 0 / 0 |
| 441 / 125 | 1176 | 972 | 0 / 0 |
| 256 / 225 | 8216 | 2064 | 0 / 0 |
| 256 / 125 | 8216 | 2064 | 0 / 0 |

Both members retain four reverb combs/three allpasses. Core 0 retains at least
1548 untouched stack bytes; all guards remain intact.

Mean seek time in microseconds, mapped / ordinary:

| Frames / MHz | Forward | Stretch | Reverse | Scheduled slices |
| --- | ---: | ---: | ---: | ---: |
| 441 / 225 | 21.108 / 22.938 | 262.576 / 264.196 | 345.016 / 348.028 | 22.303 / 24.212 |
| 441 / 125 | 27.824 / 31.802 | 297.262 / 302.345 | 378.154 / 384.936 | 29.310 / 33.420 |
| 256 / 225 | 12.027 / 13.022 | 290.651 / 295.042 | 346.125 / 353.187 | 12.144 / 13.412 |
| 256 / 125 | 17.379 / 19.363 | 323.539 / 331.587 | 378.069 / 389.240 | 17.499 / 19.817 |

Five request-to-DMA switch observations per build, milliseconds:

| Frames / MHz | Median, mapped / ordinary | Maximum, mapped / ordinary |
| --- | ---: | ---: |
| 441 / 225 | 34.886 / 37.763 | 35.933 / 38.359 |
| 441 / 125 | 36.560 / 35.250 | 38.781 / 39.423 |
| 256 / 225 | 14.310 / 13.563 | 15.576 / 14.535 |
| 256 / 125 | 20.374 / 19.959 | 22.395 / 22.790 |

Host MIDI is not aligned to the audio block. These small, mixed differences
are within a producer period and do not establish a precise switching speedup
or regression. No completions were missed, overlapped or expired in these runs.

The card's real playback files are contiguous, with 64 KiB clusters. The larger
8,145,596-byte variation was also exercised in `maps-long-441-225-on/off` before
the SRAM helper change. A fragmented-file benchmark below separately measures
allocation-chain traversal; it does not play synthetic samples through DSP.

Actual I2S rates are 44501.582 Hz at 225 MHz and 44389.204 Hz at 125 MHz. DMA
consumes 256-frame buffers for every producer size. Starvation counts measure
actual missing consumer buffers, separately from callback deadline overruns and
intentional mute. Request-to-DMA tracing includes the first selected-source frame
offset and excludes codec/analog delay. Histograms retain median/p99 ranges;
`lifetime_max_us` includes earlier activity and is not a window maximum.

Earlier complete comparisons found repeated-switch overload at 128 frames with
both seek methods. The initial mapped 128/125 reverse regression motivated the
helper changes. A targeted final-helper repeat (`maps-ramfunc-128-125-on/off`)
measured reverse mean 277.721/296.252 us and starvation 0/3; repeated switches
had 14/14, and ordinary forward playback had zero in both. These 128-frame
trials are historical and outside the supported matrix. No blanket zero-underrun claim covers every
pitch/DSP combination.

## Startup, persistence and file changes

`artifacts/seek/maps-final-startup-summary.json` retains the raw values behind
this table. Times are milliseconds; preparation is the complete map pass,
including enumeration and bookkeeping beyond its separately timed operations.

| Card/index state | Files | Build / reuse | Index writes | Prepare | Validate | Build | Index load | Commit |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | ---: | ---: |
| First use, empty test index | 192 | 192 / 0 | 195 | 1635.567 | 107.127 | 36.636 | unavailable* | 768.790 |
| Same test index, next boot | 192 | 0 / 192 | 0 | 818.747 | 163.125 | 0 | unavailable* | 0 |
| Add normal audio-variant copy | 193 | 1 / 192 | 196 | 2102.163 | 156.554 | 0.024 | 491.831 | 835.780 |
| Add stretched audio-variant copy | 194 | 1 / 193 | 197 | 2167.985 | 163.368 | 0.501 | 512.290 | 863.107 |
| Both copies unchanged | 194 | 0 / 194 | 0 | 836.915 | 170.173 | 0 | 139.700 | 0 |
| Remove stretched copy | 193 | 0 / 193 | 196 | 2118.243 | 149.294 | 0 | 506.041 | 843.667 |
| Remove normal copy | 192 | 0 / 192 | 195 | 2183.971 | 171.696 | 0 | 523.759 | 866.302 |
| Restored original card, final 441/225 boot | 192 | 0 / 192 | 0 | 800.854 | 151.019 | 0 | 132.324 | 0 |

*The older first-use/reuse ELF counted runtime cache loading only. Its zero
`load_us` does not mean no startup index reads; newer builds include header and
record I/O. Whole `sdcard_startup` took 2.385160 s first use, 1.576951 s on that
unchanged boot, and 1.572266 s on the final restored-card boot.

First use is `maps-automatic-firstuse-441`; reuse is `maps-testindex-reuse-441`.
They exercise automatic firmware work in `.core_seek_test`. The production
index remains `.core_seek`. Changed-file artifacts are
`maps-audio-fixture-{added,pair,reused,remove3,remove2}`. The changed generation
copies unchanged records into the other slot, so a one-file change still writes
an entire committed manifest. Unchanged boots write nothing.

Fixture work is excluded from map preparation: copying the normal/stretched WAVs
took 5.694/39.868 s, and verifying the stretched copy on reuse took 8.523 s.
Those test-only operations inflate whole startup to 8.567/42.813/10.140 s.
Production firmware does not copy or hash audio contents.

The fixture created `bank1/0.2.wav` (1,172,588 bytes, cluster 13512) and
`bank1/0.3.wav` (8,145,596 bytes, cluster 13533). Both variants reopened with the
correct distinct cached maps and zero starvation. The normal copy has the same
size as the original at cluster 8405, directly exercising equal-size identity.
Each addition built exactly one new map. Guarded cleanup verified each copy's
card identity, start cluster, length and SHA-256 before removal. The card is back
to 192 WAVs; production index generation is 5. The incomplete single-copy trial
did not enable logical variant 1 and is not counted as reopen coverage.

## Reopen and saved-settings coverage

The matched matrix covers sample/bank switches, repeated selection, normal/
stretched playback and both file variations. `maps-audio-fixture-reused` covers
both physical files of audio variant 1. All opens use the shared helper.

`maps-cache-load-441` exercised three deferred persisted cache loads, including
eviction and reloading a prior selection. Each made zero builds/index writes;
playback resumed mapped with no starvation. Explicit MIDI stop provided the
already-quiet window; the map code did not create a pause to service a miss.

`maps-settings-lifecycle-441-retry` saved selection [0,0,0,0] to previously empty
preset 15, switched to [2,15,0,0], and loaded the saved selection. The correct map
attached immediately, with zero builds, index writes, ownership timeouts or
starvation during the Scarlett recording. Preset 15 remains available; the
current slot selector remains 0. Existing presets were not overwritten. The
first attempt started before the boot guard was released and saved nothing;
the corrected harness waits for active playback and verifies the save occurred.

Native tests additionally cover rename/replacement/deletion, same-metadata
allocation relocation, changed cards/reformatting, read-only/full media,
corruption and interrupted persistence. Those are controlled filesystem-image
tests; no fault injection was performed on the user's audio files.

## Fragmented-file benchmark

`maps-fragment-final-256` contains the final 256-frame/225 MHz boot benchmark.
It reused a reserved 2,031,616-byte file with 31 fragments and a 64-DWORD map,
checking 768 matched
reverse/jump/grain-like positions, alternating seek order and verifying every
returned byte and file position.

Ordinary/mapped seek mean was 2323.828/221.258 us (**10.50x improvement**), with
maxima 6651/503 us. Ordinary seeks visited 7531 allocation links; mapped seeks
visited zero. All 786432 returned bytes and resulting positions matched.
Read-512-byte means were 258.837/282.624 us, with maxima 660/524 us. Seek p50
ranges were 513–1024/129–256 us and p99 ranges 5806–8192/257–512 us. The
test-only allocation counter adds a small ordinary-seek cost. Map building took
6692 us. This measures filesystem operations; it does not
measure synthetic-file DSP or analog playback.

The 1.9375 MiB fixture remains in `.core_seek_bench/fragment.bin` for repeatable
checks; its temporary spacing file was removed. It and the retained test index
are excluded from the audio manifest. The benchmark adds 520 test-only static
bytes and borrows the normal preparation workspace. Final benchmarking uses
256 frames to provide more heap headroom than the 441-frame test build.

## Diagnostics and memory

The original prerequisite passed before map implementation:
`artifacts/seek/diagnostics-gate.json`. Final paired builds with an independent
callback/starvation witness are in `maps-final-witness-builds`; completed
observations are in `maps-verified-observer-*` and `maps-final-diagnostics-gate.json`.

An earlier complete-hook gate, before the mapped helper optimization, measured
added callback means below 0.08% of the producer period with 4 Hz retrieval and
zero independent starvation in twelve trials (`maps-diagnostics-gate.json`).
The final gate passed two 20-second trials per build/configuration at 441/225
and 256/225: all eight had zero independent starvation and 3.985 Hz retrieval.
Net rendered-callback mean differences were -15.604 and -9.561 us respectively
(-0.157% and -0.166% of a producer period). Negative paired differences reflect
whole-build/timing variation; they do not mean instrumentation takes negative
time. No material slowdown was observed. Unpaired rendered lifetime maxima were
6003/6072 us compiled out versus 6000/5982 us instrumented at 441 frames, and
3720/3732 versus 3738/3745 us at 256 frames. These are not overhead bounds.

The audit covered 10302 retrieval commands: 6918 bounded SRAM reads and 648 writes
only to the exact ELF's request arguments/sequence. No unexpected commands,
halt/reset requests or out-of-range writes occurred during observation.

Fixed diagnostic data is 3468 bytes. Total diagnostic SRAM increments including
executable hooks are 6616 bytes in both final supported pairs: within the revised
7 KiB ceiling. The prerequisite passed its 6 KiB budget. Map arena/workspace/
statistics reserve 5012 ARM object bytes; the mapped helper adds 432–448 static
SRAM bytes including alignment/veneers. Allocator bookkeeping is additional.

Final 441-frame builds retain four reverb combs/three allpasses, 1176 bytes of
free heap after controls and 972 untouched core-1 stack bytes. The observed
core-0 margin in the production matrix is at least 1548 bytes; test-fixture
cleanup uses more stack and retained 832 bytes. Guards remain intact. These are
observed workloads, not proof for unexercised high-pitch/DSP paths.

Audio publication targets 10 us; foreground targets 50 us and remains preemptible.
The final full matrix observed audio 10 us, IRQ 4 us and foreground 39 us; the
225 MHz observer windows recorded 6/2/6 us respectively. Foreground elapsed
duration includes interrupt preemption.

Compiled-out observer firmware last calls `mallinfo` before map/control
allocations. Its cached free-heap value is historical; host reports label this
stage. Instrumented matrix heap values are measured after controls initialize.
All 48 WAVs have signal on both channels and zero clipped samples
(`maps-final-audio-levels.json`). WAV signal levels supplement DMA evidence; natural silence and analog timing
prevent treating level reports as proof of bit-identical output or no underruns.

## Final firmware and physical reinsertion

`maps-production-final-441` retains the programmed ELF, map and UF2: 441 frames,
225 MHz, maps and diagnostics enabled, test controls/fixtures/witness disabled.
ELF SHA-256 is
`01894a9e5578b8a4588ffe11a1076d5878314c96da501516f7e16a0b3d31820b`.
Its final ordinary/stretch/selection captures had zero starvation and reused all
192 maps with zero index writes. Heap after controls is 1176 bytes; reverb remains
four combs/three allpasses. A separate ten-second capture after the final stack
inspection reset also passed; that establishes the boot session immediately
before the requested physical action (`maps-physical-reinsert/before`).

The user then powered off, removed/reinserted the same card and powered on.
The probe was released during that action. `maps-physical-reinsert/after`
verified the same card geometry/CID and firmware, a new boot session, all 192
records reused, zero builds/index writes/failures and unchanged index generation 5.
Map preparation took 805152 us: validation 152378 us, index loading 132455 us,
building/commit zero. The subsequent ten-second 4 Hz observation and Scarlett
recording had zero DMA starvation and the correct mapped playback file.
Neither the before nor after capture halted or reset the device.
