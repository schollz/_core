# Ectocore detailed callback timing

## Result

A 90-second hardware stress capture on the ectocore at 200 MHz and 256 frames
recorded 240 file-generation changes and one DMA starvation event. The extra
output buffer and next-file preparation were both disabled. The block budget
was 5,734 us at the measured 44,642.857 Hz output rate.

The callback spanning starvation took **5,848 us**. It stayed on bank 0,
sample 10, variant 0, with no file open or close. Its active flags were
saturation, delay, filter and reverse; pitch index was 48 and stretch Q8 was
256. The source was not mapped.

| Stage | Time (us) |
| --- | ---: |
| Delay sample processing | 1,645 |
| SD read | 849 |
| SD seek | 513 |
| Filter | 757 |
| Saturation | 629 |
| Beat repeat | 329 |
| Resampling | 174 |
| Delay parameter setup | 11 |
| Other timed stages | 75 |
| Unattributed callback work | 866 |
| **Total** | **5,848** |

The event occurred 5,714 us after callback entry. This is evidence of combined
DSP and storage work exhausting the deadline, even without opening another
file in that callback. Delay parameter updates were small; the delay sample
loop is the first optimization candidate, followed by filtering. No DSP
algorithm changes were made as part of this measurement.

The longest callback was **7,554 us**, switching from sample 6 to 8 with stretch
Q8 1251. It included a 1,900 us open, 2,140 us stretch render, 1,618 us delay
loop and 792 us filter. Stretch render includes 841 us seeking and 460 us
reading; those must not be added twice. This longest callback did not coincide
with the retained starvation event.

## Apparatus and limitations

`AUDIO_DETAILED_TIMING` defaults OFF and requires `SEEK_DIAGNOSTICS`. When
enabled, fixed SRAM records retain the longest callback, the two latest
starvation-associated callbacks and one callback every 256 renders. The DMA
handler posts only an event timestamp and sequence; the audio core publishes
the completed record. Records include source/effect state and 21 timing stages.

`scripts/ectocore_profile.py` reads sequence-checked records through the existing
debug server's OpenOCD connection once per second, without halting the cores.
There is no serial printing, allocation or SD logging in the callback. The
profiler adds no output queue or intentional response delay, but timer reads,
record publication and SWD traffic can perturb execution. A separate observer
overhead gate was not run; the 114 us budget excess is an instrumented result.
The sampled records are not a complete callback latency distribution.

There were 81 changed records and no negative residual stage totals. Stack
guards remained intact; untouched stack bytes were 1,580/2,184 on cores 0/1.
Free heap after control initialization was 1,712 bytes, with the same one-comb,
one-allpass reverb configuration. Stereo Scarlett capture contained signal
without clipping, at low recorded levels. Near-zero audio spans alone cannot
distinguish intentional source silence from dropouts.

Native tests cover timer/counter wrap, stage accumulation, event attribution
inside/between callbacks, and record publication. They passed with ASan/UBSan,
alongside the existing diagnostics tests and 22 Python protocol tests. Both
detailed-ON and normal detailed-OFF firmware builds completed successfully.

## Evidence and installed firmware

Evidence directory: `artifacts/seek/ectocore-256-profile/`. Key files are
`analysis.json`, `profiles.jsonl`, `capture/summary.json`, `coverage.json`,
`audio-report.json`, `stacks/report.json`, and `native.log`.

Profile ELF SHA-256:
`5002d079a8391974a1f730fcfd0fe488457bd9f816d8ea72a1e2bd754362df97`.

After measurement, the original normal 256-frame firmware was restored from
`artifacts/seek/ectocore-256/firmware.elf`, with test controls and both latency
experiments disabled. Its SHA-256 is
`79884605a1ae3c3a6e265d00559c64a7974056bb6e8ad516025d619cf41731b4`.
Restore evidence is in `restore.log` and `restored-status.json`.
