# Ectocore hardware check

## Configuration

Tested the connected ectocore on 2026-09-15 with the standard
`ectocore_compile_definitions.cmake` configuration (hardware v4 pin definitions,
441 frames, 200 MHz), seek maps and diagnostics enabled, test controls disabled.
USB serial enumerated successfully; both RP2040 cores ran through SWD.

The initial build exposed an include-path conflict: the project's MIDI
`tusb_config.h` displaced the SDK's CDC configuration on USB-serial targets.
`CMakeLists.txt` now explicitly selects the SDK configuration when USB stdio is
enabled. The ectocore build and a zeptocore regression build both passed.

## Observed results

- First boot prepared 224 maps in 1.390 seconds, with no map failures.
- A 45-second playback capture contained 4,823 seeks, averaging 17.035 us.
  Reads averaged 469.295 us, with no errors or short reads.
- No DMA starvation occurred during the capture; no maps were built or written
  during playback. The selected file used a validated map.
- Stack guards remained intact. Core 0 retained 1,636 untouched bytes; the audio
  core retained 2,864 bytes. Free heap after controls initialized was 9,848 bytes.
- After the stack check reset the device, all 224 maps were reused, with zero
  builds or index writes. Preparation took 0.874 seconds.

## Limits

This was one unchanged playback state: bank/sample 0/0, forward, pitch index 48,
170 BPM, stretch factor 1, effects mask 1. Physical control changes, external CV,
clock inputs/outputs, higher playback rates and other buffer sizes were not
exercised. There was no paired old/new timing comparison on this device.

The initial Scarlett capture was essentially silent before the user routed the
output to the interface. After routing was confirmed, an eight-second capture
(`routed-audio.wav`) contained signal on both channels, with peaks of -40.82 and
-38.22 dBFS, no clipped samples and no near-zero run longer than 0.625 ms.
The signal level was low. This confirms captured output, but is not a calibrated
audio-quality or input-to-output latency measurement.

## Retained artifacts

Local evidence is in `artifacts/seek/ectocore-441/`, including the ELF, UF2,
linker map, build/flash logs, snapshots, capture, stack report and manifest.
The programmed ELF SHA-256 is
`08492e72f028b7038b0349fbb3643297ccf8e09570f34f7a77b5eb50c540fcd4`.

The original firmware is preserved in
`artifacts/seek/ectocore-original/flash-2MiB.bin`, SHA-256
`94c7663b21ae8313dc963f0532fc401e500404d8f47c5510c3f717a8c6f52727`.
The probe reports 32 MiB flash: this backup covers the first 2 MiB, including the
complete original firmware, rather than the entire flash chip. Programming was
verified and touched only the firmware range.

The new firmware remains programmed, with its matching on-demand debug service
running. The map index is stored on the card; original audio files were not
modified by the test.

## Follow-up: 128-frame hardware test

The standard `ectocore_compile_definitions_128.cmake` build also compiled,
programmed and verified successfully at 200 MHz, with diagnostics enabled and
test controls disabled. It differs from the 441-frame device configuration only
in the producer buffer length. Measured output rate was 44,642.857 Hz, giving a
2,867 us block budget.

One 60-second capture at four snapshots per second passed with zero DMA
starvation, no seek errors and signal on both Scarlett channels without clipped
samples. There were 21,323 seeks averaging 6.419 us. Callbacks averaged 671.631 us;
the lifetime maximum observed by the end of capture was 2,337 us. These timings
are workload-specific, not a matched performance comparison with 441 frames.

The capture observed only bank/sample 0/0, forward, pitch index 48, 170 BPM,
stretch factor 1 and effects mask 1. Rapid switching, other modes, external CV
and high-rate stress remain untested. This result supports ordinary playback at
128 frames on this ectocore; it does not establish a reason to remove that build
or prove its safety under every workload. It does not test ezeptocore hardware.

Stack guards were intact: core 0 retained 1,580 untouched bytes, and core 1
retained 3,472. Startup reused all 224 maps without builds or index writes.

Artifacts are in `artifacts/seek/ectocore-128/`. ELF SHA-256:
`884b682047aa3dd51dd1234dfceed2b21b4397c42fb6566bd35746480f9a0ede`.
The stack inspection reset the unit after capture. The 128-frame firmware now
remains installed, with its matching debug service running; the retained
441-frame image is available for restoration.

## Rapid-switching and effects stress: 128 frames

The follow-up automated test **failed the no-starvation criterion**. A coherent
90-second capture recorded 140 DMA starvation events (80,640 inserted silence
frames), with 240 file-generation changes. The maximum observed callback was
5,007 us against a 2,867 us producer-block budget. Both stack guards remained
intact, with 1,620/2,704 untouched bytes on cores 0/1.

