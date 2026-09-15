# Plan: zeptocore firmware-generated FatFs fast-seek maps

## Status

Planning only. No implementation, firmware builds, or hardware changes are part of this step. All work below remains to be done.

## Connected hardware

- The zeptocore is connected via USB-C.
- It is also connected to a Raspberry Pi Debug Probe through the debug pins only; the probe's UART pins are not connected.
- Use this setup for future firmware validation. Do not assume UART logging is available through the Debug Probe.

## Objective

Generate FatFs cluster link maps on zeptocore and reuse them for audio-file seeking. Reduce allocation-chain traversal during reverse playback, slice jumps, and real-time stretching while preserving audio output, sample-switch behavior, and compatibility with existing SD cards.

Zeptocore is the sole implementation and validation target for this plan. Put the implementation in the common firmware path so ezeptocore and ectocore inherit the behavior automatically. Use one shared map/index implementation and file lifecycle; do not gate the feature on `INCLUDE_ZEPTOCORE` or create separate device implementations, tuning work, or validation milestones.

The firmware will derive maps from the mounted card. `core_server`, ZIP exports, WAV files, and `.info` formats will remain compatible without changes.

**Build once per relevant card change:** automatically prepare and persist maps when a card is first used or its audio files/allocation layout change. Reuse completed maps across file selections, RAM-cache eviction, card reinsertion, and power cycles while they remain valid. An unchanged boot may validate and load maps, but must not rebuild them.

## Current behavior

- `lib/sdio/include/ffconf.h` already sets `FF_USE_FASTSEEK = 1`.
- The application does not initialize `FIL.cltbl` or call `f_lseek(..., CREATE_LINKMAP)`.
- The bundled FatFs implementation clears `cltbl` on every successful `f_open()`. Enabling the compile option alone does not activate fast seeking.
- Normal backward seeks across cluster boundaries can traverse the allocation chain from the beginning of the file.
- Normal playback and real-time stretching share `fil_current`. Both cores currently have paths that open or reopen that handle.
- Audio DMA is initialized before `sdcard_startup()`. Startup must not be assumed to mean that the audio core is inactive.

A map describes a file's filesystem cluster allocations, not slice offsets inside a WAV. Its validity depends on the mounted volume and the file's allocation chain. It speeds up locating file data; it does not eliminate SD reads or increase raw card throughput.

## Scope

Implement map creation, persistent storage on the SD card, change detection, bounded RAM caching, attachment after file opens, invalidation, safe ownership, fallback, and targeted measurements for zeptocore through the shared firmware modules.

Keep this change separate from resampler optimization, delay DSP, scratch phase wrapping, source-data caching, asynchronous SD streaming, and the DMA notification redesign. Those are independent performance projects.

Store the persistent map index in a reserved firmware-managed location on the SD card. Do not write maps to RP2040 program flash or require the desktop tool to generate them. Cards without an index must continue to work and acquire one automatically when writable.

Treat relevant changes as a different card/volume, added or replaced audio files, or changed audio-file allocation chains. Remove records for deleted files. Changes to unrelated settings and the map index itself must not repeatedly invalidate audio maps. If audio bytes change in place but their allocation remains identical, a validated map is still usable.

## Design

### 1. Use the existing FatFs map builder

- Add a small shared firmware module, provisionally `lib/audio_seek_map.h`, following the repository's existing organization.
- Prepare maps using a dedicated read-only `FIL`, not by changing the position or state of the live playback handle.
- Supply a suitably aligned `DWORD` work table, put its capacity in `table[0]`, assign it to the preparation handle's `cltbl`, and call `f_lseek(..., CREATE_LINKMAP)`.
- Publish a map only after successful completion. Persist a portable record of its DWORD entries and copy it into stable RAM storage before attaching it to a handle. Never persist a raw `FIL` or pointer.
- On insufficient table capacity, discard the partial result and clear the preparation handle's `cltbl`. The FatFs call reports the required size, but this must not trigger unbounded allocation or repeated attempts during playback.
- A map requires two bookkeeping DWORDs plus two DWORDs per contiguous fragment. Size budgets using fragmentation, not WAV duration alone. Map construction still walks the allocation chain even when the final map is small.
- Do not modify the bundled FatFs implementation to build maps incrementally in the first version.

