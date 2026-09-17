# On-demand seek diagnostics

The service and shared seek maps are implemented; see
[validation status](seek-implementation-status.md).
The firmware server uses SRAM snapshots over the Debug Probe SWD connection.
Probe UART is not needed. USB MIDI remains available for musical controls.

## Build and run

```sh
cmake -S . -B build-seek-441 \
  -DCORE_COMPILE_DEFINITIONS="$PWD/lib/cmake/zeptocore_compile_definitions.cmake" \
  -DSEEK_DIAGNOSTICS=ON \
  -DPICO_SDK_PATH="$PWD/pico-sdk" -DPICO_EXTRAS_PATH="$PWD/pico-extras"
cmake --build build-seek-441 -j8
```

Use `_256.cmake` and a separate build directory for 256 producer frames. The
supported zeptocore build/validation matrix is 441 and 256 frames; 128-frame
measurements are historical. Building does not program the device. With an original-image backup
and when intentionally programming, the upload command is:

```sh
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c 'adapter speed 5000' \
  -c 'program build-seek-441/_core.elf verify reset exit'
```

Run the host service with the exact ELF corresponding to the programmed image:

```sh
python3 scripts/zeptocore_debug_server.py --elf build-seek-441/_core.elf
python3 scripts/zeptocore_debug.py status --once
python3 scripts/zeptocore_debug.py status --once --json
python3 scripts/zeptocore_debug.py watch --interval 1
python3 scripts/zeptocore_debug.py capture --duration 60 \
  --workload manual/exploratory --out artifacts/seek/my-capture
```

For the historical diagnostic firmware retained in this workspace, first
program its matching ELF, then use:

```sh
.venv/bin/python scripts/zeptocore_debug_server.py \
  --elf artifacts/seek/freeze-fix-441/firmware.elf
```

The service owns one OpenOCD process, binds a user-only local Unix socket, and
serializes requests. It uses RAM reads/writes only in the validated mailbox and
checks both cores are running. Ctrl-C stops the host processes without resetting
or halting firmware. A stale ELF causes an identity error before mailbox writes.
Do not run a programmer or second debug server against the probe concurrently.

`device.info`, `seek.status`, `audio.status`, `memory.status`, `maps.status`,
`maps.layout`, `media.cached`, `maps.benchmark`, `maps.fixture`, `capture.start`,
and `capture.stop` are the JSON API commands. Test reports are available only in
the corresponding test firmware.
`maps.status` reports preparation/reuse/build/load/write counts, elapsed times,
cache bytes, errors and ownership waits. Status commands do not change playback or read
the filesystem. Captures record cumulative deltas without resetting counters.

## Interpretation

- Latency histograms have fixed inclusive upper bounds. Percentiles are ranges;
  the final bin is unbounded. `lifetime_max_us` is not a window maximum.
- IRQ `starvation_count` counts actual missing consumer buffers after startup.
  Producer-buffer misses and deliberate mute are separate counters.
- Audio callback statistics include no-buffer calls. The output consumer uses
  256-frame DMA buffers even when the producer uses 441 or 128 frames.
- `publish_us` is the copy/publication duration. The corresponding maximum
  counter appears in a later frozen response. Different domains have different
  snapshot timestamps and progress counters.
- The audio domain publishes before recording the just-finished callback's
  duration, allowing that duration to include publication itself. Its I/O
  counters can include that current callback; duration counters lag by one call.
- Firmware counter saturation invalidates window summaries. Device resets,
  stalled domains, and partial captures are reported as failures.
- `heap_free_at_startup_bytes` is a startup checkpoint, not a continuous heap or
  stack high-water mark. Unavailable measurements must not be treated as zero.

## Independent observer and audio checks

`SEEK_TIMING_WITNESS=ON` adds a small seqlock-protected callback timer outside all
diagnostic hooks. It is a test option enabled identically in both paired builds,
including the build with `SEEK_DIAGNOSTICS=OFF`. The main service does not require
it. Its bookkeeping runs after the measured interval. Witness counters are
limited to bounded test runs; inspect reboot/program logs with each comparison.

