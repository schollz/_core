# Ectocore clock-input restart latency

This is the pre-fix measurement. The subsequent
[restart correction and immediate wake](ectocore-clock-restart-fix.md) fixes the
source advance/manual mute behavior and reduces measured restart latency.

## Result

On the optimized ectocore firmware at 200 MHz / 256 frames, a simulated CV
clock after clock-loss muting reached the resumed audio block's DMA output in
**5.866–11.393 ms**, with an **8.909 ms median** across 24 restarts. The actual
sample rate was 44,642.857 Hz, giving a 5.734 ms block duration. No starvation
occurred in these measurements. A preceding 23-record pass measured
5.910–11.386 ms; the phase of the incoming edge relative to the audio block
changes the observed delay and the sample median.

These are firmware/DMA timings. They exclude the electrical input stage, NVIC
interrupt entry, remaining PIO FIFO/serializer time and DAC conversion/filter
delay. The simulated edge calls the same `gpio_callback(GPIO_CLOCK_IN,
GPIO_IRQ_EDGE_FALL)` used by the inverted CV jack signal. Its timestamp is
taken just before dispatch, not when the USB command is sent. The simulation
runs from the foreground, so its interrupt-preemption conditions also differ
from a physical GPIO interrupt.

| Interval | Median (ms) | Observed range (ms) |
| --- | ---: | ---: |
| Simulated edge to beat-position update | 0.056 | 0.053–0.064 |
| Position update to audio applying it | 3.139 | 0.094–5.620 |
| Applying position to submitting rendered block | 3.631 | 3.441–4.161 |
| Submitting block to its DMA start | 2.084 | 1.550–2.275 |
| **Edge to resumed block DMA start** | **8.909** | **5.866–11.393** |

Individual interval medians need not add to the median total. This workload
used bank 0/sample 0, forward playback, no stretch, and saturation enabled.
The first restart used the initial tempo; regular simulated pulses then
established approximately 120 BPM. It is not a worst-case heavy-effects test.

## Beat position is not the same as audible onset

Every measured restart was muted before the edge, but restarted with a
crossfade between the previous and new source heads. The first nonzero PCM
frame was frame zero, which may belong to the old head. It must not be
interpreted as the first new-beat sample or transient. The crossfade lasts
one 256-frame block, approximately 5.734 ms.

The new head was also advanced by about **16.03–16.05 ms of equivalent output
playback time**. The cause is visible in `lib/audio_callback.h`:

1. `clock_handling_start()` sets `playback_restarted` through `timer_step()`.
2. The renderer clears `audio_was_muted` on that restart, so the later
   mute-recovery fade-in does not suppress the phase-change crossfade.
3. The head loop reads the old head (1) before the new head (0).
4. The first seek consumes `clock_input_present_first` and disables latency
   compensation for that old head. The new head then receives compensation.

The captured new-head offsets were 2,867 bytes initially and usually 2,027
bytes afterward, aligned down to four bytes by the seek. With stereo PCM and
256/181 source frames per output block respectively, those offsets correspond
to about 2.8 output blocks. The default factor is 5.6, but it multiplies a
count of 16-bit values and is added to a byte position, producing roughly
half that many blocks of advance.

Consequently, a drum transient can be altered or skipped at restart. This
cannot be described solely as a fixed output delay. A targeted follow-up would
preserve first-clock handling until the new head is read and explicitly define
whether clock restart should fade from silence or crossfade the previous head.
No such playback change was made during this investigation.

## Manual stop

Two separate simulated manual stops followed by a CV edge restarted the
transport but did not resume audio: `button_mute` remained set. The clock
handler updated beat zero, but no tagged audio block was rendered before the
measurement timed out. A subsequent state read confirmed:

```
playback_stopped = 0
button_mute = 1
audio_callback_in_mute = 1
phase_change = 1
```

The manual stop uses the same `trigger_button_mute` / `do_stop_playback`
requests as the A+C gesture. The clock-start handler does not clear
`button_mute`, unlike the tap-button start path. This is different from
clock-loss muting, where the internal transport can remain running.

## Apparatus and evidence

`SEEK_CLOCK_LATENCY` defaults OFF and requires diagnostics and test controls.
It adds eight sequence-checked records, source/phase/submission timestamps,
and tags propagated through the existing buffer-copy and DMA trace path.
`scripts/ectocore_clock_latency.py` reads those records through the existing
debug server without halting either core. PCM onset is reported only when it
falls within the traced 256-frame consumer block, and still does not identify
which crossfade head produced that sample.

The test-only serial command is `T,113,mode`: mode 1 performs 24 clock-loss
restarts, mode 2 requests a manual stop then sends one clock edge, and mode 0
disables the generator. Clock-loss trials send three pulses 250 ms apart,
then wait slightly over 1.2 seconds. The command sender must keep the CDC
connection open long enough for the foreground parser to consume it. Test
settings are temporary and normal firmware is restored after measurement.

Native tests cover stage attribution, PCM frame detection, buffer offsets,
stale tags and timeout publication. They passed with ASan/UBSan alongside the
existing diagnostics tests and 22 protocol tests. Instrumented and default-OFF
firmware builds passed. Both post-test stack guards were intact. Instrumentation
has execution overhead; no separate paired observer-overhead gate was run.

Final evidence: `artifacts/seek/ectocore-clock-latency-v2/`, including
`summary.json`, `clock-loss.jsonl`, `capture/`, `manual/`, stereo audio,
native/build logs and stack checks. Test ELF SHA-256:
`e6e40a82c90516e0985a2f718ce26e2f34f791ffa87194e0caef329bfbafc0d0`.

The normal optimized firmware from `artifacts/seek/ectocore-256-dsp-normal/`
was restored afterward, with all test controls and latency experiments off.
Its ELF SHA-256 is
`0ff010329b39cdc16f4d12f20ebc66ce17ed6860e5250dd87e03478b42536385`.