### 2. Bound RAM and preparation work

- Establish a compile-time cap on cache entries, map bytes, and temporary preparation storage. Select the actual limits after checking the supported zeptocore builds' RAM and heap headroom; do not assume the available heap is spare. Keep capacity handling and ordinary-seek fallback in the shared implementation.
- Account for directory metadata, the temporary `FIL` and its sector buffer, map-building workspace, alignment, and peak stack use in addition to the retained maps.
- Keep map arrays off core 1's stack. Use fixed storage or a bounded arena reserved outside audio rendering; do not allocate or free map memory from the audio callback.
- Prioritize the initially selected physical WAV, then the selected bank, then the remaining supported audio files, including variations and audio variants. Prepare the persistent index one file at a time; do not keep every map in RAM. Enumerate actual supported files rather than assuming a fixed number of variants.
- Pin entries attached to an open handle. Evict only unpinned entries, and never move an attached map during arena compaction.
- RAM-cache capacity is optional acceleration: an uncached file must remain playable through ordinary FatFs seeking. Eviction discards only the RAM copy; a later load of the persisted record must not call `CREATE_LINKMAP` again.
- If a file exceeds the per-map capacity, persist that outcome with its validated identity and the cache-policy version. Do not repeat the same failed build on every selection or boot; reconsider it when the file changes or the supported capacity increases.
- Bound persistent index size and record count as well as RAM use. Define a documented fallback for cards that exceed supported limits.

### 3. Separate preparation from audio rendering

Use separate paths with different timing requirements:

| Path | Allowed work |
| --- | --- |
| Playback open/reopen | Open the requested file, look up a completed map, validate its identity, and attach it. On a miss, use normal seeking immediately. |
| Map load | Under exclusive filesystem ownership, load an already validated persistent record into the RAM cache without rebuilding it. |
| Media validation/preparation | Check the card/index, reuse valid records, build missing or invalid maps, and persist completed records under exclusive filesystem ownership. |

- Never call `CREATE_LINKMAP` from `i2s_callback_func()`, `timer_step()`, or a DMA/GPIO interrupt.
- At startup or a detected remount, validate the persistent index before enabling its use. Build only missing or invalid records during coordinated media preparation. Separate first-use/changed-card preparation time from the unchanged-card startup budget.
- Treat any startup time budget as a limit checked between files: a single FatFs map-building call is synchronous and cannot be interrupted safely just because a budget expires.
- Runtime RAM misses should queue a map-load request, not automatically a rebuild. Only genuinely missing or invalid persistent records queue map construction. Both types of request must wait for an explicitly quiescent media window. Do not build maps merely because core 0 appears idle or some audio buffers are queued.
- Do not introduce a mute, delayed sample switch, or background filesystem contention solely to build an optional map. A miss can remain unmapped until a suitable preparation window occurs.
- `playback_stopped`, `audio_callback_in_mute`, and `sync_using_sdcard` individually do not prove exclusive filesystem ownership. Define and verify the necessary acknowledgment before starting preparation.
- Complete the changed-card preparation pass before normal playback where necessary, with a documented indication using existing device UI. Deduplicate repeated change notifications into one job for that validated card state. Interrupted work may resume; it must not discard and rebuild already completed, still-valid records.
- Hook startup, explicit remount/recovery, supported card-detect events, and firmware-side audio-file mutations through the common media lifecycle. Zeptocore disables card-detect GPIO support, so do not promise immediate physical hot-swap detection without a supported event or a mount/I/O recovery path.

### 4. Make handle and cache ownership explicit

