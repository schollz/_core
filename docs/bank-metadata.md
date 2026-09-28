# Bank-resident sample metadata

The shared firmware now scans a small catalogue for the complete card and keeps
slice/transient arrays for one bank. Zeptocore, Ectocore and Ezeptocore use the
same implementation. Beat-repeat, delay and comb buffers are unchanged, as are
the CV detection algorithm and the phase-zero silent sample-switch behavior.

## Ownership and selection

`lib/bank_metadata.c` owns the catalogue and a single malloc-aligned arena. Each
bank has one allocation containing the sample pointer table and scalar
`SampleInfo` entries. The scan uses a fixed 512-byte read buffer and 255-entry
slice-start validation workspace. It does not allocate slice arrays. The arena
is reserved once, to the largest bank's aligned storage requirement, and reused
without allocation on bank switches.

Catalogue entries retain mutable BPM, playback mode, one-shot, tempo matching
and splice settings. Reloads publish only array pointers, so control changes
survive visits to other banks. A checksum and file length cover the original
metadata; mutable settings do not participate in the reload comparison.

Internal sample selections are ordinals over valid slots. `valid_slots` and
`filename_index` preserve physical identities independently of those ordinals.
Filenames, flash state, presets, visualizer identities and diagnostic file IDs
use physical slots. For example, files 0 and 2 produce two selectable positions,
but the second still opens and saves identity 2. No sample file is renamed or
written by this loader. Existing metadata versions 0 and 1 and the preset,
visualizer and seek-map formats remain supported.

`metadata_ready(bank)` and `metadata_generation()` describe the published bank.
Inactive banks have null detail pointers. The staging views are private and
never published until every file is validated. Array pointers must not survive
a service call or an ownership handoff. Legacy `NOSDCARD` sampleinfo tests have
a separate owned-allocation reader; that destructor is absent from firmware.

The parser checks exact read lengths, supported versions, nonzero slice counts,
play modes, usable splice divisors, signed playback-size bounds and slice
start/stop ranges. It consumes every on-disk transient entry while retaining at
most 16 per lane. Reloads check the startup checksum, file size, counts and arena
bounds. A changed or truncated file cannot publish a partial bank.

## Startup and transitions

Startup builds the catalogue, applies the existing flash/preset precedence,
reserves the arena with 16 KiB temporarily held for remaining initialization,
and loads the selected bank. Reverb is allocated after filters and device
controls, with a further 8 KiB held back during its existing memory check. Its
existing insufficient-memory fallback remains in use.

Existing selection producers share the pending bank/sample request. Same-bank
changes retain the audible and silent renderer paths. The renderer captures a
bounded selection once per block to prevent concurrent controls from changing
its target halfway through an open.

Cross-bank selection runs through `REQUESTED`, `FADING_OUT`, `LOADING`,
`COMMITTING`, then `IDLE`. The existing fade envelope is applied to the final
output, including effect tails from a silent source. After fade completion the
foreground obtains the existing audio/filesystem acknowledgement. Only then can
it invalidate pointers and reuse the arena. Each loader step performs at most
one open, close or read of at most 512 bytes. Control and USB polling continues
between steps.

The complete bank and selected playback file publish under ownership. Ordinary
transitions retain the existing relative phase calculation, including variation
scaling. Silent/exhausted selections start at zero; explicit transport, mute,
direction and effect settings are not reset. MIDI slice triggers, timers and
telemetry cannot use unpublished arrays; stretch grains and pending transient
tracking are reset for the new generation.

New requests replace superseded work at file-operation boundaries. Preset reads
and saves wait until the transition finishes. A failed transition reloads the
captured previous bank once and restores its phase/beat state. Loading and
rollback have independent two-second budgets checked between operations; SD
operations retain driver timeouts. A failed rollback stays silent and accepts a
new selection without an automatic retry loop.

## Diagnostics

