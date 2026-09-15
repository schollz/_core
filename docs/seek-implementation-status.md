# Seek implementation status

**Follow-up correction:** a user-reported freeze exposed a retrigger-pitch stack
overflow outside the original workload matrix. The source-buffer fix is now
programmed and passes pitch/tempo sweeps on both supported buffer sizes. See
[cause, verification and rate limits](seek-freeze-fix.md).

The full GOAL.md implementation is authorized and implemented. The supported
441/256-frame playback matrix, final observer gate and fragmented benchmark
passed. Physical power-cycle/reinsertion also passed; GOAL.md is complete.
The authoritative measurements and limitations are in
[hardware results](seek-hardware-results.md). Usage is in
[on-demand diagnostics](seek-diagnostics.md); limits and ownership rules are in
[the shared design](seek-map-design.md).

## Implemented behavior

The common firmware builds FatFs maps automatically at startup, validates every
saved allocation chain on each mount, and persists completed work in a bounded,
recoverable SD index. An unchanged writable card reuses its records without
building maps or rewriting the index. Two immutable RAM entries accelerate
cached files; uncached or unsupported files retain ordinary seeking. Cache
loading and preparation require acknowledged filesystem ownership outside audio
rendering. The feature is shared by the device wrappers without a zeptocore-only
implementation gate.

The SWD service reads coherent snapshots while both cores run. Its fixed mailbox
requests do not initiate filesystem work, allocation, media ownership or musical
controls. Normal builds exclude the optional test MIDI controls and boot fixtures.
An exact matching ELF and boot session are checked before mailbox writes.

## Final integration audit

All active opens/closes of `fil_current` use `audio_file_open` and
`audio_file_close`. Startup, normal/stretch selection, variation, audio variants,
settings save/load and missing-file recovery route through them. Settings
operations acquire the same acknowledged filesystem ownership. Startup manifest
and `.info` reads run under the boot guard. Unmount detaches references; remount
advances validation, and I/O recovery performs shared startup again.

No active production operation replaces or truncates audio WAVs. The optional
boot audio fixture invalidates its affected map before creating/removing a
verified test copy, under startup ownership. The index and fragmented benchmark
write only their reserved paths. `WavFile_Load`, `Chain_load`/`Chain_save`, and
`big_file_test` have no active application callers; `main.c` has only a commented
chain-load call. Future callers of those legacy mutators must acquire ownership
and invalidate affected maps. Bundled filesystem utility entry points do not
establish application ownership by themselves.

Startup restores a valid saved selection before selecting the initial map and
file. Saved settings keep their existing byte layout while ignoring serialized
heap/callback addresses. Runtime loading uses current allocations and callbacks.
Audio-variant discovery now uses the same one-based bank path as playback.

## Native correctness evidence

Both commands passed on the final production source:

```sh
.venv/bin/python test/fatfs_seek/run.py
.venv/bin/python test/seek_diagnostics/run.py
```

Logs and 470 source fingerprints are retained in
`artifacts/seek/native-production-validation`. The diagnostics suite includes
22 Python protocol/server tests plus native aggregation/mailbox and actual PCM
conversion tests. Native C/C++ tests use AddressSanitizer and UndefinedBehaviorSanitizer.

Coverage includes:

- Actual bundled FAT12/16/32/exFAT: 1500 byte/position comparisons per filesystem
  on fragmented files, boundary and EOF behavior, malformed maps, cycles and I/O
  errors. Mapped seeking visits no FAT sectors for those sequences.
- Full-width cluster-order arithmetic, exact cluster/sector boundaries after
  reads, and 128/256/441-frame PCM conversion with unchanged sample bytes.
- Pinning, eviction, persisted reload without rebuilding, failed opens, detach,
  remount, retained active-handle position and failed optional arena allocation.
- Metadata-preserving interior cluster relocation; rename, equal-size replacement,
  truncation, changed CID/reformat, timestamp-only and in-place-byte changes.