- Centralize audio-file open/close and map attach/detach in shared helpers. Route every `fil_current` open through them.
- Define which core owns `fil_current` at each stage. Use a short command/acknowledgment handoff or another appropriate synchronization mechanism for ownership changes.
- Do not hold a spinlock or disable interrupts while traversing the FAT or waiting for SD I/O. Synchronization should protect ownership/publication, not encompass the blocking operation itself.
- Map lookup on the playback path must have bounded cost and must not wait for a builder.
- Publish immutable completed entries with the required memory ordering. Detach and unpin the old map only once the old handle is no longer in use.
- A failed open must leave no attached map or published valid playback handle. A map-building failure must not change the active playback handle's error state or file position.

### 5. Detect changes and validate persistent maps

Identify persistent records using:

- Card identity from the driver's existing CID data, plus volume/partition identity and filesystem geometry. CID alone does not detect a reformat or changed files.
- The canonical full audio filename, including bank, sample, variation, and audio variant.
- File size, starting cluster, relevant directory metadata, and the validated allocation layout of the successfully opened file.
- A persistent schema/validation-policy version and record integrity checksum.

Give in-memory handles and validation results a separate mount-generation identifier. On unmount/remount, detach old RAM references and revalidate persistent records; do not delete or rebuild those records merely because the mount generation changed.

On attachment, require a matching record validated for the current mount and copy its table into pinned RAM. Reattach after every successful open because FatFs resets `cltbl`.

**Do not substitute a cheap metadata fingerprint for allocation validation.** A computer can relocate file clusters without changing the filename, size, or timestamps. There is no application-level change marker in the existing ZIP workflow that proves the card was untouched.

- On each fresh mount, enumerate the supported audio-file manifest and validate cached allocation layouts against the mounted filesystem before using them.
- Prefer batched reads of the relevant allocation metadata. For FAT-chain files, verify the links represented by each saved extent, including interior links, transitions between extents, and the end of the chain. Checking only the first cluster or extent endpoints is insufficient.
- Respect the bundled FatFs implementation's FAT/exFAT allocation rules, including contiguous exFAT files. Use a narrowly scoped read-only validator or adapter with tests rather than duplicating undocumented assumptions. Unsupported validation cases fall back to fresh construction or normal seeking.
- This validation reads filesystem metadata and has a startup cost. Measure it separately from map construction and map loading; do not describe unchanged boots as having no card scan or no I/O. Avoid hashing WAV contents, which does not establish allocation-map validity.
- Reuse records whose allocation validates. Build new or changed records once, discard deleted records, and retain unchanged records. A card-wide identity/format mismatch invalidates the whole index.
- Invalidate affected entries before firmware operations that replace, truncate, or alter audio-file allocation. Unrelated file writes should not invalidate unchanged audio chains.
- Exclude the index's own files from the audio manifest/change detector. Creating or extending the index must not cause an endless rebuild cycle.
- Treat uncertain media state after an I/O failure conservatively. Cache reuse assumes exclusive access to the mounted card; another host must not modify it concurrently.

### 6. Persist completed work safely

- Use a reserved, versioned index format containing card/volume identity, per-file identity, table length, table data, status, and integrity checks. Stream records through bounded buffers.
- Validate serialized lengths, cluster bounds, terminators, and checksums before a stored table can become an attached `cltbl`.
- Use a recoverable commit scheme, such as two generation slots with completion markers and checksums. Do not assume FAT rename alone guarantees atomicity or power-loss durability. Keep the previous completed generation until the new one is fully synchronized and validated.
- Checkpoint completed records so an interrupted preparation pass can resume missing work after revalidation. Power loss may require retrying unfinished work; "once" means no unnecessary rebuilding of successfully saved, valid records.
- Perform all index reads/writes outside audio rendering with explicit filesystem ownership. Cache loading and persistence must not contend with live audio reads.
- A full/read-only/failing card must still play with ordinary seeks or a bounded RAM-only map. Report the persistence limitation through deferred diagnostics; never retry a failing index write every audio block.
- On a writable unchanged card, do not rewrite the index on every boot. Only commit actual record/status/schema changes.
- Firmware updates that retain the same map/validation format must reuse valid records; incompatible format changes may require one new preparation pass.

