# Pico SDK 2.3.1 upgrade and validation

Status: SDK 2.3.1 integration and hardware validation completed. The default
production image is running on the connected Zeptocore. The full comparison
matrix and default-setting transport soak passed; the extended 256-frame/125 MHz
transport test reproduced one baseline underrun on both SDK versions (see below).
The initial preflight below preceded programming. The original flash is now
backed up under artifacts/sdk-upgrade/original with SHA-256 checksums. Both SDK
versions were explicitly programmed for matched hardware tests.

## Connected hardware and preflight (2026-09-17)

The user supplied a Zeptocore connected by USB and Raspberry Pi Debug Probe
(SWD only, no UART), with stereo output routed to the Scarlett 2i2.

- CMSIS-DAP probe enumerates as 2e8a:000c, serial E6632891E3889E30.
  OpenOCD successfully examines both RP2040 cores at 1000 kHz.
  This establishes SWD connectivity, not firmware health or diagnostic identity.
- OpenOCD, ARM GCC/nm, and picotool are installed.
- Scarlett capture is available at hw:4,0, stereo S32_LE at 48000 Hz.
- Five-second capture: artifacts/sdk-upgrade/preflight/input.wav.
  Channel RMS levels were approximately -94.4 and -91.2 dBFS, with no clipped
  samples. This is effectively silence/noise, not a validated playback signal.
- No Zeptocore appeared in lsusb, and amidi -l listed no MIDI endpoints.
  Establish USB enumeration and audible playback before automated workloads.
  Discover the MIDI port again after each firmware enumeration; do not assume
  the scripts' default hw:6,0,0 is correct.

## Implementation sequence

1. Preserve the currently programmed image before first programming. Determine
   actual flash geometry and back up the required flash contents with hashes;
   do not assume the historical Ectocore 2 MiB backup covers this device.
   Retain a known-good restore image and record current USB identity.
2. Build the same project sources against SDK 2.2.0 and extras sdk-2.2.0,
   retaining compiler versions, ELF/map/UF2 files, source revision/dirty state,
   generated headers, dependency revisions, and configuration.
3. Update Makefile and CI to SDK 2.3.1 / extras sdk-2.3.1. Refresh import helpers
   under lib/cmake and pin optional fetch paths. Handle existing dependency
   directories explicitly: current Make clone recipes only run if absent.
   Preserve local dependency changes, including the TinyUSB patch.
4. Review TinyUSB patch applicability and make unexpected source layouts fail
   clearly. Check custom audio/SDIO synchronization, DMA/PIO, shared interrupts,
   USB reset/CDC/MIDI integration, and RAM code placement. Keep compiler,
   application options, SD clock, and checksum policy fixed for comparison.
5. Use fresh build directories for each SDK/configuration. Build all shipped
   CI variants and ezeptocore_midi, run existing native suites and visualizer
   build checks, and compare memory/linker maps.
6. Execute the Zeptocore hardware matrix below, then retain the validated
   production image and a documented rollback path.