The host sent four sample selections per second across eight samples, then
maximum break probability, cycling effect banks and amen patterns, then their
combination, then added stretch/reverse. Captured playback states confirmed all
eight samples, up to five active effect flags and stretch values of 256/1251
(Q8), with both playback directions observed. The normal overload protection
can disable effects/stretch, so these commands do not imply every requested
effect remained continuously enabled.

| Approximately 20-second phase | Starvation events |
|---|---:|
| Switching alone | 12 |
| Effects on one sample | 2 |
| Switching plus effects | 46 |
| Switching, effects, stretch/reverse | 76 |

Phase counts cover snapshots wholly within each phase; four additional events
occurred outside those interiors, for 140 total. There were no map builds or
index writes during capture. Many selected files used ordinary-seek fallback:
the two-entry map cache does not perform optional map loads during continuous
playback. This tests the deployed fallback behavior as well as cached maps.
Normal reads recorded two short reads, with zero FatFs error results; their
individual cause was not established by this capture. Analog signal was
recorded on both channels without clipping; effects and source content also
produce silence, so waveform gaps alone were not used to count dropouts.

The standard ectocore has USB serial, not USB MIDI. A `SEEK_TEST_CONTROLS`-guarded
three-byte serial transport in `lib/ectocore.h` invokes existing musical
controls, using ectocore's debounced knob path for sample selection. It accepts
no preset-save commands and is absent from normal builds. Reproduction uses
`scripts/ectocore_serial_stress.py` (pyserial required), concurrently with the
diagnostic capture and Scarlett recording. It does not alter the read-only SWD
protocol. Command logs must be checked against actual snapshot coverage.

An initial manual window stayed in one state. The first automated pass used the
zeptocore-only flattened MIDI sample table and did not switch files; it is not
switching evidence, although its effects workload recorded 178 starvation
events. Both runs remain retained. The corrected run is in
`artifacts/seek/ectocore-128/stress-retry/`, including command logs, phase coverage,
WAV, coherent snapshots and stacks. Its ELF SHA-256 is
`e08485bc1f7112e3090620ce76335d507b3eab9678c7ad5ccdcb4446bc2d7819`.

After the test, the normal 128-frame ELF listed above was restored and verified,
with test commands disabled. These results support dropping 128 from standard
ectocore releases or marking it experimental. They do not establish the result
for 256 frames, 441 frames under this same workload, or ezeptocore hardware.

## 128 release removal and 256-frame comparison

The release workflow no longer builds or publishes either ectocore 128-frame
variant (200 MHz or no-overclock). Historical Makefile targets and configurations
remain available to reproduce these tests. Ezeptocore release variants are
unchanged; that hardware has not been tested here.

The 256-frame build passed compilation and programming at 200 MHz. The same
four-phase serial-control sequence and 90-second capture recorded **one DMA
starvation event**, versus 140 in the preceding 128-frame run. This is a large
improvement, but still fails a strict zero-starvation requirement. Randomized
effects differ between runs, so this is not a sample-identical benchmark.

| Approximately 20-second phase | 128 starvation events | 256 starvation events |
|---|---:|---:|
| Switching alone | 12 | 0 |
| Effects on one sample | 2 | 0 |
| Combined switching/effects | 46 | 1 |
| Added stretch/reverse | 76 | 0 |
| Full capture including transitions | 140 | 1 |

The 256-frame capture confirmed eight selected samples, 241 file-generation
changes, up to five active effect flags, both directions and Q8 stretch values
256/1251. No maps were built or written during playback. The one starvation
event inserted 576 silence frames (about 12.9 ms at the measured sample rate).
Callbacks averaged 2,087 us; the observed lifetime maximum was 7,759 us against
a 5,734 us producer-block budget. Normal reads reported 32 short reads with zero
FatFs error results; their individual causes remain unclassified. Audio was
captured on both channels without clipping.

Both stack guards remained intact, with 1,620/2,192 untouched bytes on cores 0/1.
Evidence is in `artifacts/seek/ectocore-256-stress/`; test ELF SHA-256:
`e2f4519557effcb3324ac538efb491f2fe7b0fc5bfcef0bdc4b04ee9dc181f85`.

The normal 256-frame firmware, with diagnostics enabled and test controls
disabled, was then programmed and verified. It is retained in
`artifacts/seek/ectocore-256/`, ELF SHA-256:
`79884605a1ae3c3a6e265d00559c64a7974056bb6e8ad516025d619cf41731b4`.
This supersedes the earlier statement that 128 remains installed.

Later experiments tested [next-file preparation](seek-next-file-experiment.md)
and [an extra maintained output buffer](seek-output-buffer-experiment.md). The
latter passed two consecutive stress runs without starvation, but the user
rejected its added continuous latency. The original normal 256-frame firmware
is restored with both experimental options disabled; see that report for the
current ELF and latency/RAM tradeoffs.

[Detailed callback timing](seek-detailed-timing.md) subsequently captured the
callback spanning starvation with both experiments disabled, identifying delay
sample processing, SD access and filtering as major contributors.
