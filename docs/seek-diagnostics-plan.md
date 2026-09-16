# Plan: on-demand zeptocore seek diagnostics

## Status and purpose

Implementation authorized. This document specifies the debugging apparatus
required before the fast-seek implementation in the [implementation brief](seek-implementation-brief.md). Track
measured results and outstanding work in [implementation status](seek-implementation-status.md).
The acceptance checkboxes below remain unproven until backed by that evidence.

The apparatus must answer:

- How much time do seeks, reads, opens, and complete audio callbacks consume?
- Which playback modes and files produce the long tail, errors, or starvation?
- Do seek maps improve the same workload without changing audio correctness?
- Are persistent maps reused, and what are their startup and memory costs?
- Does collecting or retrieving diagnostics itself affect playback?

## Architecture and transport decision

Use an on-demand service, following ouroboros's separation of measurement from
status retrieval. For this hardware, implement the first transport over SWD:

```text
CLI / capture client
    -> local Python server (versioned JSON requests)
    -> persistent OpenOCD Tcl connection
    -> Raspberry Pi Debug Probe / SWD
    -> fixed request mailbox and published snapshots in RP2040 SRAM

Audio callback / DMA interrupt / media lifecycle
    -> small, independently owned measurements in SRAM
```

The server provides named operations and human-readable output. Firmware handles
small numeric requests and fixed binary structures; it does not parse JSON or
format reports. There is one probe connection and one serialized request stream.

This uses the connected debug pins. Probe UART is unwired; zeptocore's current
USB configuration is MIDI with CDC disabled. USB CDC and MIDI SysEx transports
can use the same snapshot schema later, but are not prerequisites or deliverables
for this apparatus. USB descriptors and MIDI behavior stay compatible.

The session has established that a short SRAM read through OpenOCD succeeds with
both RP2040 cores reporting `running` before and after. That is transport
feasibility, not proof of zero timing impact. Validate repeated reads and mailbox
writes during playback before using them for performance conclusions.

