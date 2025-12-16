# `_core`

[![build workflow](https://github.com/schollz/_core/actions/workflows/build.yml/badge.svg)](https://github.com/schollz/_core/actions/workflows/build.yml) [![GitHub Release](https://img.shields.io/github/v/release/schollz/_core)](https://github.com/schollz/_core/releases/latest)


this is the monorepo for [zeptocore](https://zeptocore.com), [zeptoboard](https://zeptocore.com/#zeptoboard), ectocore, and [ezeptocore](https://get.ezeptocore.com) music devices, their documentation, their firmware, and the tools to interact with them.

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


## diy

- [Website](https://zeptocore.com/)
- [Schematic](https://github.com/schollz/_core/blob/main/schematics/zeptocore_v28.pdf)
- [Source code](https://github.com/schollz/_core)
- [Firmware](https://zeptocore.com/#uploading-firmware)
- [Instructions for uploading firmware](https://zeptocore.com/#instructions) 
- [Video demonstration](https://www.youtube.com/watch?v=WBvos0TkcSY)
- [Video DIY guide](https://www.youtube.com/watch?v=FH1R4RCh0vU)



## EZEPTOCORE

The EZEPTOCORE is a eurorack version of the zeptocore developed by Infinite Digits in collaboration with Maneco Labs. See [below](#attributions) for full atributions.


<div align="center">
<img src="docs/static/img/ezeptocore.png" height="300px">
</div>

### EZEPTOCORE firmware 

The firmware is divided into two categories: *overclocking* and *non-overclocking*. 

- Choose *overclocking*  if you are using an external clock and want maximum CPU bandwidth for FX. These builds run faster but can exhibit slight clock drift if not externally synced.
- Choose *non-overclocking*  if you are using the internal clock and need extremely stable timing. These builds have slightly reduced CPU overhead but offer the highest temporal stability.

For latency, normal latency will work for most, but choose low or ultra-low if you encounter latency issues (note: available FX bandwidth decreases for low latency).

|                  | Normal Latency                                                                                            | Low Latency                                                                                                           | Ultra-Low Latency                                                                                                          |
| ---------------- | --------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------------- | -------------------------------------------------------------------------------------------------------------------------- |
| Overclocking     | [v7.0.1](https://github.com/schollz/_core/releases/download/v7.0.1/ezeptocore_v7.0.1.uf2)*                | [v7.0.1](https://github.com/schollz/_core/releases/download/v7.0.1/ezeptocore_v7.0.1_low_latency.uf2)                 | [v7.0.1](https://github.com/schollz/_core/releases/download/v7.0.1/ezeptocore_v7.0.1_ultralow_latency.uf2)                 |
| Non-Overclocking | [v7.0.1](https://github.com/schollz/_core/releases/download/v7.0.1/ezeptocore_v7.0.1_no_overclocking.uf2) | [v7.0.1](https://github.com/schollz/_core/releases/download/v7.0.1/ezeptocore_v7.0.1_no_overclocking_low_latency.uf2) | [v7.0.1](https://github.com/schollz/_core/releases/download/v7.0.1/ezeptocore_v7.0.1_no_overclocking_ultralow_latency.uf2) |

*default firmware

### diy

- [Schematic](https://github.com/schollz/_core/blob/main/schematics/ezeptocore-schematic.pdf)
- [Source code](https://github.com/schollz/_core)
- [Firmware](https://github.com/schollz/_core/releases)


## ectocore (discontinued)

the ectocore is the eurorack version of the zeptocore developed by Infinite Digits in collaboration with Toadstool Tech. See [below](#attributions) for full atributions.

<div align="center">
<img src="docs/static/img/ectocore_2.png" height="300px">
</div>


### ectocore firmware 


The firmware is divided into two categories: *overclocking* and *non-overclocking*. 

- Choose *overclocking*  if you are using an external clock and want maximum CPU bandwidth for FX. These builds run faster but can exhibit slight clock drift if not externally synced.
- Choose *non-overclocking*  if you are using the internal clock and need extremely stable timing. These builds have slightly reduced CPU overhead but offer the highest temporal stability.

For latency, normal latency will work for most, but choose low or ultra-low if you encounter latency issues (note: available FX bandwidth decreases for low latency).

|                  | Normal Latency                                                                                          | Low Latency                                                                                                         | Ultra-Low Latency                                                                                                        |
| ---------------- | ------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------ |
| Overclocking     | [v6.4.5](https://github.com/schollz/_core/releases/download/v6.4.5/ectocore_v6.4.5.uf2)*                | [v6.4.5](https://github.com/schollz/_core/releases/download/v6.4.5/ectocore_v6.4.5_low_latency.uf2)                 | [v6.4.5](https://github.com/schollz/_core/releases/download/v6.4.5/ectocore_v6.4.5_ultralow_latency.uf2)                 |
| Non-Overclocking | [v6.4.5](https://github.com/schollz/_core/releases/download/v6.4.5/ectocore_v6.4.5_no_overclocking.uf2) | [v6.4.5](https://github.com/schollz/_core/releases/download/v6.4.5/ectocore_v6.4.5_no_overclocking_low_latency.uf2) | [v6.4.5](https://github.com/schollz/_core/releases/download/v6.4.5/ectocore_v6.4.5_no_overclocking_ultralow_latency.uf2) |


### diy


- [Schematic](https://github.com/schollz/_core/blob/main/schematics/ectocore_v1.0.1.pdf)
- [Source code](https://github.com/schollz/_core)
- [Firmware](https://github.com/schollz/_core/releases)
- [Instructions for uploading samples](https://www.youtube.com/watch?v=NfjjhU1z6Ek) 


## zeptoboard

zeptoboard is the breadboard variant of the zeptocore. It has most of the same functionality, but instead of using the buttons on the handheld device, you can utilize your keyboard. This version requires some knowledge of breadboarding, but it is ideal if you want to develop your ideas based on the firmware. more information [here](https://zeptocore.com/#zeptoboard).

<div align="center">
<img src="docs/static/img/zeptoboard_img.png" height="350px">
</div>


# license

- Apache License 2.0 for no-OS-FatFS (Copyright 2021 Carl John Kugler III)
- MIT license for the SdFat library (Copyright (c) 2011-2022 Bill Greiman)
- MIT license for the USB library (Copyright (c) 2019 Ha Thach)
- GPLv3 for all _core code
- Hardware: cc-by-sa-3.0

## guidelines for derivative works

The schematics are open-source - you are welcome to utilize them to customize the device according to your preferences. If you intend to produce boards based on my schematics, I kindly ask for your financial support to help sustain the development of future devices.
Also note - Infinite Digits and Ectocore are registered trademarks. The name "Infinite Digits" and "Ectocore" should not be used on any of the derivative works you create from these files. 


## attributions 

<details><summary>Click here to see the full attributions for the Ectocore and EZEPTOCORE projects</summary>

<br>

The "Infinite Digits x Maneco Labs EZEPTOCORE" is a eurorack version of the Infinite Digits' handhold sample slicer, the [zeptocore](https://zeptocore.com), with manufacturing and hardware design by Maneco Labs. The [zeptocore](https://zeptocore.com) is also the basis for the 2024 Ectocore which which had manufacturing and hardware co-designed with Toadstool Tech ("Infinite Digits x Toadstool Tech Ectocore"). This acknowledges the work encompassing both of these products.

- ID = Infinite Digits
- ML = Maneco Labs
- TT = Toadstool Tech
- EG = Émilie Gillet
- IN = Instruo

Both the EZEPTOCORE and Ectocore are an open-source modular version of ID's open-source [zeptocore](https://zeptocore.com) device. The [zeptocore](https://zeptocore.com) itself comes from a long lineage of Infinite Digit's open-source devices and open-source software including [pikocore](https://github.com/schollz/pikocore), [nyblcore](https://github.com/schollz/nyblcore), [amen script](https://github.com/schollz/amen), [glitchlets script](https://github.com/schollz/glitchlets), [amenbreak script](https://github.com/schollz/amenbreak), [abacus script](https://github.com/schollz/abacus), [makebreakbeat script](https://github.com/schollz/makebreakbeat), [sampswap script](https://github.com/schollz/sampswap), [dnb.lua utility](https://github.com/schollz/dnb.lua), [raw script](https://github.com/schollz/raw), and the [paracosms](https://github.com/schollz/paracosms) script. These devices and software libraries have their own long legacies and many acknowledgments, but would especially like to acknowledge being inspired by [Jerboa modular synthesizer](http://wiki.yak.net/1132) (for inspiring me to use the attiny85), Fay Carsons (for inspiring to use the rp2040), Limor Fried and EG (for pioneering CC-BY-SA hardware), and Nick Collins (for the Breakcore UGen), the open-source contributors to RP2040 community (Raspberry Pi Foundation, Carl J Kugler III for the SDIO library, which is built upon FatFS which I am thankful for), Steven Noreyko and Jacob Vosamer for helping improve MIDI and porting to RP2040v2, as well as countless musicians who inspire all ID creations and all the open-source maintainers who I find inspiration and inspire me to continue to produce open-source designs and making my work freely available to remix and re-purpose.

The EZEPTOCORE and Ectocore websites and sample manipulator and downloader was developed and maintained by ID. The open-source tools for splitting drums was designed at [Facebook Research by Alexandre Défossez](https://github.com/facebookresearch/demucs) which is used to generate the splice points to do a Trig Out in the Ectocore. The automatic splicing was done using open-source Aubio library.

The name "EZEPTOCORE" is a product of ID, combining "eurorack" and "zeptocore" to signify its lineage.
The name "Ectocore" is a collaboration of TT+ID, combining of TT's mythical ethos ("ecto") and ID's *core products ("core").

TT asked for the following attribution and credit text without modification:
"Ectocore's original hardware design, interface layout, and artwork were created by Toadstool Tech / Izaak Hollander. Toadstool Tech has no involvement with the current Maneco Labs/Infinite Digits 2025 iteration of Ectocore and is not responsible for any support related to iterations not branded 'Toadstool Tech'. This product is not a collaboration with Toadstool Tech, and there was no involvement with its engineering or marketing. Toadstool Tech will receive no monetary compensation from sales of this product."

ID would like to clarify compensation of TT: TT was paid for the original Ectocore collaboration, and compensation was offered for the EZEPTOCORE project but TT declined.

ID would like to clarify the attributions of the original Ectocore hardware design: the original design was based from Infinite Digits' [open-source pikocore schematic](https://github.com/schollz/pikocore) and [open-source zeptocore schematic](https://github.com/schollz/_core) combined with open-source designs from Raspberry Pi foundation, Adafruit, and Émilie Gillet's [plaits](https://pichenettes.github.io/mutable-instruments-documentation/modules/plaits/downloads/plaits_v50.pdf) schematic (licensed by CC-BY-SA schematics).  Iteration on the Ectocore hardware design was done by TT, ID, and IN. The final Ectocore hardware schematic and board files developed solely by TT.

ID would like to clarify the attributions of the interface layout of the Ectocore: The interface was designed throughout a collaboration between TT and ID, combining of TT's mythical inspiration ("Grimoire") and ID's interfaces from previous monome norns scripts written by ID (e.g. [amenbreak script](https://github.com/schollz/amenbreak) for singular "amen" and "break" knobs, [makebreakbeat script](https://github.com/schollz/makebreakbeat) for sample splicing, [sampswap script](https://github.com/schollz/sampswap), [dnb.lua utility](https://github.com/schollz/dnb.lua) for "tunneling" and jumping) and inspiration from devices previously created by ID ([pikocore](https://github.com/schollz/pikocore) and [nyblcore](https://github.com/schollz/nyblcore)). ID also acknowledges that the norns scripts developed by ID that inspired the Ectocore and EZEPTOCORE panel design were born out of ideas from many other people, built in a community of open-source creations, with special thanks to scanner_darkly (who came up with the single "amen" and "break" knob idea) and Nick Collins (who created the inspirational Breakcore UGen from SuperColldier).

The "Infinite Digits x Maneco Labs EZEPTOCORE" front panel is inspired by the design from TT+ID, and incoroprates changes by ML to add a reset button to the front.

ID also wants to acknowledge the countless community members of the open-source world (Supercollider, monome norns, Raspberry Pi, Adafruit, many many more) who I have been inspired from and continue to be inspired, and from their work I am grateful and continue to try to pay forward by continuously making my work similarly freely open-source and available. (One note on that: The final hardware design from TT and ML are NOT open-source as they are proprietary designs of their own work, each created separately based on my open-source [zeptocore](https://zeptocore.com) device).


</details>

