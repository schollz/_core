# `_core`

[![build workflow](https://github.com/schollz/_core/actions/workflows/build.yml/badge.svg)](https://github.com/schollz/_core/actions/workflows/build.yml) [![GitHub Release](https://img.shields.io/github/v/release/schollz/_core)](https://github.com/schollz/_core/releases/latest)


this is the monorepo for [zeptocore](https://shop.infinitedigits.co/collections/zeptocore/), [zeptoboard](https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-zeptoboard), ectocore, and [ezeptocore](https://get.ezeptocore.com) music devices, their firmware, and the tools to interact with them.

for purchasing and the complete maintained documentation, visit the [Zeptocore collection guide](https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-guide). prepare samples with the [Zeptocore tool](https://tool.zeptocore.com/). demos are available [on youtube](https://www.youtube.com/watch?v=FZ2C9VIMgeI&list=PLCNN6FnBNdpWQUyHAQO_wCQkbMl95-293).

For contributors and coding agents, start with the [repository documentation
index](docs/README.md): repository layout, build and test commands, current
firmware behavior, and the consolidated seek/audio development notes.

## dsp

The digital signal processing for all the *core things was written by Zack, from scratch, in C. This was done partially to have strict control over the sound/utility, but also because the RP2040 is fixed-point based and needed special care in all the DSP. The libraries are written with modularity in mind, so [they can be used in other programs](https://github.com/schollz/fpfx). Here are the DSP header files:

- [beat repeat](https://github.com/schollz/_core/blob/main/lib/beatrepeat.h) based on zero-crossings
- [bit crush](https://github.com/schollz/_core/blob/main/lib/bitcrush.h) with sample rate and bit rate modulation
- [comb filter](https://github.com/schollz/_core/blob/main/lib/comb.h) tuned for some cool chaotic sounds and stereo field
- [simple delay](https://github.com/schollz/_core/blob/main/lib/delay.h)
- [reverb stereo](https://github.com/schollz/_core/blob/main/lib/freeverb_fp.h) and [reverb mono](https://github.com/schollz/_core/blob/main/lib/freeverb_fp_mono.h) (stereo takes too much cpu)
- [distortion/fuzz](https://github.com/schollz/_core/blob/main/lib/fuzz.py), this is a meta code file that generates the header
- [reampling](https://github.com/schollz/_core/blob/main/lib/array_resample.h) with linear and quadratic forms
- [resonant filter](https://github.com/schollz/_core/blob/main/lib/resonantfilter.h) which has a fade-in/out
- [saturation](https://github.com/schollz/_core/blob/main/lib/saturation.h)
- [shapers](https://github.com/schollz/_core/blob/main/lib/shaper.h) for a loss-type effect
- [tape delay](https://github.com/schollz/_core/blob/main/lib/tapedelay.h) 
- [transfer](https://github.com/schollz/_core/blob/main/lib/transfer.h) which can also be used for wave shaping

## zeptocore

the zeptocore device is a versatile, open-source, handmade audio player and synthesizer, featuring stereo playback of 16-bit audio files at a 44.1 kHz sampling rate. 

<div align="center">
<img src="docs/static/img/zeptocore_noche.png" width="70%">
</div>

the zeptocore supports SD-card storage for up to 32 gigabytes of samples and can recall up to 256 audio files organized into 16 banks of 16 tracks each. the zeptocore has 16 different audio effects - saturation, fuzz, delay, comb, beat repeater, filter, tape stop, reverb + more - and includes a single-cycle wavetable synthesizer. The device offers a real-time sequencer with optional quantization, optional clock sync out, and MIDI input and [MIDI output](https://www.youtube.com/watch?v=Rl_LBdM1FQw) over USB. the device has a built-in 8-ohm speaker and can be powered by two AAA batteries or USB-C.

The firmware for the zeptocore is written in C, and instructions for building it are in the [documentation](https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-firmware).

### zeptocore firmware

| Normal | Low latency | Visualizer |
| --- | --- | --- |
| [v8.0.2 UF2](https://github.com/schollz/_core/releases/download/v8.0.2/zeptocore_v8.0.2.uf2) | [v8.0.2 UF2](https://github.com/schollz/_core/releases/download/v8.0.2/zeptocore_v8.0.2_low_latency.uf2) | [v8.0.2 UF2](https://github.com/schollz/_core/releases/download/v8.0.2/zeptocore_v8.0.2_visualizer.uf2) |

Normal suits most uses; low latency reduces available FX bandwidth. Choose visualizer firmware to use the visualizer below.

### visualizer

<a href="https://www.youtube.com/watch?v=YlEtNIeCu6k">
<img width="1018" height="601" alt="2026-09-18_09-21" src="https://github.com/user-attachments/assets/e68e0806-3978-47b1-bbff-63f823d6fed1" />
</a>


| Platform | Download |
| --- | --- |
| Linux (x86_64) | [v8.0.1 ZIP](https://github.com/schollz/_core/releases/download/v8.0.1/zeptocore-visualizer-8.0.1-Linux-x86_64-Standalone.zip) |
| macOS (Apple Silicon) | [v8.0.1 ZIP](https://github.com/schollz/_core/releases/download/v8.0.1/zeptocore-visualizer-8.0.1-macOS-arm64-Standalone.zip) |
| macOS (Intel) | [v8.0.1 ZIP](https://github.com/schollz/_core/releases/download/v8.0.1/zeptocore-visualizer-8.0.1-macOS-x86_64-Standalone.zip) |
| Windows (x64) | [v8.0.1 ZIP](https://github.com/schollz/_core/releases/download/v8.0.1/zeptocore-visualizer-8.0.1-Windows-x64-Standalone.zip) |

The [visualizer](visualizer-juce/README.md) follows playback over USB MIDI, showing waveforms, slices, and effects. Flash the visualizer UF2, then copy the **entire contents of your SD card** into a folder on your computer. In the visualizer, click **CHOOSE FOLDER** and select that folder (the one containing `bank1`, `bank2`, etc.), then connect your zeptocore over USB.


## diy

- [Product and documentation](https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-guide)
- [Schematic](https://github.com/schollz/_core/blob/main/schematics/zeptocore_v28.pdf)
- [Source code](https://github.com/schollz/_core)
- [Firmware](https://shop.infinitedigits.co/firmware/zeptocore/)
- [Instructions for uploading firmware](https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-upload)
- [Video demonstration](https://www.youtube.com/watch?v=WBvos0TkcSY)
- [Video DIY guide](https://www.youtube.com/watch?v=FH1R4RCh0vU)



## EZEPTOCORE

The EZEPTOCORE is a eurorack version of the zeptocore developed by Infinite Digits in collaboration with Maneco Labs (full attributions [here](https://infinitedigits.co/posts/eurorack-zeptocore/)).

<div align="center">
<a href="https://get.ezeptocore.com">
<img src="https://github.com/user-attachments/assets/e407fdc6-f9c1-44be-9f5e-343952043121" height="300px">
</a>
</div>


### EZEPTOCORE firmware 

The firmware is divided into two categories: *overclocking* and *non-overclocking*. 

- Choose *overclocking*  if you are using an external clock and want maximum CPU bandwidth for FX. These builds run faster but can exhibit slight clock drift if not externally synced.
- Choose *non-overclocking*  if you are using the internal clock and need extremely stable timing. These builds have slightly reduced CPU overhead but offer the highest temporal stability.

For latency, normal latency will work for most, but choose low if you encounter latency issues (note: available FX bandwidth decreases for low latency).

|                  | Normal Latency                                                                                            | Low Latency                                                                                                           |
| ---------------- | --------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- |
| Overclocking     | [v8.0.2](https://github.com/schollz/_core/releases/download/v8.0.2/ezeptocore_v8.0.2.uf2)*                | [v8.0.2](https://github.com/schollz/_core/releases/download/v8.0.2/ezeptocore_v8.0.2_low_latency.uf2)                 |
| Non-Overclocking | [v8.0.2](https://github.com/schollz/_core/releases/download/v8.0.2/ezeptocore_v8.0.2_no_overclocking.uf2) | [v8.0.2](https://github.com/schollz/_core/releases/download/v8.0.2/ezeptocore_v8.0.2_no_overclocking_low_latency.uf2) |

*default firmware

For the [visualizer](visualizer-juce/README.md), download the [v8.0.2 visualizer UF2](https://github.com/schollz/_core/releases/download/v8.0.2/ezeptocore_v8.0.2_visualizer.uf2) (normal latency, overclocked).

To build and upload the default 441-frame, overclocked firmware with USB MIDI
visualizer telemetry enabled, run `make ezeptocore-visualizer`. For ectocore
hardware, use `make ectocore-visualizer` to retain its knob mapping. These produce
`ezeptocore_visualizer.uf2` and `ectocore_visualizer.uf2`, respectively, and upload
only after the build succeeds.

To build without uploading, run `make ezeptocore ZEPTOCORE_VISUALIZER=ON` or
`make ectocore ZEPTOCORE_VISUALIZER=ON`. These opt-in builds use USB MIDI in place
of USB serial and work with the existing [visualizer](visualizer-juce/README.md).
Both devices appear as **ezeptocore** in the MIDI port list. Normal builds omit
all visualizer code and state, even after building an enabled version in the
same directory. The new commands use the default latency and clock settings.

### Sample CV mapping (ectocore and ezeptocore)

In the webtool's Settings, **Sample CV mapping** offers **Bank divisions**
(the default) and **1 V/oct**. Bank divisions spreads the samples across the
selected CV range. With 1 V/oct, 0 V selects sample 1 and each chromatic semitone
(1/12 V) advances one sample, wrapping around the bank. For an eight-sample bank,
+1 V selects sample 5. CV selects samples; it does not change their pitch or add
retriggering. Existing sample-switch timing still applies.

With **bipolar** polarity, negative notes wrap backward: -1/12 V selects the last
sample. With **unipolar** polarity, negative voltages select sample 1. While CV is
connected in 1 V/oct mode, it controls sample selection; the sample knob still
supports bank selection and its other modifier gestures. Unplug CV to resume
ordinary knob selection. Assigning Sample CV to Reset disables sample selection
by CV.

Download either the full SD-card contents or settings only, copy the settings
to the card, and restart the device. The mapping is stored in
`settings/sample_cv_mapping` as `bank` or `1voct`; copying a new download overwrites
the previous choice. Missing settings preserve the default bank divisions.
This option requires firmware with 1 V/oct sample CV support; **the published
v8.0.2 builds do not support it**. Older firmware ignores the new setting. Note detection uses the
existing nominal voltage scale, nearest-semitone rounding, and hysteresis to
reduce jitter; it does not add a CV calibration procedure.

### diy

- [Schematic](https://github.com/schollz/_core/blob/main/schematics/ezeptocore-schematic.pdf)
- [Source code](https://github.com/schollz/_core)
- [Firmware](https://github.com/schollz/_core/releases)


## ectocore (discontinued)

the ectocore is the eurorack version of the zeptocore developed by Infinite Digits in collaboration with Toadstool Tech (full attributions [here](https://infinitedigits.co/posts/eurorack-zeptocore/)).

<div align="center">
<a href="https://infinitedigits.co/docs/products/ectocore/">
<img src="docs/static/img/ectocore_2.png" height="300px">
</a>
</div>


### ectocore firmware 


The firmware is divided into two categories: *overclocking* and *non-overclocking*. 

- Choose *overclocking*  if you are using an external clock and want maximum CPU bandwidth for FX. These builds run faster but can exhibit slight clock drift if not externally synced.
- Choose *non-overclocking*  if you are using the internal clock and need extremely stable timing. These builds have slightly reduced CPU overhead but offer the highest temporal stability.

For latency, normal latency will work for most, but choose low if you encounter latency issues (note: available FX bandwidth decreases for low latency).

|                  | Normal Latency                                                                                          | Low Latency                                                                                                         |
| ---------------- | ------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- |
| Overclocking     | [v8.0.2](https://github.com/schollz/_core/releases/download/v8.0.2/ectocore_v8.0.2.uf2)*                | [v8.0.2](https://github.com/schollz/_core/releases/download/v8.0.2/ectocore_v8.0.2_low_latency.uf2)                 |
| Non-Overclocking | [v8.0.2](https://github.com/schollz/_core/releases/download/v8.0.2/ectocore_v8.0.2_no_overclocking.uf2) | [v8.0.2](https://github.com/schollz/_core/releases/download/v8.0.2/ectocore_v8.0.2_no_overclocking_low_latency.uf2) |

For the [visualizer](visualizer-juce/README.md), download the [v8.0.2 visualizer UF2](https://github.com/schollz/_core/releases/download/v8.0.2/ectocore_v8.0.2_visualizer.uf2) (normal latency, overclocked).

See [Sample CV mapping](#sample-cv-mapping-ectocore-and-ezeptocore) for the optional
1 V/oct sample-selection mode shared with ezeptocore.


### diy


- [Schematic](https://github.com/schollz/_core/blob/main/schematics/ectocore_v1.0.1.pdf)
- [Source code](https://github.com/schollz/_core)
- [Firmware](https://github.com/schollz/_core/releases)
- [Instructions for uploading samples](https://www.youtube.com/watch?v=NfjjhU1z6Ek) 


## zeptoboard

zeptoboard is the breadboard variant of the zeptocore. It has most of the same functionality, but instead of using the buttons on the handheld device, you can utilize your keyboard. This version requires some knowledge of breadboarding, but it is ideal if you want to develop your ideas based on the firmware. more information is in the [Zeptocore guide](https://shop.infinitedigits.co/collections/zeptocore/#zeptocore-zeptoboard).

<div align="center">
<img src="docs/static/img/zeptoboard_img.png" height="350px">
</div>


# license & attributions

- Apache License 2.0 for no-OS-FatFS (Copyright 2021 Carl John Kugler III)
- MIT license for the SdFat library (Copyright (c) 2011-2022 Bill Greiman)
- MIT license for the USB library (Copyright (c) 2019 Ha Thach)
- GPLv3 for all _core code
- Hardware: cc-by-sa-3.0

## guidelines for derivative works

The schematics are open-source - you are welcome to utilize them to customize the device according to your preferences. If you intend to produce boards based on my schematics, I kindly ask for your financial support to help sustain the development of future devices.
Also note - Infinite Digits and Ectocore are registered trademarks. The name "Infinite Digits" and "Ectocore" should not be used on any of the derivative works you create from these files. 
