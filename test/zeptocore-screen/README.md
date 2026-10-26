# zeptocore-screen

First display bring-up firmware for a Zeptocore with an Adafruit 4694 Sharp
400 x 240 memory LCD. This standalone test replaces the audio firmware while
running; it does not access the SD card, knobs, audio hardware, or PIO.

| Zeptocore | Wire | Display |
| --- | --- | --- |
| GPIO26 | Blue | CLK |
| GPIO27 | Purple | DI |
| GPIO28 | Gray | CS |
| 3.3 V | | VIN |
| GND | | GND |

The knobs must be disconnected from GPIO26-28.

Build from the repository root with `make zeptocore-screen-test`. This creates
`zeptocore-screen-test.uf2` and `build-zeptocore-screen/zeptocore-screen.elf`, using
the repository's Pico SDK in a separate build directory.

Flash through the Raspberry Pi Debug Probe:

```sh
openocd -f interface/cmsis-dap.cfg -f target/rp2040.cfg \
  -c 'adapter speed 1000' \
  -c 'program build-zeptocore-screen/zeptocore-screen.elf verify reset exit'
```

Expected display: a border, ZEPTOCORE / SCREEN TEST / HELLO WORLD!, the pin
assignments, a checkerboard, a seconds counter, and a blinking square. The
onboard LED also blinks. USB serial identifies as `zeptocore-screen` and prints
seconds and completed frame transfers. These counters confirm execution;
visual inspection is required because the LCD has no readback.

The reusable driver is `lib/sharp_memory.c`. It uses SPI1 at 1 MHz, active-high
software CS, byte reversal for LSB-first transmission, and 12,000 framebuffer
bytes. The test refreshes every 500 ms to keep alternating VCOM.

For full audio playback and SD-card waveforms, use `make zeptocore-screen`.
See `docs/zeptocore-screen.md` for the integrated firmware.
