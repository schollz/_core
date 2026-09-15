# Delay and filter loop optimization

The pre-optimization checkpoint was committed and pushed to
`zack/fast-seeking` as `9cc7cac` before these changes.

## Changes

The active delay implementation is **`lib/tapedelay.h`**, included by
`lib/includes.h`. Earlier discussion of the old `lib/delay.h` implementation
was incorrect; the stage timings themselves measured the active tape delay.

- Tape delay replaces its normal per-sample ring-buffer modulo operations
  with bounds checks, retains a modulo fallback for other representable read
  indices, and keeps its index and smoothed delay in local variables. Its
  floating-point smoothing and interpolation expressions remain unchanged.
- Filtering chooses steady-state, fade-in or fade-out processing once per
  block, with local coefficients and history. Each original fixed-point
  product is still rounded separately and added in the original order.
- Both loops use `dsp_multiply_q16`, an exact decomposition of the signed
  64-bit Q16 product into three unsigned 32-bit products. Unsigned intermediate
  arithmetic defines the intended modulo-2^32 result. The generated filter
  assembly uses hardware `muls` instructions instead of calling the 64-bit
  multiply helper.

There is no additional output buffer or intentional latency. The implementation
uses 816 more bytes of static SRAM in the detailed ectocore build, mainly for
specialized filter code. Profiling/test firmware retained 896 heap bytes after
controls initialized, with the same one-comb/one-allpass reverb configuration.

## Validation

`test/dsp_loops/run.py` compares output samples and complete state against
frozen process functions from the checkpoint. It passes at `-O0` and `-O2`
with ASan/UBSan:

- Two million randomized multiply inputs plus signed boundary cases.
- 4,000 delay blocks, including activation, fractional duration changes,
  feedback changes, wrap positions and both channels; additional tiny-ring
  and full-scale clipping cases.
- 12,000 filter blocks, including bypass, transitions, coefficient changes,
  both channels and buffer sizes from zero through 441 frames.

The host oracle explicitly uses signed wraparound for existing DSP sums and
disables floating-point contraction. This validates the tested numerical
behavior, not arbitrary invalid parameters or concurrent mutation of DSP state.
Ectocore detailed and normal 256-frame builds, ezeptocore 256-frame and
zeptocore 256-frame builds all pass. Only ectocore was exercised on hardware.

## Hardware timing

At 200 MHz, 256 frames and a 5,734 us callback budget, the first final-build
90-second stress run completed 240 file-generation changes with one starvation
event. It used the same serial switching/effect workload as the baseline.

| Active stage, sampled median | Baseline (us) | Optimized (us) |
| --- | ---: | ---: |
| Filter | 765.5 (14 samples) | 251.5 (18 samples) |
| Tape delay | 1,473 (10 samples) | 1,396 (14 samples) |

These are changed periodic profile records with stage time above 100 us, not
every callback. Workload randomness and profiling overhead limit exact paired
comparisons. Filter improvement is substantial; the measured delay improvement
is smaller. Neither result establishes that all dropouts are fixed.

The remaining starvation callback took 5,985 us while switching samples 6 to 8.
Its costs included open 1,842 us, seek/read 955 us, delay 1,443 us, filter
246 us and comb 447 us. The longest callback was 7,044 us. The original baseline
also had one starvation event, with a 7,554 us longest callback.

An initial candidate that placed tape delay in SRAM was slower and is not
retained. Its capture is `artifacts/seek/ectocore-256-dsp/`. Final-build evidence
is `artifacts/seek/ectocore-256-dsp-v2/`, including the ELF/source hashes,
reference tests, assembly, timing comparison, SWD capture and stereo recording.

The consecutive repeat captured another 242 file changes with **zero**
starvation events. Both captures were coherent and built/wrote no seek maps.
Thus the final build recorded one starvation in 180 seconds and 482 file
changes. Post-capture stack guards were intact, with 1,580/2,184 untouched
bytes on cores 0/1. This is evidence for the exercised workload, not a guarantee
against other SD-card stalls or untested effect combinations.

## Installed firmware

The optimized normal firmware is retained in
`artifacts/seek/ectocore-256-dsp-normal/`, with detailed profiling, test controls,
next-file preparation and the extra output buffer all disabled. SWD diagnostics
remain enabled. ELF SHA-256:
`0ff010329b39cdc16f4d12f20ebc66ce17ed6860e5250dd87e03478b42536385`.

This supersedes the original firmware restoration recorded in the preceding
timing report. Flash verification and post-install status are retained with
the normal firmware artifacts.

Post-install diagnostics confirmed stage 3 playback, 256 frames, 224 reused
maps, zero starvation events at the check, and 2,248 free heap bytes after
controls initialized. Both analog stress recordings contained stereo signal
without clipped samples; their low recorded levels limit listening-based
conclusions about short gaps.