## Integration inventory

Audit the following paths and route them through the shared lifecycle helpers:

| Location | Responsibility |
| --- | --- |
| `lib/globals.h` | `fil_current`, filename, and shared ownership state. |
| `lib/includes.h` | Include the new module after its required types and declarations. |
| `lib/sdcard_startup.h` | Card/index validation, mount generation, initial open, savefile reload/reopen, and one preparation job per changed card state. |
| `lib/audio_callback.h` | Normal and stretch sample changes; attach cached maps without building them. |
| `lib/realtime_stretch.h` | Continue using the mapped playback handle for grain seeks; preserve existing positions and loop wrapping. |
| `lib/zeptocore.h` | Variation changes and reopen handling. |
| `lib/button_handler.h` | Audio-file reopen after saving settings. |
| `lib/sdcard.h` | Unmount/remount RAM detachment and persistent-index revalidation hooks. |
| `lib/sdio/sd_driver/sd_card.h` and bundled FatFs | Existing CID/volume information and a narrowly scoped read-only allocation validator if required. |
| New map/index module | Persistent records, restart recovery, change reconciliation, map-load requests, and bounded RAM storage. |

Search again for direct `f_open`, `f_close`, mount/unmount, and audio-file mutation calls during implementation. This inventory is a starting point, not a substitute for checking the final call graph. Where device wrappers call the shared lifecycle, keep them routed through that single implementation so inherited behavior does not depend on a later port. Any necessary caller migration is shared integration, not a separate device project.

## Implementation sequence

- [ ] **Baseline and limits:** record seek/read/open times, startup time, audio underruns, and RAM/stack headroom for representative zeptocore builds. Confirm the installed FatFs API, card identity, allocation-validation approach, and documented cache/index limits.
- [ ] **Map module:** implement construction, immutable bounded storage, lookup, pinning, invalidation, and deterministic fallback. Add focused lifecycle tests.
- [ ] **Persistent index and change detection:** implement versioned records, allocation validation, reconciliation, recoverable commits, and resume behavior. Prove that an unchanged card requires zero map-building calls across boots.
- [ ] **Ownership and file helpers:** establish safe media preparation and centralize all playback-handle opens/closes. Preserve current error handling and sample-change semantics.
- [ ] **Startup/changed-card preparation:** load the index, reuse validated records, and prepare missing/changed maps once in priority order. Keep audio services safe while file ownership is withheld.
- [ ] **Playback integration:** attach valid cached maps after every successful open, including save/load, variation, and audio-variant changes. Leave cache misses on normal seeking.
- [ ] **Deferred map loads/preparation:** distinguish RAM misses from missing persistent records, and process requests only in verified quiescent media windows. Deduplicate requests and persist oversized-file outcomes.
- [ ] **Instrumentation and regression checks:** expose RAM/persistent hits, validation time, actual build counts/time, index writes, capacity failures, retained/peak bytes, and invalidations through deferred diagnostics.
- [ ] **Zeptocore hardware validation:** compare mapped and ordinary seeking using the same files, access sequences, clocks, and buffer sizes. Document results and chosen shared defaults.

## Validation

### Functional correctness

