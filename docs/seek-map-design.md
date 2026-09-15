# Shared seek-map implementation design

This is the implementation design derived from the measured diagnostics baseline.
The diagnostics acceptance gate passed. The common map/index and ownership code
is implemented. Native fault testing and supported zeptocore hardware validation
passed; see [hardware results](seek-hardware-results.md).

## Memory policy

Start with two fixed cache entries of 64 DWORDs each (31 fragments per map).
Use no per-entry allocation, compaction, or core-1 stack arrays. Pin the single live
playback handle's entry; load/evict only under acknowledged media ownership.
The initially selected WAV stays resident during preparation; prioritize its bank.
Allocate one fixed-size map arena during startup, after the existing reverb
allocator has chosen its DSP configuration. Retain that arena across media
remounts; never allocate/free it in rendering. A failed allocation leaves ordinary
seeking available. `PICO_MALLOC_PANIC=0` restores ordinary NULL-on-failure behavior
so that this optional allocation can fail safely. No reverb algorithm or parameter
is changed. The earlier static placement reduced the reverb comb count and was
replaced; compare reverb counts as well as heap values in hardware validation.

Bound the persistent manifest to 256 physical WAVs and each index generation to
512 fixed 512-byte records. Excess files stay playable with normal seeking.
Enumerate supported numeric WAV names in bank1..bank16 directories, including
all actual variations/variants, rather than guessing how many exist. Keep a
compact bounded file-ID/record-slot directory in RAM, not all map arrays.

## Allocation validation

Use an adapter in the bundled FatFs translation unit so it can call the existing
read-only get_fat implementation and reuse its sector window. Verify every
interior link, extent transition, and end-of-chain against a serialized map;
interpret contiguous exFAT objects through those same FatFs rules. Bound all
walks by file/volume geometry to reject malformed loops before CREATE_LINKMAP.
Oversized status records need a digest of the full allocation chain, not merely
filename/size/timestamps. All attached maps additionally pass structural bounds
and exact link validation. Never read or hash WAV contents to establish validity.

## Durable index

Reserve a firmware directory on the card, with two generation files. Each has a
versioned, checksummed card/volume header and fixed-size self-checking records.
Each generation has two 512-byte headers followed by at most 512 records.
The first header establishes the generation and remains untouched. The second
header commits a checksummed active-record bitmap. Each completed record is
synchronized before it is counted as checkpointed work.
Keep the last completed generation while constructing the next; on restart,
merge valid checkpoints from an interrupted newer generation after revalidation.
Scan the bounded checkpoint extent, checking each record independently; one
damaged sector does not discard subsequent intact records. Generation numbers
and record checksums prevent stale trailing bytes from being accepted.
A final synchronized header identifies the completed manifest. Completed older
records for deleted files remain only in the rollback generation, not the active
manifest. An unchanged mount validates and reads, and performs no index writes
or CREATE_LINKMAP calls.

Bound record lengths, counts, offsets, cluster totals and all arithmetic before
using disk data. Full/read-only/failing persistence disables further writes for
that preparation pass. Ordinary playback remains available. A compatible firmware
update retains the schema and validation policy, independent of firmware build ID.

Schema and validation policy are both version 1. Storage is `.core_seek/maps0.bin`
and `maps1.bin`, at most 263168 bytes each (filesystem allocation overhead is
additional). The 256-file limit is applied in initial-file/selected-bank/remaining
bank order. Excess files use ordinary seeking. An exhausted 512-slot checkpoint
generation or persistence failure stops writes for that preparation pass; unsaved
files can still use bounded RAM maps. A read-only card cannot guarantee reuse of
work that it cannot persist.

Native tests cover a 257-file card (including file ID 65535), the 256-file
manifest boundary, and a full 512-slot interrupted checkpoint. A file beyond
the manifest cap does not repeatedly queue work. A checkpoint with no free slots
retains its validated records, makes no further writes and leaves new files on
bounded RAM/ordinary fallback. An unsaved record cannot be reused after reboot.

The native build accounts for 5032 object bytes (ARM: 5012) for the map arena,
pointer, statistics and FatFs workspace. The ARM arena is 3784 bytes. It contains
one index handle, switching between generation files when necessary,
a union of the preparation handle and directory scanner, one sector buffer,
the build table, two cache entries, the compact manifest and generation headers.
Allocator bookkeeping is additional; runtime heap measurements account for it.
Recheck both stack margins and complete-feature heap usage for every buffer size.
These object totals exclude executable SRAM. The mapped FatFs seek helper adds
432–448 static SRAM bytes including alignment/veneers in the measured builds;
complete linker and runtime measurements include that cost.