OpenOCD supports memory inspection while a compatible target runs, and exposes a
Tcl RPC interface for host tools. Use that interface directly so the collector
does not need a GDB attachment. See the [background-memory-access documentation](https://openocd.org/doc-release/html/GDB-and-OpenOCD.html#Using-GDB-as-a-non_002dintrusive-memory-inspector)
and [Tcl scripting API](https://www.openocd.org/doc/html/Tcl-Scripting-API.html).

### Reference behavior to retain and improve

In `/home/zns/Documents/ouroboros-eurorack`:

- `tools/rp2350_peripheral_status.py` sends requests with schema, version, ID,
  and command; its live view requests `peripherals.status` every 250 ms.
- `firmware/rp2350/src/main_realtime.cpp` handles commands outside audio work
  and builds status from cached observations.
- Daisy records audio callback timing separately and relays compact statistics.
- RP2350's `WriteManagerSerialBytes` loops until transmission completes and
  sleeps when capacity is exhausted. Do not copy this waiting behavior into
  zeptocore's diagnostic service.

These are design references only; the implementation must not depend on another
checkout or import its board-specific service.

## 1. Measurement layer

Add a common compile-time diagnostic option independent of device selection.
Use static storage and inline hooks; retain the normal optimization settings.
An instrumentation-disabled build must compile out hooks and diagnostic RAM.
Do not enable existing `PRINT_SDCARD_TIMING` or `PRINT_AUDIOBLOCKDROPS`: they
also inject random jumps/retriggers. Do not rely on the current utilization ring,
whose index handling and 8-bit percentages can misrepresent overloads.

| Measurement | Collection point | Required data |
| --- | --- | --- |
| Seek | Immediately around each playback `f_lseek`, normal and stretch | Call count, duration histogram, maximum, result code, old/requested/resulting byte position, direction, mapped/unmapped state |
| Read | Immediately around each playback `f_read`, normal and stretch | Count, duration histogram, maximum, requested/returned bytes, errors, short reads |
| File lifecycle | Close/open/map-attach helpers and remaining direct callers during migration | Separate close/open/attach durations and errors, file generation, switch request-to-first-output timing |
| Audio render | Callback entry through output-buffer publication, all return paths | Count, duration histogram, maximum, rendered frames, no-producer-buffer returns, intentional mute and error-silence counts |
| Audio output | DMA consumer buffer selection and transfer start | DMA count and interval, missing-buffer silence count and frames, playing buffer generation |
| Core notification | Existing DMA-to-core-1 notification | Cumulative timeout count, maximum wait, existing consecutive-failure value separately |
| Startup/media | Explicit lifecycle milestones | Initial/changed/unchanged startup, validation, construction, index loading and commit times, completion/failure state |
| Maps, once implemented | Cache/index lifecycle | RAM/persistent hits and misses, attachment/fallback reason, build count/time, index writes, invalidations, capacity failures, generation and validated identity |
| Memory | Build artifacts and bounded runtime checkpoints | Static/retained/peak diagnostic and map bytes, heap availability, both stacks' measured headroom or explicit unavailable status |

Keep normal and stretch I/O in separate aggregates. Count skipped seeks separately
from executed ones. Store bounded detail for the worst seek and latest error;
do not store filenames or a record for every operation. Use numeric file/mount
generations and retrieve filename metadata outside audio rendering.

Compute seek destinations before starting the seek timer. Stop timing immediately
after the call, before error handling, and record failures before early returns
or reboot requests. Audit all seek/read/open sites again during implementation.
Observing an operation must not issue another filesystem operation. Resulting
file position can be obtained from the existing `FIL` state.

Use unsigned 32-bit microsecond subtraction for individual short operations;
document timer wrap. Use 64-bit accumulated durations or explicit saturation
where required. Counters must saturate with a flag or have documented modular
delta handling; never silently wrap percentages. Snapshot publication must make
multiword values coherent. Compute percentages and percentile estimates on the
host. Fixed histogram bins must report percentile ranges and overflow counts,
not claim exact p99 from coarse buckets.

Distinguish callback budget overruns from actual missing-buffer silence. Exclude
startup priming and deliberate mute from steady-state starvation totals. The
existing DMA FIFO push can wait 10 ms; measure it as a possible contributor while
keeping its redesign outside this task. A producer-pool miss alone is not an
output underrun.

Measure sample-switch latency in stages: request to first rendered new-file
buffer, then request to DMA consumption of that buffer. Use side metadata keyed
by the existing buffer identity; do not equate render completion with audibility.
An external recording remains the check for actual audible behavior.

## 2. Ownership, snapshots, and firmware requests

Use separate statistics for core-1 audio work, core-0 foreground work, and the
DMA interrupt. Each has one writer. Do not add a shared increment that can race
between cores, and do not make either core spin waiting for the other.

Define a versioned, explicitly laid-out little-endian ABI with aligned 32-bit
fields, fixed offsets, size limits, static assertions, and no native pointers in
payloads. Retain its exported descriptor symbol in the ELF. Include:

- Magic, ABI version, descriptor size, feature flags, firmware build identity,
  session identity, and independently readable progress counters.
- One host-owned request slot: request sequence, command, page, and bounded
  arguments. Write payload first and commit the sequence last.
- Firmware-owned response slots: status, matching request sequence, source
  timestamp/block counter, payload length, and publication generation.
- States for pending, complete, busy, unsupported, malformed, and unavailable.

For a snapshot request, each writer copies its aggregates into its own fixed
response storage at a safe boundary and acknowledges completion. Core 1 can
publish at callback completion or another point in its existing loop when no
buffer is rendered. Initialization must expose readiness so a host can distinguish
startup, no audio progress, and an unsupported command. Core-0 servicing must also
run during bounded gaps in startup preparation; a long synchronous filesystem
call may delay replies and must not be interrupted to satisfy a timeout.

Use explicit compiler/hardware ordering appropriate to RP2040; `volatile` alone
is not a synchronization protocol. Publish completion only after payload writes.
Freeze each completed response until the next accepted request, so large payloads
can be read in pages without being overwritten. The host verifies generation and
request identity before and after reads and retries within a finite budget.
Counters from different writers have separate timestamps; do not claim a globally
simultaneous snapshot. Interrupt-owned counters need a reviewed bounded capture
method; no retries or locks may block an interrupt or the audio producer.

Service at most one request at a time, with fixed work per pass and no queue that
grows with host traffic. The audio path does not wait for host acknowledgement or
core-0 progress. A disconnected host may leave a frozen response indefinitely;
normal audio statistics continue updating. Reconnection can read/recover that
response and issue a new sequence after checking the session identity.

Do not reset live counters from SWD. A capture window uses host-side differences
between snapshots from the same session; lifetime maxima and window histogram
estimates must be labeled separately. Measurement enable/disable requests, if
needed for overhead tests, are handled by the owning core at a boundary and
reported with an acknowledged measurement generation.

### Initial resource budgets

- Fixed live/mailbox data ceiling: **4 KiB**, including live aggregates, frozen
  responses, request state, and buffer metadata. Initial measured total SRAM
  increment including executable hooks/veneers is 5112 bytes for 441 frames;
  the prerequisite passed a **6 KiB** total budget. Final map/layout/switch
  diagnostics reserve 6616–6632 bytes, so the complete-feature ceiling is
  **7 KiB**, including executable hooks/veneers. The full matrix retains the
  same reverb allocation. The final 441-frame mapped helper leaves 1176 free
  heap bytes after controls (1608 before that additional executable SRAM).
- Use a small fixed histogram (initially 16 bins per selected operation class).
  Define bin boundaries around the 128-frame deadline as well as long outliers.
- No full event ring in the first version. If later needed, separately budget
  it, make it drop on overflow, and report dropped records.
- Target audio publication at no more than **10 microseconds**. The final
  foreground elapsed-time target is **50 microseconds**: normal observations
  were 5–10 us, with isolated tails up to 39 us. Foreground publication remains
  preemptible and uses bounded RAM copies; it never waits for audio. Its elapsed
  measurement includes any interrupt preemption. Report these tails separately
  from audio/IRQ publication and the measured callback overhead.
- Start host polling at 1 Hz and bounded memory-read pages of at most 256 bytes;
  validate 4 Hz before making it selectable for normal captures.

These are implementation targets, not established headroom. Inspect linker maps,
both core stacks, and peak runtime allocation for the supported 441/256-frame builds before
fixing the layout. Runtime stack watermark scans happen only in a verified safe
context; do not fill or scan an actively used stack as part of a status request.

## 3. Host server and user interface

Implement a local Python service owning OpenOCD and a CLI client. Bind the client
API to a local Unix socket; use newline-delimited JSON with a bounded message
size. Bind OpenOCD to loopback. Keep the existing probe connection open between
requests and enforce one outstanding firmware request across all clients.

Proposed commands, all read-only with respect to musical and filesystem state:

| Command | Response |
| --- | --- |
| `device.info` | Firmware/session identity, ABI/features, actual clock and buffer configuration, readiness |
| `seek.status` | Normal/stretch I/O aggregates, histogram boundaries, worst-seek context and error counters |
| `audio.status` | Callback statistics, DMA starvation, notification failures, progress and switching stages |
| `maps.status` | Map/index coverage, actual build counts, persistent reuse, fallback and invalidation reasons; unavailable before implementation |
| `memory.status` | Known static/runtime memory measurements with provenance and unavailable fields |
| `capture.start` / `capture.stop` | Host capture window IDs and artifact paths; no playback change or firmware counter reset |

Example client request:

```json
{"schema":"zeptocore.debug-command","version":1,"id":"seek-001","command":"seek.status"}
```

Responses echo ID and command, return status, session identity, source timestamp,
and data. Unsupported features are explicit rather than zero-filled. Host-level
timeouts report the last known progress and preserve the target's current state.

Resolve descriptor addresses from the exact build ELF, and validate the device's
descriptor/build identity before writing the request mailbox. A stale ELF must
fail closed. On reconnect/reboot, discard previous-session deltas and repeat
discovery. Restrict memory writes to the validated request structure; no arbitrary
Tcl, memory-write, flash, reset, halt, or playback-control API in this service.

At connection and during captures, verify both cores' running states. Never
automatically halt, reset, resume, or flash to recover a failed status request.
Keep programming and deliberate stopped-target debugging as separate workflows.

Provide these proposed entry points (names are not implemented commands yet):

```sh
python3 scripts/zeptocore_debug_server.py --elf build/_core.elf
python3 scripts/zeptocore_debug.py status --once
python3 scripts/zeptocore_debug.py watch --interval 1
python3 scripts/zeptocore_debug.py capture --duration 60 --out artifacts/seek-baseline
```

Write artifacts on the computer: a manifest, raw JSONL snapshots, summary JSON,
and CSV tables. Record firmware/ELF hash, source revision and dirty state, probe
and OpenOCD versions, ABI, actual clocks, buffer size, card identity/geometry,
file identity/layout, workload/seed, effects/pitch/stretch settings, map mode,
startup class, polling rate, capture gaps, saturation, and errors. Record unknown
values explicitly. A text view should make stale data and device resets visible.

## 4. Implementation inventory

| Proposed location | Responsibility |
| --- | --- |
| `lib/seek_diagnostics.h` and, if needed, `.c` | Common hooks, aggregates, ABI and fixed mailbox/snapshots |
| `lib/audio_callback.h`, `lib/realtime_stretch.h` | I/O/render timing and publication at safe boundaries |
| `lib/my_pico_audio_i2s/audio_i2s.c` | Actual starvation, DMA timing, notification and buffer-generation measurements |
| `lib/zeptocore.h` and shared initialization/control helpers | Bounded foreground request servicing |
| Audio-file lifecycle, startup, future map module | Open/switch/media/map measurements |
| `CMakeLists.txt` and I2S subproject build definitions | Consistent diagnostic option across translation units, retained descriptor and memory accounting |
| `scripts/zeptocore_debug_server.py`, `scripts/zeptocore_debug.py` | OpenOCD ownership, discovery, request/response service, CLI/capture |
| `test/seek_diagnostics/` | ABI, aggregation, publication and host protocol tests |

## 5. Validation and delivery sequence

1. **Transport and budgets:** record current clocks, memory/stack constraints,
   and the exact running build where identifiable. Specify/test the ABI and
   resource budget; build a minimal mailbox and identity/progress service.
2. **Baseline instrumentation:** add all normal/stretch I/O and audio-output
   hooks, including errors/early returns. Implement the host service, one-shot
   status, and capture artifacts before implementing seek maps.
3. **Apparatus verification:** test malformed/unknown commands, partial writes,
   torn reads, duplicate IDs, counter/timer wrap, saturation, stale ELF, reboots,
   absent/stalled core progress, concurrent clients, and disconnect mid-response.
   Use host simulations for interleavings and hardware stress for actual SWD
   ordering. Verify fixed memory use and bounded publication/service work.
4. **Observer overhead:** compare hooks compiled out, hooks collecting without
   retrieval, and collection with 1 Hz/4 Hz polling. Use matched workloads and
   repeated runs. Compiled-out measurements use an independent, test-only callback witness
   outside all apparatus hooks, enabled identically in both paired builds, plus
   the Scarlett audio recording. The witness uses a fixed RAM seqlock read by
   the host, with no requests, formatting, or filesystem access. Account for its
   own fixed timestamp/bookkeeping cost. Do not claim zero overhead from
   statistics collected only by the enabled implementation.
5. **Ordinary-seek baseline:** capture forward, reverse, deterministic slice
   jumps, stretching, loop wraps, sample/variation switches, and controls/MIDI
   activity. Use short/long and contiguous/fragmented files. Mark manual runs
   exploratory; exact A/B comparisons need a fixed seed/schedule and the same
   actual operation sequence. Keep test playback controls separate from the
   read-only diagnostic API and route any later harness through normal controls.
6. **Map integration:** add map lifecycle counters and compare mapped versus
   ordinary seeking with identical card/files, access sequence, clocks, buffer
   size, and diagnostic settings. Attach/detach optional maps only through the
   future safe file lifecycle, never by host pointer writes. If runtime selection
   is not safe, use paired builds with recorded feature settings.
7. **Persistence and regression:** capture first-use, changed-card and unchanged
   boots; prove zero rebuild calls on unchanged boots and persistent reload after
   eviction. Combine performance artifacts with the separate byte/position
   correctness tests and audio captures required by the implementation brief.

For all phases, distinguish measured callback overruns, DMA starvation, intentional
silence, and audible discontinuities. A successful mailbox read alone establishes
none of these. Preserve unknown/unavailable measurements in reports.

### Acceptance gate before seek-map work

- [x] All playback seek/read paths and actual missing-buffer silence are measured.
- [x] Requests retrieve completed measurements without initiating SD I/O or
  modifying playback. Host loss/backpressure cannot block either audio or IRQ work.
- [x] Coherent snapshots, identity checking, bounded resources and failure
  behavior pass focused tests and a hardware capture.
- [x] Both cores remain running throughout retrieval; no hidden attach/reset/
  halt operations appear in the host command log.
- [x] Static/peak memory and incremental stack use fit all three buffer builds.
- [x] Repeated matched runs show no added starvation attributable to collection
  or retrieval. Initial overhead target: less than 1% of the actual block period
  added to callback time, including publication; report tail/max as well as mean.
  If this target fails, reduce/split collection work and revalidate before using
  the apparatus to claim map improvements.
- [x] A reproducible ordinary-seek baseline and documented host commands exist;
  all absent measurements and external-capture limitations are stated.

Apparatus overhead results, resource budgets and baseline artifacts become inputs
to the implementation brief's map-cache limits and hardware acceptance, rather than assumptions.

Gate evidence: `artifacts/seek/diagnostics-gate.json`, `diagnostic-memory.json`,
paired retained ELFs/audio/captures, and the documented baseline matrix. The mean
overhead target passed; lifetime extremes are reported without claiming a paired
per-call upper bound. Remaining lifecycle measurements are listed explicitly in
[diagnostic usage](seek-diagnostics.md).