- Ninety write-failure points, sixteen durable-sync failure points that discard
  unsynchronized sectors, interrupted generations and damaged headers/records.
- Read-only/full media, 15 malformed serialized records, incompatible schema,
  257-file and full 512-record limits, and persisted oversized outcomes.
- 31-fragment application maps at the 64-DWORD cap; 32-fragment ordinary fallback.
- 768 paired filesystem benchmark positions with identical bytes, zero mapped
  allocation-link visits and refusal to overwrite unknown fixture files.
- Guarded audio-copy add/reuse/removal, CID/content/pin protection, unchanged
  original source, companion variation and exactly one map for one added WAV.
- Existing savefile compatibility using current sequencer/callback addresses;
  missing and truncated settings are rejected.
- 20200 acknowledged ownership handoffs, 200 rejected optional jobs, nested
  ownership, timeout and publication ordering.

## Hardware evidence

The prerequisite diagnostics gate passed before map implementation; retained in
`artifacts/seek/diagnostics-gate.json`. The complete feature has additional
layout/map/switch measurements and a documented 7 KiB total diagnostic SRAM
ceiling. The final paired witness comparison passed. Fixed diagnostic
data remains below 4 KiB. Map objects reserve 5012 ARM bytes including the
3784-byte arena; executable SRAM and allocator bookkeeping are additional.

Automatic first use built/persisted 192 maps; its next boot reused all 192 with
zero builds/writes (`maps-automatic-firstuse-441`, `maps-testindex-reuse-441`).
Three deferred cache loads including eviction/reload made no builds/writes and
resumed mapped without starvation (`maps-cache-load-441`).

Settings save/load restored the original selection and map with zero builds,
index writes or starvation (`maps-settings-lifecycle-441-retry`). It created
previously empty preset 15 and left the current slot selector at 0. The first
attempt began before the startup guard was released; the corrected harness
waits for active playback and verifies that a save actually ran.

Two guarded alternate WAV copies each caused exactly one new map. The following
boot reused all 194 records with zero writes/builds and played both alternate
physical files using their distinct correct maps. Guarded removal restored the
original 192-file manifest without rebuilding unchanged maps. See
`maps-audio-fixture-{added,pair,reused,remove3,remove2}`.

After the user powered off and reinserted the same card, the new boot reused all
192 records with zero builds, index writes or failures and retained generation 5.
Map preparation took 805152 us. The ten-second observation had zero starvation;
card identity, firmware identity and a changed boot session were verified.
Evidence: `maps-physical-reinsert/before` and `maps-physical-reinsert/after`.

The final eight-build supported comparison is retained under `maps-final-builds` and
`maps-final-matrix-*`; final observer builds are under `maps-final-witness-builds`.
Use the hardware report for measured performance, memory, starvation and startup
costs. Original 128-frame stress runs had repeated-switch overload with ordinary
seeking too; no blanket zero-underrun claim is made for every DSP setting.

## Preserved bring-up evidence

The initial complete matrix predates the mapped SRAM helper. Its small
contiguous-seek regression motivated the shift, known-cluster and SRAM changes;
`maps-matrix-*`, `maps-repeat-*`, `maps-shift-*`, `maps-reuse-*` and
`maps-ramfunc-*` retain those trials. The original 31-fragment benchmark measured
10.46x lower mean seek time before the final helper optimization.

An initial FatFs optional 32 KiB directory-clear allocation failed on this card.
The permanent fixed workspace now uses its existing sector fallback; the failed
image/stack and manually assisted first index are retained and excluded from
performance acceptance. Moving the arena allocation after reverb initialization
preserves the existing four combs and three allpasses.

The original 2 MiB device flash backup is
`artifacts/seek/original/device-flash-2MiB.bin`, SHA-256
`e16fbdaa2e04ec9b077dffb0c3ddefc28c8e2b101298034fb3a3b9e82b69176f`.
The unrelated user formatter `dev/flash_zeptocore_punk.py` was not run or changed.
