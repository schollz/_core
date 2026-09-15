# Maintaining an extra output buffer

## Result

On the connected ectocore at 256 frames / 200 MHz, maintaining an additional
queued output block passed two consecutive 90-second stress captures with
**zero DMA starvation events and 479 file-generation changes**. The preceding
baseline had one event in 90 seconds with 241 changes. The command sequence was
the same; randomized effects differ, and these tests are not proof of zero
dropouts under every workload.

The option was initially left enabled with test controls disabled. The user
subsequently rejected the added continuous latency, so the normal 256-frame
firmware with the extra buffer disabled was restored and verified. Its ELF is
`artifacts/seek/ectocore-256/firmware.elf`, SHA-256
`79884605a1ae3c3a6e265d00559c64a7974056bb6e8ad516025d619cf41731b4`.
Restore evidence is in `artifacts/seek/ectocore-256-latency-restored/`. The CMake
option `AUDIO_EXTRA_OUTPUT_BUFFER` remains OFF by default, so release builds do
not silently change latency or RAM consumption on other configurations. The
next-file preparation experiment remains disabled.

## Change and tradeoffs

- Allocate four producer buffers instead of three. At 256 stereo int16 frames,
  the added buffer costs 1,024 sample bytes plus 48 bytes of allocation metadata.
- Prime two blocks instead of one before enabling DMA.
- After a render, count the prepared queue under its existing short spinlock.
  If fewer than two blocks remain, attempt one extra render. This is bounded to
  one additional render per notification, with no SD work under the queue lock.
- This maintains one block of reserve beyond the ordinary next block. It adds
  approximately 5.73 ms of control-to-output delay at the measured 44,642.857 Hz
  output rate. This latency increment is derived from buffering, not a measured
  analog trigger-to-output comparison.

The startup-only attempt still produced one starvation event. It did not
maintain the reserve after it drained. The final implementation recorded two
refill attempts over boot and both stress runs.

The test build had 1,656 free heap bytes after controls initialized, versus
2,824 in the baseline (1,168 bytes less including code/bookkeeping effects).
The normal build with test controls disabled retains 1,896 free heap bytes.
The existing one-comb/one-allpass reverb allocation is unchanged.

## Validation

Both runs confirmed eight sample selections, forward/reverse playback and
Q8 stretch values 256/1251, with up to four/five active effect flags respectively.
All four phases (switching, effects, combined, stretch/reverse) had zero
starvation. File-generation deltas were 239 and 240. There were no map builds or
index writes during either capture.

The longest observed callback wrapper was 9,303 us. With refill enabled this can
include two renders, so it is not directly comparable to one producer-block
budget. Playback still maintained DMA continuity. Normal reads had six/twenty
short reads in the two captures, with zero FatFs error results; individual short
reads were not classified. Both analog captures were retained for inspection.

After both runs, stack guards remained intact: core 0 retained 1,580 untouched
bytes and core 1 retained 2,184. The diagnostic protocol/conversion tests passed.
Both test and normal firmware builds compiled and programming verified. The
normal firmware booted into active playback with 224 reused maps and zero
starvation at the final check.

## Artifacts and installed firmware

- Startup-only attempt: `artifacts/seek/ectocore-256-buffer/`.
- Maintained queue, first run: `artifacts/seek/ectocore-256-buffer-refill/`.
- Repeat without reboot: `artifacts/seek/ectocore-256-buffer-repeat/`.
- Installed normal firmware: `artifacts/seek/ectocore-256-buffer-normal/`.

Test ELF SHA-256:
`822d01fef2b2152babc235359a11a1f687eaa64c33ea69fac0a28d13d30c4094`.

Installed ELF SHA-256:
`3842362f0f4db38d0db92eb56d881e5deaa87b2413c05d7799a0372b4af314b4`.
This is the retained extra-buffer ELF, not the currently installed firmware.