`scripts/zeptocore_observer_check.py` is a separate, explicit programming workflow
that alternates paired ELFs, sends existing MIDI controls, records Scarlett audio,
and compares compiled-out, collection-only, 1 Hz and 4 Hz cases. It does flash and
reset the target. The normal debug server never does so.

```sh
arecord -D hw:4,0 -f S32_LE -r 48000 -c 2 -d 30 -t wav capture.wav
python3 scripts/zeptocore_audio_report.py capture.wav
python3 test/seek_diagnostics/run.py
```

WAV silence candidates and signal levels supplement firmware evidence. Natural
silence in the sample can look like a gap; a WAV report alone does not establish
audio starvation or bit-identical output.

## Stack inspection and control workloads

Use `-DCORE_NO_OVERCLOCK=ON` for the existing 125 MHz clock path.
`zeptocore_validate_build.py` explicitly programs a build, records normal/stretch/
selection workloads and Scarlett audio, then runs a separate stack inspection.
The latter **halts both cores and resets afterward**, after audio recording has
ended. The runner stops playback and waits for pending map work before this step;
`--skip-stack` omits the halt/reset and leaves firmware running. Never run a
standalone stack inspection during firmware SD writes. It paints unused stack memory at
boot, then measures intact paint in an exercised workload. Core 0's available
stack starts after `__scratch_y_end__`; `__StackBottom` is only the linker's
minimum reservation. Core 1 uses its separately reserved 4 KiB stack.

Select `--modes transport --seconds 180` to alternate MIDI Stop, Start/Continue,
sample selection, and ordinary/stretch playback once per second. This uses
normal MIDI controls and records their schedule; intentional stops produce
expected silence. Check DMA starvation counters separately from audio gaps.

`SEEK_TEST_CONTROLS=ON` enables test-only MIDI CC 110 (reverse), 111 (slice),
112 (file variation) and 113 (audio variant), routed through existing musical
control functions. The validation runner's `--test-controls` option exercises
these and records the host event schedule. Host event timing does not guarantee
identical alignment to audio blocks; byte-for-byte map correctness needs the
separate deterministic filesystem tests. These controls are absent by default.

Test CC 114 saves to an empty preset slot 0..15 only; CC 115 loads an existing
slot. Both use the normal acknowledged save/load and reopen functions. They
restore the current slot selector afterward. Feature bit 512 identifies these
protected preset controls. `zeptocore_lifecycle_check.py` waits for active
playback, checks that the requested slot is empty, then records save, selection
change and load. A successful test creates a preset; it does not delete it.

`media.cached` reads only four fixed, typed ELF symbols: the detected audio-variant
count, logical sample count, current preset slot and 16 preset-presence flags.
It reads them twice and checks firmware/session identity. It performs no directory
scan and accepts no host-supplied memory address. A zero variant count can also
mean that the initially selected sample lacks a complete alternate WAV pair.

## Map layout and sample-switch measurements

Feature bits identify map counters (8), attachment enabled (16), packed heap and
reverb counts (32), request-to-DMA tracing (64), and cache-layout replies (128).
Test-index builds additionally set bit 256. `SEEK_TEST_FRESH_INDEX=ON` requires
test controls and uses `.core_seek_test` for automatic first-use/reuse measurements.
The production default remains `.core_seek`; the test option changes only the
firmware-owned index path and is included in the build identity.
The mailbox remains 1840 bytes. Command 1 snapshots ordinary counters. Command 2
uses argument 0 as a 16-bit physical file ID, with the other arguments zero, and
places a cache-layout record in the 480-byte control payload. Other domains still
return their ordinary snapshots. The host finishes any outstanding request
before issuing one of the other kind.

`maps.layout` takes `file_id = bank_index << 12 | sample << 8 | physical_variant`,
where bank_index is zero-based and physical_variant combines variation plus
twice the audio variant. It returns only a current validated RAM cache entry;
a miss reports unavailable and never queues a load. Captures retain this layout
when it matches their starting playback state. The reply includes exact extents,
file size, cluster geometry and mount generation. The map writer and layout
publisher run on core 0; core 1 can atomically invalidate an entry but cannot
replace its immutable table during this copy.