The `metadata_status` SWD symbol adds resident/loading bank (16 means none),
generation, arena capacity/usage, minimum free heap, last/maximum transition
microseconds, rollback count, last error, state and rejected-file count. The
existing diagnostic mailbox ABI is unchanged. The debug server's read-only
`metadata.status` command validates the matching ELF and reads the optional
symbol twice to reject changes during retrieval. Old firmware returns
`available: false`.

Errors: 0 success, 1 IO, 2 truncated, 3 format, 4 range, 5 memory, 6 changed,
7 timeout, 8 cancelled, 9 playback-file open/close. States: 0 idle, 1 requested,
2 fading, 3 loading, 4 committing, 5 rollback, 6 error, 7 cancellation.

## Verification, 2026-09-28

`python3 test/bank_metadata/run.py` runs the real parser and foreground state
machine under ASan/UBSan. Coverage includes every truncation boundary, versions
0/1, invalid counts/ranges/divisors, short/error reads, sparse filenames, reload
mismatch, every catalogue/arena allocation failure, null inactive pointers,
mutable settings, cancellation, independent deadlines, both rollback outcomes,
requests during rollback, muted/stopped/reverse state, and variation phase.
A synthetic 16-bank/256-sample card switches repeatedly with constant allocation
count and only one resident bank. The same-bank silent-switch and interpolation
regressions also pass.

The existing FatFs/map/ownership, DSP, CV detection, MIDI and visualizer suites
pass. Diagnostics has 23 passing protocol tests. MIDI tests used `CC=clang`;
the default system compiler rejects a pre-existing `printf_sysex` test call with
a fortified null-format warning. Eight regression builds pass: Zeptocore
441/256, default Ectocore and default Ezeptocore, each with diagnostics and
visualizer both off and both on.

The correctly mapped Ectocore diagnostic build boots the connected seven-bank
card, finding all 104 samples with no metadata rejections. The final test build
selected every sample twice (208 selections, 13 cross-bank loads), verified the
physical playback filenames and checked that other banks' detail pointers were
null after every selection.

- Arena: 15,588 bytes; active-bank use varies from 9,568 to 15,588 bytes.
- Ordinary transition service duration: 135.380–166.369 ms.
- Minimum free heap: 13,608 bytes, unchanged through the sweep; observed peak
  heap allocation: 164,100 of 177,708 bytes.
- No metadata errors, rollback, FatFs open/close/read/seek errors, DMA starvation
  or notification timeouts. There were 14 normal audio short reads; these are
  recorded separately and have not been classified as audio defects.
- Stack guards intact; 1,816/2,836 untouched bytes on cores 0/1.

A preceding build also passed a 36-request rapid burst and committed the final
selection without errors or starvation. Its entire superseded transition train
lasted 1.085 seconds. Continuous new requests can extend the silent interval;
the sub-250 ms result applies to individual settled bank requests, not an
unbounded request burst. Timings measure firmware request-to-commit service;
this run did not include an analog recording or a listening assessment.

The sample CV input was detected connected during testing. The acceptance
harness uses reserved test-only serial controls 116/117 to choose exact
bank/ordinal values, temporarily overriding SAMPLE knob/CV selection for one
second while continuing detection. This override is absent from the final
normal image. Reproduce with `scripts/bank_metadata_check.py --help` on a
`SEEK_DIAGNOSTICS=ON, SEEK_TEST_CONTROLS=ON` Ectocore build.

Artifacts are retained under `artifacts/bank-metadata/`: `original/` contains
the matching prior ELF/UF2 and a byte-verified 2 MiB flash backup;
`ectocore-final-test/` contains the complete final sweep and stack evidence;
`ectocore-test/rapid.json` retains the rapid-request run; `builds/` contains the
eight build logs. `ectocore-final/` holds the final normal diagnostic firmware,
with test controls disabled, its programming log and post-startup status.
The installed normal ELF SHA-256 is
`0c94ca3d565f904c1469b014de395c71bda763200865e5f1472c315af112d65f`.
Its post-startup minimum free heap is 14,200 bytes, with no errors or starvation.
The matching on-demand debug service uses `/tmp/bank-metadata.sock`.

No commits were created by this implementation.