FatFs uses one fixed 1120-byte name buffer plus lease state/alignment (1124 bytes).
Exclusive filesystem ownership makes this single buffer sufficient. Optional
directory-clear allocations are declined while the name buffer is leased, so
FatFs uses its existing sector-window fallback. This avoids its optional 32 KiB
allocation on cards with 64 KiB clusters. Formatting callers provide their own
work buffer. The first hardware attempt exposed the previous allocator panic;
native tests now cover this cluster size and full/read-only media explicitly.

The allocation validator rejects chains longer than the file's rounded-up size,
unfinished exFAT allocation state, cycles, bad links, and malformed tables.
Unsupported or corrupt chains remain unmapped. For contiguous exFAT files it also
checks every allocation bitmap bit; chained files use the bundled FAT-link rules.
After a disk I/O error the module invalidates acceleration and requires a remount
before ordinary playback, because a failed index write can leave FatFs's shared
metadata window dirty. Read-only/capacity errors without uncertain I/O permit
ordinary playback directly.

## Filesystem ownership

Core 1 owns filesystem access during playback. Core 0 requests ownership and
waits for acknowledgement issued at a callback boundary after all current audio
filesystem calls have returned. It performs filesystem work only after that
acknowledgement, with IRQs enabled and no long-held spinlock. Core 1 continues
bounded output servicing while ownership is withheld. Startup begins with core 0
owning media; audio must not inspect uninitialized playback state.

Centralize all playback opens/closes, attaching only current-mount validated cache
entries. Opening failure clears validity and detaches/unpins the old map. Runtime
misses queue a bounded load request and immediately use normal seeking. Optional
loads/builds wait for startup, remount, or an already stopped, acknowledged media
window; they do not create a pause or prolong a sample change.
The audio owner grants optional work only while transport is stopped, its output
is already muted, delay/reverb tails are disabled, and all bass voices are idle.
Otherwise the optional request is rejected and the map stays queued. Required
file changes use the normal acknowledged handoff regardless of optional work.

Audit settings I/O, variation changes, card-detect/recovery and transfer helpers
because FatFs reentrancy is disabled. Audio-file writes invalidate affected maps;
settings and index writes do not invalidate unrelated audio chains.

Startup loads the saved selection before choosing preparation priority and the
initial playback file. The existing savefile byte layout is retained; loading
discards serialized heap and callback addresses and uses current allocations.
Runtime settings loads retain the normal reopen and sample-change path.

## Mapped seek execution

The bundled builder remains synchronous and unchanged. The bounded mapped seek
body runs from SRAM on Pico builds. It reuses the handle's known cluster when
both old and new positions refer to the same cluster, including FatFs's
preceding-byte convention at exact boundaries. Other targets scan the immutable
extent table. Cluster-order calculation uses shifts for the mounted filesystem's
power-of-two cluster geometry, retaining full-width offsets until the final
DWORD conversion. The sector-cache and disk read/write behavior is preserved.

This reduces CPU overhead for short contiguous seeks, where a table lookup alone
had initially been slower than ordinary FatFs seeking. Fragmented seeks still
scan up to 31 extents; they avoid following the filesystem allocation chain.
Native tests compare full-width arithmetic and exact cluster/sector boundaries,
then compare returned bytes and file positions using the actual bundled FatFs.

### Boundary handoff detail

Use aligned C11 atomic loads/stores with release/acquire ordering and separate
single-writer request, acknowledgement and release generations. Do not use a
single sticky acknowledgement bit: a new request must never inherit an old ack.
At runtime the audio owner may finish one last callback using the old handle,
then acknowledge at its end, after every FIL access and metadata snapshot. This
lets core 0 complete a short reopen between callbacks without forcing a silent
block. While ownership is held across a subsequent callback, submit a bounded
intentional-silence buffer without dereferencing mutable playback state. Startup
uses an explicit boot guard so audio cannot touch uninitialized handles or DSP
objects. Core-0 acquire/release supports nesting for startup/settings helpers.

Runtime cache misses use a single coalescing request for the latest physical WAV.
The RAM directory provides a bounded lookup to distinguish validated persistent
records from genuinely missing/invalid records. Processing waits for a stopped
playback window plus the same explicit acknowledgement; stopping alone is not
ownership. Releasing ownership publishes complete cache entries and all playback
handle metadata before audio can resume filesystem work.