Switch tracing starts in the existing MIDI sample-selection handler. A successful
matching file open tags rendered source buffers. The conversion code carries the
first occurrence into the consumer buffer, including its frame offset when a
441-frame producer is split or two 128-frame producers are combined. DMA records
the elapsed time plus that offset at the actual I2S rate. This ends at DMA output;
it excludes PIO/codec pipeline and analog/capture latency and does not assert that
the source's first sample is nonzero.

There is one measurement in flight. Overlapping requests are counted as unmeasured;
an unfinished request expires when a later request arrives after two seconds.
IRQ counters 9..15 contain completion count, last/max latency, requested/completed
tokens, overlapping requests and expiries. Raw host snapshots retain individual
latencies at one switch per second. Summaries report missing observations if
multiple completions occur between polls. Use `--repeat-switches` with the
validation runner to alternate the same two MIDI selections once per second.

## Reserved fragmented-file benchmark

`SEEK_TEST_FRAGMENT_BENCH=ON` requires diagnostics and test controls. It runs
automatically during boot, under the existing exclusive media ownership, before
normal map preparation. `maps.benchmark` only retrieves the immutable completed
512-byte RAM report. It cannot start or repeat the benchmark. Reads are bounded
to 256 bytes, checked twice, and validated against the running ELF and session.

The fixture lives in `.core_seek_bench/fragment.bin`; existing names are never
truncated. It has 31 one-cluster fragments. During initial creation a temporary
spacing file reserves 128 clusters between each fragment, spreading FAT links
across metadata sectors. At the connected card's 64 KiB cluster size this needs
about 240 MiB of temporary free space; the spacing file is then removed and the
1.9375 MiB fixture retained. Interrupted or mismatched fixtures report failure;
they are not silently overwritten. Production builds compile this code out.

Each of 768 pairs uses the same file, endpoint and target position with ordinary
and mapped seeking; pair order alternates. The sequence mixes reverse offsets,
seeded slice jumps including a 44-byte WAV offset, and distant grain-like jumps.
Seek and 512-byte read timings are separate; every returned byte and position is
checked against the deterministic pattern. A test-only `get_fat` counter records
allocation-link visits during timed seeks. This is a filesystem microbenchmark,
not playback of the synthetic file. Its measurements include that counter's
small cost on ordinary seeks, and do not measure DSP or analog latency.

The fixture borrows the existing preparation handles, table and sector buffer;
its report and two counters add 520 bytes in this test build only. Combined fixed
diagnostic data is 3988 bytes including alignment. Benchmark setup/run time is
excluded from map `prepare_us` and included in overall startup time.

## Guarded audio-variant fixture

`SEEK_TEST_AUDIO_FIXTURE` defaults to 0 (compiled out). Values 1 and 3 create or
verify `bank1/0.2.wav` and `bank1/0.3.wav` by copying the corresponding existing
`.0.wav` and `.1.wav`. Values 2 and 4 remove those respective copies. Both
diagnostics and test controls are required. This is automatic boot work under
exclusive media ownership; no server request can initiate it.

Creation uses `FA_CREATE_NEW` and never overwrites a name already present.
Completed copies have markers in `.core_seek_bench` containing card CID, length,
starting cluster, SHA-256 and CRC. Reuse and removal verify the marker and the
entire copy. Unknown, altered, unmarked or pinned targets are refused. An
interrupted unmarked copy requires manual inspection; it is not silently deleted.
The fixture invalidates the affected map before mutation. Content hashing here
proves ownership of a disposable test file; allocation-map validation never
hashes WAV contents.

Feature bit 1024 exposes `maps.fixture`, which only reads the immutable completed
32-byte boot report twice. It reports result, mode, operation, file size, starting
cluster and elapsed time. Fixture copying/checking is excluded from map
`prepare_us`. Its report adds 32 test-only static bytes (4020 fixed diagnostic
bytes if combined with the fragmented benchmark). Copying uses core-0 stack;
the exercised cleanup build retained 832 untouched bytes with an intact guard.

The hardware validation created both alternate WAVs, verified their distinct
maps through normal and variation opens, then removed them using these guards.
Production firmware has neither fixture enabled. See
[hardware results](seek-hardware-results.md) for the retained evidence.

`load_us` now measures index header/record I/O during startup and later cache
loads. Older captures recorded runtime load work only and may show zero despite
performing startup index reads. Compare those versions using their retained ELF.
