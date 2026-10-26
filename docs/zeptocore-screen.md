# Zeptocore Sharp memory display

`make zeptocore-screen` builds full Zeptocore audio firmware with an Adafruit
4694 (400 x 240) memory LCD. The isolated build lives in
`build-zeptocore-screen-firmware`; its output is `zeptocore-screen.uf2`.
`make zeptocore-screen-test` retains the standalone HELLO WORLD test.

| GPIO | Wire | LCD |
| --- | --- | --- |
| 26 | Blue | CLK |
| 27 | Purple | DI |
| 28 | Gray | CS |
| 3.3 V | | VIN |
| GND | | GND |

Disconnect all three potentiometer wipers. The screen build undefines
`INCLUDE_KNOBS`, including ADC setup and knob filter allocations. Existing
pads, clock, effects, presets and USB MIDI retain their controls and identity.
Bank/sample selection through the existing MIDI controls remains available.
Ordinary firmware builds explicitly disable `ZEPTOCORE_SCREEN`.

## Card preparation

Opening a card/project in the JUCE sample manager automatically prepares screen
waveforms for its committed primary audio, including with Autosave off. Pending
edits remain pending. Saving new rendered audio writes its waveform companion
in the same recoverable transaction. Moves, deletes, undo and duplication carry
the companions with their samples. Metadata-only edits retain WAV bytes and
reuse the waveform amplitudes. A missing or corrupt derivative is regenerated
on opening; valid unchanged files retain their modification times.

The native application also exposes a windowless preparation command:

```sh
"/path/to/_core sample manager" --prepare-screen-waveforms /path/to/card
```

It uses the normal project worker and transaction path with Autosave off, and
reports the prepared/expected count. Existing cards are adopted as editable
projects through the manager's usual process. Original card audio is retained.
Close other manager windows using the same project before running this command.

There is no waveform generation on the instrument. Missing or rejected files
show WAVEFORM UNAVAILABLE while audio continues. Older firmware ignores these
optional files. The browser manager and browser visualizer are unchanged.

## Companion format, version 1

The file for `bank1/0.0.wav` is `bank1/0.0.wav.wfm`. Exactly 448 bytes are
required. Integers are little-endian; serialized C struct layouts are never used.

| Offset | Type | Value |
| --- | --- | --- |
| 0 | 4 bytes | ASCII ZCW1 |
| 4 | u16 | Version 1 |
| 6 | u16 | 384 columns |
| 8 | u32 | Unpadded PCM byte count from .info |
| 12 | u32 | 44100 or 88200 Hz |
| 16 | u8 | 1 or 2 channels |
| 17 | u8 | Encoding 1: unsigned mirrored peak amplitude |
| 18 | u16 | Zero |
| 20 | u32 | FNV-1a of all .info bytes, seed 2166136261 |
| 24 | 32 bytes | SHA-256 of the complete primary WAV |
| 56 | u32 | IEEE CRC32 of bytes 64–447 |
| 60 | u32 | IEEE CRC32 of bytes 0–59 |
| 64 | 384 bytes | Peak magnitudes, 0 silence to 255 full-scale |

CRC32 uses reflected polynomial 0xedb88320, initial/final XOR 0xffffffff.
Quantization is `round(max(abs(min), abs(max)) * 255 / 32768)` across both
channels and every overlapping source bin. The JUCE visualizer's existing
4096-bin peaks provide this data, excluding circular padding. Newly rendered
audio shares its analysis with the JUCE visualizer instead of running it twice.

Firmware validates size, rate, channel count, .info identity and both CRCs.
The manager verifies complete audio fingerprints. Firmware intentionally does
not read entire WAVs to recheck SHA-256; manually replacing an equal-format WAV
requires reopening the card in the manager to regenerate its waveform.

## Runtime

The current bank's 16 physical slots occupy a fixed 6144-byte peak cache,
allocated once after the existing DSP/reverb sizing and before releasing the
boot filesystem guard. Bank/sample changes allocate no display memory.
Startup and bank transitions load the selected slot first under existing
filesystem ownership. Each step performs at most one open, read (448 bytes),
or close. Optional loading has a 250 ms budget and yields before the normal
bank-transition deadline. Remaining work resumes only during an acknowledged,
already-stopped playback window. Cancellation and rollback discard old validity.

The audio core publishes a bounded atomic snapshot of source phase, identity,
slice and transport. The foreground draws a 384-column waveform with slice
ticks, an inverted active region, cursor, bank/sample/BPM header and transport
footer. The cursor follows source playback, including reverse and granular
stretching; effects' output waveforms are not analysed. Frames use one snapshot
and are discarded if metadata changes while drawing.

The screen targets five frames per second. SPI1 runs at 1 MHz, using a dedicated
TX DMA channel with foreground completion polling and eight-row stripes. No
screen IRQ or PIO state machine is used. There is no full framebuffer. The
screen reuses the metadata loader's foreground scratch space and copies each
stripe into its private DMA packet before returning. Total additional retained
RAM stays below 8 KiB, including the cache and extra RAM-resident code. VCOM alternates
every 500 ms independently of stripes, including stopped/unavailable views.
`screen_status` is an independent SWD-readable diagnostic symbol.

## Checks

```sh
python3 test/screen/run.py
python3 test/bank_metadata/run.py
ctest --test-dir sample-manager/build/linux-x86_64 -R screen_waveforms --output-on-failure
```

The shared golden bytes were independently constructed with Python struct/zlib.
Host checks cover every corrupted byte and truncation boundary, sparse slots,
I/O errors, deadline/resume, cancellation, raster layout and metadata generation.
Native tests exercise stereo/silence, automatic backfill, metadata edits,
move/delete/undo, unchanged-file reuse and corrupt-file repair.

## Hardware verification, 2026-10-26

The attached WeAct Zeptocore and Adafruit 4694 passed a sweep of all 96 samples
in six banks. All 96 companions were generated through the JUCE manager;
SHA-256 checks confirmed that all 192 original WAV/`.info` files were unchanged.
The installed normal firmware passed the full sweep at approximately 5 fps,
with no display-transfer, waveform-validation, allocation or metadata errors.

Matched builds with knobs disabled retain four reverb combs and three allpass
stages. The fixed peak cache is allocated after DSP sizing to preserve those
counts. Additional retained SRAM, including RAM-resident code and allocator
rounding, is 7856 bytes for the normal build and 8160 bytes for the diagnostic
plus visualizer build. The normal sweep retained over 21 KiB of free heap.

Instrumented sweeps covered reverse, granular stretching, retriggers, stop/start,
retargeting during waveform loading, and completion of deferred cache reads
while stopped. Audio counters recorded no DMA starvation or error silence.
Post-workload stack dumps retained 1400/1336 untouched bytes on cores 0/1,
with both bottom guards intact; these observations cover the exercised workload.
Eight-second Scarlett captures contain unclipped stereo audio.

The final installed firmware has diagnostics, test controls and USB visualizer
telemetry disabled. Screen support remains enabled. Captures, original flash
backup, retained ELFs, hashes and reports are under `artifacts/zeptocore-screen/`.