- Compare bytes and resulting file positions for the same seek/read sequence with and without maps, using the bundled FatFs implementation and a test FAT image where practical.
- Cover contiguous files, fragmented files, fragment and cluster boundaries, WAV header/preroll offsets, end-of-file, and loop wrap.
- Exercise forward playback, reverse playback, random slices, and alternating time-stretch grain reads.
- Exercise every reopen path: sample/bank switches, both file variations, audio variants, settings save/load, and repeated selection of a cached file.
- Verify that two different files with the same size cannot share the wrong map, and that remounting detaches old handles while allowing reuse of revalidated persistent records.
- Boot and reinsert an unchanged writable card repeatedly: after its initial successful preparation, assert zero `CREATE_LINKMAP` calls and zero unnecessary index rewrites.
- Evict and reload a RAM entry and repeatedly reopen its file: assert that the persistent map is loaded rather than rebuilt.
- Add, remove, replace, and rename audio files; alter allocation chains while preserving filename/size/timestamps; reformat a card; and swap cards with matching filenames. Check that exactly the necessary records are regenerated or removed and that stale maps are never used.
- Change only settings or the map index and verify that this does not trigger rebuilding unchanged audio maps.
- Interrupt index writes and preparation at multiple points, then reboot. Recover completed validated records and retry only unfinished/invalid work. Test corrupted/truncated records, full media, read-only media, and incompatible schema versions.
- Test too-small tables, exhausted cache space, pinned-entry eviction attempts, missing files, and map-build I/O failures. No partial or stale map may be attached.
- Confirm that map preparation never alters the active handle's position, phase, or grain state.
- Test ownership handoff during sample changes and remount recovery. Confirm no builder runs concurrently with playback filesystem access.

### Performance and memory

- Measure seek time separately from read/open and DSP time. Record median, high percentile, and maximum values, plus actual DMA underruns and sample-switch latency.
- Use full-width microsecond counters; do not rely on the existing utilization ring until its index and percentage-overflow issues are corrected or bypassed by dedicated measurements.
- Test long files as well as heavily fragmented files. Fast seeking still scans map fragments, so avoid claiming constant-time behavior for every file layout.
- Measure first-use, changed-card, and unchanged-card startup separately, including allocation validation, index loading, map construction, and commits. Verify that runtime RAM misses do not add map-building delays to audio callbacks.
- Inspect the linker map and heap/stack high-water marks. Treat existing high-pitch stack failures as separate faults, not evidence that fast seeking is slower.
- Build and validate zeptocore at its supported 441, 256, and 128 frame buffer sizes and supported clock configurations. Do not add a separate build, hardware-test, or tuning matrix for other devices to this plan.
- Run hardware checks with simultaneous controls/clock/MIDI activity where supported. Use the actual I2S/DMA cadence when assessing deadlines.
- Do not change SD clock speed or checksum behavior as part of these comparisons.

## Acceptance criteria

- [ ] Zeptocore's implementation uses the common map/index and audio-file lifecycle, so the other firmware devices inherit it without a separate implementation or later port.
- [ ] Existing cards and `core_server` exports work without user conversion or desktop-generated files; firmware manages its own optional persistent index.
- [ ] Maps are prepared automatically for a new or changed card state and saved for reuse. Successfully persisted unchanged records are not rebuilt on reboot, reinsertion, selection, or RAM-cache eviction.
- [ ] Relevant allocation changes are detected before reuse, including changes that preserve ordinary file metadata. Validation cost and supported filesystem rules are documented.
- [ ] Index writes do not cause self-invalidation. Interrupted writes, read-only/full cards, and invalid records have a tested playback fallback.
- [ ] Supported audio opens attach the correct completed map when one is cached; every other open has an explicit ordinary-seek fallback.
- [ ] Map construction, eviction, and allocation never run in audio rendering or an interrupt.
- [ ] No handle can retain a pointer to a partial, invalidated, moved, or evicted map.
- [ ] RAM use is bounded and documented for the supported zeptocore builds, including preparation peak usage.
- [ ] Mapped and ordinary reads return identical data for the tested access sequences; sample selection, fades, pitch, and stretch behavior remain unchanged.
- [ ] The mapped workload avoids allocation-chain traversal during seeks and shows measured improvement on representative reverse/jump/stretch cases, without a material playback or switching regression.
- [ ] First-use/changed/unchanged startup costs, persistent-index coverage, RAM-cache limits, fragmentation limits, fallback behavior, and hardware measurements are documented before the feature is considered complete.

## Reference

[FatFs `f_lseek` and fast-seek setup](https://elm-chan.org/fsw/ff/doc/lseek.html). Verify implementation details against the repository's bundled `lib/sdio/ff15/source/ff.c`, particularly `f_open`, `f_lseek`, and `clmt_clust`.
