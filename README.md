# `_core`

[![build workflow](https://github.com/schollz/_core/actions/workflows/build.yml/badge.svg)](https://github.com/schollz/_core/actions/workflows/build.yml) [![GitHub Release](https://img.shields.io/github/v/release/schollz/_core)](https://github.com/schollz/_core/releases/latest)


this is the monorepo for [zeptocore](https://zeptocore.com), [zeptoboard](https://zeptocore.com/#zeptoboard), and [ectocore](https://getectocore.com) music devices, their documentation, their firmware, and the tools to interact with them.

for information about purchasing and documentation, visit [zeptocore.com](https://zeptocore.com). demos are available [on youtube](https://www.youtube.com/watch?v=FZ2C9VIMgeI&list=PLCNN6FnBNdpWQUyHAQO_wCQkbMl95-293).

## zeptocore

the zeptocore device is a versatile, open-source, handmade audio player and synthesizer, featuring stereo playback of 16-bit audio files at a 44.1 kHz sampling rate. 

<div align="center">
<img src="docs/static/img/zeptocore_noche.png" width="70%">
</div>

the zeptocore supports SD-card storage for up to 32 gigabytes of samples and can recall up to 256 audio files organized into 16 banks of 16 tracks each. the zeptocore has 16 different audio effects - saturation, fuzz, delay, comb, beat repeater, filter, tape stop, reverb + more - and includes a single-cycle wavetable synthesizer. The device offers a real-time sequencer with optional quantization, optional clock sync out, and MIDI (in and out) over USB. the device has a built-in 8-ohm speaker and can be powered by two AAA batteries or USB-C.

The firmware for the zeptocore is written in C, and instructions for building the firmware are in the [documentation](https://zeptocore.com/#firmware-development).

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

#### Download for Windows: **[x64](https://github.com/schollz/_core/releases/download/v6.2.13/core_windows_v6.2.13.exe)**

Once downloaded, double click on the executable file to run it.

</details>


<details><summary>macOS</summary>

To install the tool on macOS, first open a terminal.

Then, if you are on an Intel-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v6.2.13/core_macos_amd64_v6.2.13 > core_macos
```

Or, if you are on a M1/M2-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v6.2.13/core_macos_aarch64_v6.2.13 > core_macos
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

#### Download for Linux: **[x64](https://github.com/schollz/_core/releases/download/v6.2.13/core_linux_amd64_v6.2.13)**

After downloading, run it directly from the terminal.

</details>

# license

- Apache License 2.0 for no-OS-FatFS (Copyright 2021 Carl John Kugler III)
- MIT license for the SdFat library (Copyright (c) 2011-2022 Bill Greiman)
- MIT license for the USB library (Copyright (c) 2019 Ha Thach)