Upstream references:
- [SDK 2.3.1](https://github.com/raspberrypi/pico-sdk/releases/tag/2.3.1)
- [Intervening SDK 2.3.0 changes](https://github.com/raspberrypi/pico-sdk/releases/tag/2.3.0)
- [Extras release tags](https://github.com/raspberrypi/pico-extras/tags)

## Hardware comparison

Use the same SD card, samples, input gains, MIDI workload, compiler, and
configuration for each SDK pair. Compare 441 and 256 producer frames at default
overclock and CORE_NO_OVERCLOCK=ON. Keep seek maps enabled in both versions:
the independent variable is SDK version, not map attachment.

Build diagnostic cases with SEEK_DIAGNOSTICS=ON and SEEK_TEST_CONTROLS=ON.
Leave fresh-index, fragmented benchmark, and audio fixture mutation options
disabled. Exercise ordinary playback, stretch, reverse, slices, variation,
repeated sample switching, and supported clock stop/restart workloads.
Use 60-second initial captures, followed by a longer mixed-workload soak.
Only select variations/long-file workloads supported by the actual card.

Reuse:
- scripts/zeptocore_validate_build.py: explicitly flashes one diagnostic ELF,
  captures workloads and audio, then runs a separate halt/reset stack check.
  Pass --midi-port and --audio-device explicitly. Confirm media writes have
  finished before the final stack inspection; split that step if needed.
- scripts/zeptocore_debug_server.py and zeptocore_debug.py: exact-ELF identity
  checks and live SWD snapshots; one process owns OpenOCD at a time.
- scripts/zeptocore_audio_report.py: stereo levels, clipping, and candidate gaps.
- scripts/zeptocore_observer_check.py: separately measure diagnostic observer
  overhead using matching SEEK_TIMING_WITNESS builds, diagnostics ON/OFF,
  collection-only, and 1/4 Hz polling.
- scripts/zeptocore_stack_report.py: inspect only after recording finishes.
  Verify its linker-symbol assumptions against the new SDK before use.

The existing build-matrix script compares map attachment, not SDK versions.
It now accepts --sdk-path and --extras-path and owns fresh build directories
inside its output directory. The SDK comparison uses explicit, separate CMake
builds with maps enabled in both versions; commands and hashes are retained in
artifacts/sdk-upgrade/build-matrix.json.

After diagnostic tests, smoke-test the actual production configurations with
diagnostics/test controls disabled, visualizer OFF and ON, USB reconnect, and
the existing bootloader/upload path. No UART logging is expected.

## Evidence and acceptance

Store results below artifacts/sdk-upgrade/<sdk>/<configuration>/<run>/:
exact ELF/hash and map, build options/tool versions, flash logs, device info,
MIDI event schedules, raw snapshots and summaries, WAV/audio reports, and
post-capture stack reports.

Require valid captures without resets/stalled domains, progressing audio,
no new DMA starvation or seek/read errors versus baseline, intact stack guards,
and no unexplained memory or timing regression. Compare callback and seek
histograms plus sample-switch latency; distinguish cumulative maxima from
capture-window measurements. Repeat suspicious differences in alternating
SDK order before attribution.

Audio silence alone does not prove an underrun, and analog recordings need not
be bit-identical. Correlate gaps with source material and firmware counters.
Record zero-signal captures as inconclusive. Hardware results cover Zeptocore;
other device configurations receive build coverage until hardware is available.

See [diagnostics usage](seek-diagnostics.md) for transport and measurement
semantics. Historical hardware results are context, not evidence that this
new SDK or currently attached image has passed.

## Implementation and automated results

- Make and CI pin SDK 2.3.1 and extras sdk-2.3.1; refreshed import helpers also
  pin their default fetch versions. Make rejects stale dependency checkouts
  without changing them.
- TinyUSB's revision is identical between these SDK releases. The patch now
  changes only the two endpoint mutex calls, preserves an already patched file's
  timestamp, honors PICO_TINYUSB_PATH, and rejects an unexpected source layout.
- All device Make targets select their definitions before CMake configuration.
- All 13 production build cases passed, including visualizer and Ezeptocore MIDI.
  The default/ON/OFF and invalid-target visualizer build checks passed.
- FatFs, seek diagnostics, audio-source, and DSP native suites passed. MIDI and
  telemetry tests passed with CC=clang, as in CI. The default host GCC run hit
  an existing fortified-vsnprintf warning promoted to an error.
- The SDK integration tests check targeted/idempotent patching, rejection of
  unknown lock layouts, and device selection before configuration.
- Twelve matched diagnostic/witness builds passed. Timer callbacks and the I2S DMA
  handler retain RAM addresses; stack boundary symbols remain compatible.
- Dependencies and generated headers were prepared with the documented Python
  3.11 environment. The preexisting Python 3.13 environment and incomplete SDK
  checkout were preserved in the artifact directory.
- A concurrent workspace cleanup removed intermediate root build directories.
  Comparison builds were recreated under artifacts/sdk-upgrade/builds. Regenerating
  headers changed SoX-dithered startup cue samples and the associated diagnostic
  identity; all other loadable bytes in the first baseline stayed identical.
  The first baseline retains its original ELF and a rebuild note. The initial CPU figures
  are exploratory; extended CPU repeats were deprioritized at the user's request
  in favor of functional validation.

The baseline firmware restored USB MIDI enumeration at hw:6,0,0. A subsequent
five-second capture had signal on both Scarlett channels (RMS approximately
-35.3/-32.4 dBFS) and no clipping. The earlier silent preflight is not counted
as a successful playback test.

## Rollback

The probe reported a 32 MiB flash bank; flash-32m.bin retains its OpenOCD dump.
flash-first-2m.bin retains the prefix containing all flash addresses programmed
by the test ELFs. SHA256SUMS covers both files. Stop the diagnostic server and
any other probe owner before restoring this prefix with OpenOCD:

```sh
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c 'adapter speed 5000' \
  -c 'program artifacts/sdk-upgrade/original/flash-first-2m.bin 0x10000000 verify reset exit'
```

This restores the pre-upgrade flash prefix, not SD-card contents. Its original
USB enumeration/playback was not functional in preflight. A tested SDK 2.2.0
baseline ELF is also retained for firmware rollback.

## CPU comparison method

The requested CPU comparison uses scripts/zeptocore_cpu_report.py on the saved
raw snapshots. Report callback elapsed-time occupancy as
100 * delta(callback.total_us) / delta(audio.timestamp_us), plus mean callback
time, percentile ranges, rendered frames, and DMA starvation. Device timestamps
handle uint32 rollover; host polling jitter is not used as the denominator.
Report absolute percentage-point changes and relative changes for matched
SDK/workload/buffer/clock pairs, with repeated runs for small differences.

This is audio-core callback wall-time occupancy, including waits and preemption,
not instruction-cycle usage, core-0 utilization, or total dual-core utilization.
Collection publishes callback duration one call late; long windows limit that
boundary effect. Independent timing-witness runs with diagnostics compiled out
would check whether diagnostic observer cost changes the conclusion. Those
paired images were built, but extended witness captures were not run.

The user subsequently prioritized functional reliability over CPU improvements.
Retain the measured callback figures as descriptive observations; do not claim a
CPU optimization or extend testing solely to resolve sub-percent differences.
The final functional soak uses the transport workload, alternating MIDI Stop,
Start/Continue, sample selections, and ordinary/stretch playback. Intentional
stops create expected silence and are retained in the control schedule.

## Completed comparison matrix

Each row covers six 60-second captures: ordinary playback, stretch, repeated
sample switching, reverse, slices, and variation. All 48 captures were valid,
with zero DMA starvation, zero reported metric errors, and zero clipped samples.
Both Scarlett channels had signal in every recording. All boots reused the 76
existing maps without map construction or index writes.

| SDK | Producer frames | Clock MHz | Core 0 untouched stack bytes | Core 1 untouched stack bytes |
| --- | ---: | ---: | ---: | ---: |
| 2.2.0 | 256 | 125 | 1548 | 2224 |
| 2.2.0 | 256 | 225 | 1604 | 2224 |
| 2.2.0 | 441 | 125 | 1612 | 1480 |
| 2.2.0 | 441 | 225 | 1628 | 1480 |
| 2.3.1 | 256 | 125 | 1548 | 2224 |
| 2.3.1 | 256 | 225 | 1588 | 2224 |
| 2.3.1 | 441 | 125 | 1588 | 1480 |
| 2.3.1 | 441 | 225 | 1604 | 1480 |

Every stack bottom guard was intact. These measurements bound only the exercised
workloads. SDK 2.3.1 uses 388 fewer static RAM bytes in the matched diagnostic
builds; flash usage is lower by 352 bytes at 441 frames and 360 at 256 frames.
Reverb allocation stayed at four combs and three allpasses for both SDKs.

The detailed comparison is retained in hardware-summary.json, cpu-results.json,
memory-comparison.json, and per-run artifacts beneath artifacts/sdk-upgrade/.
The source/configuration snapshot is tested-source.tar.gz with its SHA-256 file.

## Extended transport checks

The 180-second combined Stop/Start/Continue, sample-selection, and stretch
workload exercised 181 control events per run. At 256 producer frames and
125 MHz, both SDK 2.2.0 and 2.3.1 recorded one DMA starvation event (576 frames)
at approximately 6.13 seconds, during the early restart/selection sequence.
Neither run recorded another starvation event, metric errors, clipped audio,
or damaged stack guards. This is a reproduced baseline limitation for that
configuration, not a clean zero-underrun result or evidence of an SDK regression.
Raw snapshots, MIDI schedules, and stereo recordings are retained in
artifacts/sdk-upgrade/final-hardware/soak and soak-baseline.

The SDK 2.3.1 default configuration (441 producer frames, 225 MHz) passed the
same 180-second workload with 181 control events, zero DMA starvation, zero
metric errors, no clipped audio, and intact stack guards. Its evidence is in
artifacts/sdk-upgrade/final-hardware/soak-default.

## Final production checks and device state

- Production visualizer firmware passed a 12-second stereo capture with MIDI
  clock, Stop/Start/Continue, sample/stretch controls, and `view=2` telemetry.
  The first attempt overlapped USB re-enumeration after SWD programming and
  aborted with a missing ALSA port; evidence is retained as first-attempt-*.
  The same programmed image passed after enumeration completed.
- Root `make upload-built` successfully requested BOOTSEL over MIDI, uploaded
  and verified the default production UF2 with picotool, and started it.
- Default production firmware passed the same 12-second audio/MIDI check,
  with visualizer telemetry absent as expected. Another capture passed after
  a Linux USB bus reset of the identified Zeptocore device. This was a software
  bus reset, not a physical power-cycle test.
- Each successful production capture sent 822 MIDI messages, had signal in
  both Scarlett channels, and contained no clipped samples. No UART was used.
- The connected device was left running SDK 2.3.1 with the default 441-frame,
  225 MHz configuration; diagnostics, timing witness, test controls, and
  visualizer telemetry are disabled. The validated image is
  artifacts/sdk-upgrade/production-final/default.uf2 (also zeptocore.uf2 at the
  repository root). Hashes are recorded in production-final/manifest.json.

Final recordings, control schedules, upload logs, and completion metadata are
under artifacts/sdk-upgrade/final-hardware/. Other device variants received
build coverage only. No material CPU improvement is established by these tests.
