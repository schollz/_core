# Next-file preparation experiment

## Outcome

Implemented and tested an optional `AUDIO_PREPARE_NEXT` prototype on the ectocore
at 256 frames / 200 MHz. **It did not demonstrate fewer dropouts.** The option
defaults to OFF and the previously tested normal 256-frame firmware was restored.

| Same 90-second control sequence | Baseline | Preparation enabled |
|---|---:|---:|
| DMA starvation events | 1 | 1 |
| File-generation changes | 241 | 184 |
| Maximum observed callback, us | 7,759 | 5,929 |
| Mean callback, us | 2,087 | 2,084 |

The prototype improved map availability but delayed/coalesced selections, so
the reduced callback maximum cannot be treated as an equivalent-load speedup.
Random effects also differ between runs. Both starvation events occurred during
the combined switching/effects phase. The 256-frame budget is 5,734 us.

## Implementation

The audio core retains exclusive filesystem ownership. After queuing the
current audio block, a render that used less than half its nominal time budget
may perform one preparation step: open the requested file, then load its
already validated persisted map into the second cache slot. No map construction
or index writing is allowed in this path. The old sample keeps playing until
the candidate is ready, then the existing fade/switch adopts its file handle.

On the fade-out block, remaining headroom may be used to warm the candidate's
512-byte FatFs sector near the predicted next phase. This is a best-effort sector
prefetch, **not an entire rendered audio block**; scratching, negative-latency
offsets, phase changes and stretch may use a different source position. Ordinary
FatFs reads always remain the source of truth.

Superseded candidates detach their map and close their file. Ordinary lifecycle
close/reopen paths also cancel preparation. Native tests cover byte equality,
untouched current-handle state, map pin transfer, cancellation, missing-file
fallback and zero writes. The new map-loading API only reads existing records.

The work is synchronous and SD latency is not bounded by the available time
estimate. Preparation can defer a requested switch under sustained DSP load.
It therefore does not provide the independent SD worker and source-audio queue
that would be needed for fully asynchronous preparation.

The optional build uses additional RAM and disables foreground map-layout
snapshots, because the cache writer is now the audio core. Other diagnostics
remain available. The normal build retains its existing layout snapshots.

## Evidence

`artifacts/seek/ectocore-256-prefetch/` contains the firmware, control log,
90-second diagnostic/audio capture, phase coverage, preparation counters, native
and protocol test logs, stack inspection and restore log. The experimental ELF
SHA-256 is
`cd8acdfd02acb55a482cd89960c8579535c91bc59da3a3e6dfdd455863354e51`.

- Eight samples and up to five active effect flags were observed; both playback
  directions and Q8 stretch values 256/1251 were exercised.
- After the run: 171 successful preparations opened, 168 adopted, 144 sector
  warms. Not every file-generation change is one of these prepared switches.
- Mapped snapshots within the four phases were 81/81, 67/80, 77/80 and 77/81,
  versus baseline 11/80, 0/81, 8/80 and 13/81.
- No map builds or index writes occurred during capture. Normal reads reported
  nine short reads with no FatFs error results; those short reads were not
  individually classified.
- Stack guards were intact: 1,596/2,152 untouched bytes on cores 0/1.
- FatFs tests (including a second run with preparation enabled) and diagnostic
  protocol tests passed. The default-off ectocore build was also checked.

The restored normal ELF is
`artifacts/seek/ectocore-256/firmware.elf`, SHA-256
`79884605a1ae3c3a6e265d00559c64a7974056bb6e8ad516025d619cf41731b4`.
