# `_core`

[![build workflow](https://github.com/schollz/_core/actions/workflows/build.yml/badge.svg)](https://github.com/schollz/_core/actions/workflows/build.yml) [![GitHub Release](https://img.shields.io/github/v/release/schollz/_core)](https://github.com/schollz/_core/releases/latest)


this is the monorepo for [zeptocore](https://zeptocore.com), [zeptoboard](https://zeptocore.com/#zeptoboard), and [ectocore](https://getectocore.com) music devices, their documentation, their firmware, and the tools to interact with them.

for information about purchasing and documentation, visit [zeptocore.com](https://zeptocore.com). demos are available [on youtube](https://www.youtube.com/watch?v=FZ2C9VIMgeI&list=PLCNN6FnBNdpWQUyHAQO_wCQkbMl95-293).

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

the zeptocore supports SD-card storage for up to 32 gigabytes of samples and can recall up to 256 audio files organized into 16 banks of 16 tracks each. the zeptocore has 16 different audio effects - saturation, fuzz, delay, comb, beat repeater, filter, tape stop, reverb + more - and includes a single-cycle wavetable synthesizer. The device offers a real-time sequencer with optional quantization, optional clock sync out, and MIDI (in and out) over USB. the device has a built-in 8-ohm speaker and can be powered by two AAA batteries or USB-C.

The firmware for the zeptocore is written in C, and instructions for building the firmware are in the [documentation](https://zeptocore.com/#firmware-development).

### zeptocore diy


## diy

- [Website](https://zeptocore.com/)
- [Schematic](https://zeptocore.com/#is-a-schematic-available)
- [Source code](https://github.com/schollz/_core)
- [Firmware](https://zeptocore.com/#uploading-firmware)
- [Instructions for uploading firmware](https://zeptocore.com/#instructions) 
- [Video demonstration](https://www.youtube.com/watch?v=WBvos0TkcSY)
- [Video DIY guide](https://www.youtube.com/watch?v=FH1R4RCh0vU)


## zeptoboard

the zeptoboard is the breadboard version of the zeptocore. 

<div align="center">
<img src="docs/static/img/zeptoboard_img.png" height="350px">
</div>

It retains most of the same functionality but allows you to use your keyboard and a MIDI interface (via [this website](https://zeptocore.com/#computer-keyboard)) instead of the buttons on the handheld device. While this version does require some breadboarding knowledge, it is perfect for developing your ideas based on the firmware.

## ectocore 

the ectocore is the eurorack version of the zeptocore. 

<div align="center">
<img src="docs/static/img/ectocore.png" height="300px">
</div>

this is currently under development and will be released soon. more info at https://getectocore.com/


## sample loading tool

the sample loading tool is a web-based tool (available at [zeptocore.com/tool](https://zeptocore.com/tool) or [ectocore.rocks](https://ectocore.rocks)) that allows you to load samples onto the *_core* devices. it is built with Go, and Vue.

<div align="center">
<img src="docs/static/img/core_tool.webp" width="70%">
</div>

the source code is located in the [core folder](https://github.com/schollz/_core/tree/main/core), and information for building the tool is located can be found in [the documentation](https://zeptocore.com/#building-from-source).

### ectocore.rocks offline usage

The ectocore.rocks sample loading tool can be used offline by following the instructions for your system.

<div align="center">
<img src="docs/static/img/ectocore_screenshot.png" width="70%">
</div>

<details><summary>Windows</summary>

#### Download for Windows: **[x64](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_windows_v6.3.9.exe)**

Once downloaded, double click on the executable file to run it.

</details>


<details><summary>macOS</summary>

To install the tool on macOS, first open a terminal.

Then, if you are on an Intel-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_macos_amd64_v6.3.9 > core_macos
```

Or, if you are on a M1/M2-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_macos_aarch64_v6.3.9 > core_macos
```

Then to enable the program by entering this into the terminal:

```
chmod +x core_macos 
xattrc -c core_macos
```

Now to run, you can just type

```
./core_macos
```

A window should pop up in the browser with the offline version of the tool.

</details>


<details><summary>Linux</summary>

#### Download for Linux: **[x64](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_linux_amd64_v6.3.9)**

After downloading, run it directly from the terminal.

</details>


## faq


### Why is it freezing?

Normally, ectocore and zeptocore should not freeze. 

If you encounter a freeze, it would be helpful to know the following:

> What SD card are you using? If you are using a custom SD card, please try using the stock SD (or [approved SD card](https://github.com/schollz/_core?tab=readme-ov-file#which-sd-card-can-i-use)) card to see if the issue persists. If it does, please answer the following questions.

> How many samples did you load? Even though all 16 banks can be used, the ectocore currently only supports about 150 samples due to recent firmware changes. If you exceed 
> this limit, the device may freeze on start.

> Were effects on or off during the freeze? Try turning off all effects (Break knob fully CCW) and see if the issue persists.

> Are you using custom samples or the stock samples? Try using the [stock samples](https://infinitedigits.co/zeptocore_default_samples_v6.zip) and see if the issue persists.

> Is it a particular sample that causes a freeze or any sample? If a particular sample, please provide the original sample if you can.

> What was the BPM set to?

If it happens a lot, please try using [this firmware](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_no_overclocking_v6.3.9.uf2) which disables overclocking, allows fewer fx, but should increase the stability.

Please submit an issue with responses to these questions by [clicking here](https://github.com/schollz/_core/issues/new?template=ectocore-freezing.md) or send an email to zack@infinitedigits.co.

If, however, you want to return the device, I completely understand. Please reach out to the seller for a return.

### Why is the ectocore not playing in time?

The ectocore is very good at putting out what you put into it, and it is *incredibly* stable if you need it to be. In many cases, users that experience timing issues are due 
to their samples not being in time (e.g. if you have a sound sample that is 8 beats at 120 bpm it should be 8*60/120 = 4 seconds long) or due to the sample not being sliced well (slices should ideally go right before transients).

The stability of the ectocore varies based on whether you use the internal clock, with or without oversampling, or externally clocked.

In order of stability:

1) Internally clock Ectocore using the non-overclocking firmware (found [here](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_no_overclocking_v6.3.9.uf2)). This is more stable than my external clock source (Pam's new workout).
2) Externally clock Ectocore using a clock source. Even when Ectocore is overclocked, it follows the clock source very well.
3) Internally clock Ectocore using the overclocking firmware (the default). This is about two times less stable than Pam's clock source.

You can visualize this ordering in the data I collected below, where Ectocore was clocked internally (with or without overclocking) or externally (with Pam's clock source). 

![Clocking](/dev/clocking.png)

1) Pam's, by itself, without Ectocore, starts to slowly drag (Red line).
2) Ectocore, clocked by Pam's, will stay very close to Pam's (which means it will slowly drag) (Blue line).
3) Ectocore, with the normal firmware (overclocked), internally clocked, will start to get ahead of the tempo, worse than Pam's but not terribly bad (Green line).
4) Ectocore, with the non-overclocked firmware, internally clocked, will be rock steady, more than Pam's or anything else (Purple line). Rock steady means deviating by only 0.5 ms after almost 20 minutes.

If you are still having issues, please submit an issue  by [clicking here](https://github.com/schollz/_core/issues/new?template=ectocore-freezing.md) or send an email to zack@infinitedigits.co.

### Why is the ectocore lagging?

Ectocore is highly responsive to start/stop triggers. It seamlessly starts when receiving an external clock signal (e.g., from Pamela's New Workout) or when triggered manually from a stopped position using the Tap button. Earlier versions of Ectocore had issues with latency, but these have been resolved in version 6.3.7 and later. 

Here’s how latency (or lag) is measured for testing purposes: Two outputs from Pamela's New Workout are set to 150 BPM at 2x speed. One output clocks Ectocore, whose signal is then sent to the first channel of an output module. The second output from Pam’s is routed to the second channel of the same module. Latency is determined by measuring the difference between the two channels at the peak of Pamela’s signal. For example, here is a case where the measured latency is 4.9 ms:

![Latency](/dev/latency/how.png)

By collecting multiple measurements, I can generate a latency distribution:

![Latency](/dev/latency/20250315.png)

If you are still experiencing lag, it is likely due to sample splicing rather than actual system latency. Ectocore is designed for maximum responsiveness, but if a sample’s splice points are placed ahead of the transient, it may create the perception of lag. If you need assistance optimizing your sample splicing, feel free to email me with a link to your workspace, and I’ll be happy to take a look.

### Which firmware should I use?

That depends on your needs. The firmware is divided into two categories: overclocking and non-overclocking. The non-overclocking firmwares have the most temporal stability but reduced cpu overhead. The overclocking firmwares have the most CPU bandwidth for FX but has a slight drift in the clock *if it is not being externally synced*.

- Overclocking firmware (great if you are externally clocking): [v6.3.9](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_v6.3.9.uf2)
- Non-overclocking firmware (great if you are using the internal clock and need extremely steady timing): [v6.3.9](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_v6.3.9_no_overclocking.uf2)

If you are still experiencing latency issues, you can try the low latency version of the firmware. Keep in mind the lower latency firmwares might have less bandwidth for fx:

|                  | Normal Latency                                                                                          | Low Latency                                                                                                         | Ultra-Low Latency                                                                                                        |
| ---------------- | ------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| Overclocking     | [v6.3.9](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_v6.3.9.uf2)                 | [v6.3.9](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_v6.3.9_low_latency.uf2)                 | [v6.3.9](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_v6.3.9_ultralow_latency.uf2)                 |
| Non-Overclocking | [v6.3.9](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_v6.3.9_no_overclocking.uf2) | [v6.3.9](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_v6.3.9_no_overclocking_low_latency.uf2) | [v6.3.9](https://github.com/schollz/_core/releases/download/v6.3.9/ectocore_v6.3.9_no_overclocking_ultralow_latency.uf2) |


### How do I update the ectocore?

The latest firmware version can be downloaded from [any of the available firmwares](https://github.com/schollz/_core?tab=readme-ov-file#which-firmware-should-i-use). After downloading the firmware, follow these instructions ([click here for a video version](https://www.youtube.com/watch?v=9Ql57oJBMQM)):

1. **Power Connection:** Ensure your ectocore is connected to case power, not just USB power, to avoid potential damage.
2. **Access the USB-C Port:** Remove the ectocore from your rack or create space to access the USB-C port.
3. **Connect to Computer:** Plug a USB-C cable from the ectocore to your computer.
4. **Enter Bootloader Mode:**
   - Locate the two buttons on the back: "_Boot_" (right) and "_Reset_" (left).
   - Hold the "_Boot_" button and tap the "_Reset_" button.
   - The module lights will indicate success: all lights on and buttons are unresponsive. A drive named "RPI-RP2" will appear on your computer.
5. **Transfer Firmware:**
   - Copy the firmware file (ending in .uf2, around 3 MB) to the "RPI-RP2" drive.
   - The module will reset automatically once the file is copied.
6. **Reinstall:** Carefully unplug the USB-C cable and reinstall the ectocore into your case.


### Which SD Card can I use?

Please note that not all SD cards are equal. Terms like "high-speed," "A1," or "U3" on the card do not necessarily indicate its actual speed performance.

Below is a list of known good and bad cards:

#### Known bad cards

Do *not* use these cards! They may appear to work, but they can cause spurious glitches.

- Lexar brand
- Kootion brand
- Kingston brand
- 5% of JUANWE cards

#### Known good cards

- Gigastone 16GB + 32GB
- SP Elite
- SanDisk Extreme
- Samsung EVO
- PNY Elite
- MicroCenter

# license

- Apache License 2.0 for no-OS-FatFS (Copyright 2021 Carl John Kugler III)
- MIT license for the SdFat library (Copyright (c) 2011-2022 Bill Greiman)
- MIT license for the USB library (Copyright (c) 2019 Ha Thach)
- GPLv3 for all _core code
- Hardware: cc-by-sa-3.0

# guidelines for derivative works

The schematics are open-source - you are welcome to utilize them to customize the device according to your preferences. If you intend to produce boards based on my schematics, I kindly ask for your financial support to help sustain the development of future devices.
Also note - Infinite Digits and Ectocore are registered trademarks. The name "Infinite Digits" and "Ectocore" should not be used on any of the derivative works you create from these files. 
