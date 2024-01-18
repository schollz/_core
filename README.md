# _core

[![build workflow](https://github.com/schollz/_core/actions/workflows/build.yml/badge.svg)](https://github.com/schollz/_core/actions/workflows/build.yml) [![GitHub Release](https://img.shields.io/github/v/release/schollz/_core)](https://github.com/schollz/_core/releases/latest)


this is the source code for *_core* music devices that utilize the rp2040. devices may differ, but the basic specification is as follows:

- mono or stereo playback of 16-bit (internal 32-bit) audio files @ 44.1 kHz sampling rate
- sd-card storage for audio + data
- recall up to 256 audio files (16 banks of 16 tracks)
- digital low-pass and high-pass filter
- basic single-cycle wavetable synthesizer
- realtime sequencer with optional quantization
- low-latency (<10 ms)

there are a few versions of the "*_core*" devices that utilize this firmware:

## boardcore

breadboard version (see [demo](https://www.instagram.com/p/CvzdZTYtV8H/)).

- raspberry pi pico (various versions)
- pcm5102
- sdio sd card

![PXL_20231225_060210016-removebg-preview](https://github.com/schollz/_core/assets/6550035/a33e5fcb-b052-48ba-ab71-0d95d77dea5c)



## zeptocore

hand-held version (see [demo](https://www.instagram.com/p/C1PFLGDvB9I/)).

<center>
<img src="https://github.com/schollz/_core/assets/6550035/05e2b34b-efbc-47d1-8ba0-605ad723f85c" width=40%>
</center>


### goals

the goal of this project is to produce a device that achieves compelling music without learning, though learning is rewarded.

### non-goals

a non-goal for this project is to be emulate a specific musical device. 

a non-goal for this project is audio synthesis. though I may change my mind about this.


### similar devices

if you like this device, you might also like:

- [joydrums](https://www.yzhkinstruments.com/download), as if a beat pad and a loop pedal had a baby.
- [bitty](https://www.curioussoundobjects.com/), an Arduino-compatible device that makes making music fun.
- [hss2020](http://gieskes.nl/instruments/?file=HSS2020), a trimmed down version of the hard soft synth that fits in the hands.
- [phantasmal force](https://www.tindie.com/products/distropolis/phantasmal-force-micro-midi-controller/), a micro midi controller.



### zeptocore spec (draft)

knobs: X, Y, Z

top buttons are **A**, **B**, **C**, and **D**.

the rest of the buttons (1-16) are just called **H**.


#### combo buttons

x = finished

o = partially finished

- [ ] **none** → n/a
- [o] **H** → (JUMP): do jump, (MASH): toggle fx, (BASS): play bass
- [x] **H** + **H** → JUMP: retrig depending on location
- [ ] **A** → n/a
- [x] **A** + **H** → JUMP: do fx (toggle), MASH/BASS: do jump
- [x] **A** + **B** → JUMP mode
- [x] **A** + **C** → MASH mode
- [x] **A** + **D** → BASS mode
- [x] **B** → show bank (blinking) + sample (bright)
- [x] **B** + **H** + **H** → select bank (1st) + sample (2nd)
- [x] **B** + **C** → start/stop
- [x] **B** + **D** → mute
- [ ] **C** → show which sequence is selected (bright)
- [ ] **C** + **H** + **H**... → create chain of sequences
- [x] **C** + **B** → toggle play sequence
- [x] **C** + **D** → toggle record sequence
- [ ] **C** + **H** → select sequence
- [ ] **D** → show which save slot is selected (bright)
- [ ] **D** + **H** → select save slot
- [ ] **D** + **B** load from save slot
- [ ] **D** + **C** → save into save slot

#### combo knobs

- [ ] **X** → n/a
- [ ] **Y** → n/a
- [ ] **Z** → n/a
- [x] **A** + **X** → tempo
- [ ] **A** + **Y** → 
- [x] **A** + **Z** → volume
- [ ] **B** + **X** → 
- [x] **B** + **Y** → filter fc (lowpass/highpass?)
- [ ] **B** + **Z** → 
- [ ] **C** + **X** → pitch
- [ ] **C** + **Y** → 
- [x] **C** + **Z** → quantize
- [ ] **D** + **X** → 
- [ ] **D** + **Y** → 
- [ ] **D** + **Z** → 
- [o] **H** + **X/Y/Z** -> in MASH mode this edits the parameters of the effect

#### effects 

there are 16 effects in four categories - "shape", "time", "space", and "pitch".
holding an effect and using a knob will change its parameters.

- [x] **1** -> warm (preamp multiplier)
- [x] **2** -> loss (preamp multiplier, threshold)
- [x] **3** -> fuzz (preamp multliplier)
- [x] **4** -> crush (frequency, bitdepth)
- [x] **5** -> reverse 
- [x] **6** -> stretch
- [x] **7** -> delay (delay feedback, delay length)
- [x] **8** -> repeat (repeat length)
- [x] **9** -> tighten (gate amount)
- [x] **10** -> heighten (tremelo speed, depth)
- [x] **11** -> circulate (pan speed, depth)
- [ ] **12** -> phasor/flanger
- [x] **13** -> lower (depth)
- [o] **14** -> slower (duration, depth)
- [o] **15** -> faster (duration, depth)
- [o] **16** -> stop (duration)

#### cheat codes

- [x] **4**,**5**,**4** -> toggle one-shot mode
- [ ] **7**,**8**,**7** -> toggle play mode
- [x] **5**, **8**, **7**, **6** -> toggle tempo match mode
- [x] **9**,**12**,**10**,**11** -> change resampling (linear or quadratic)
- [ ] ?? -> toggle sync mode?

#### needs

- pitch
- gate
- all probabilities
- distortion level
- distortion wet
- saturation wet
- probability jump
- probability reverse
- probability retrig
- probability tunnel
- probability repitch

# building image

## windows

Install WSL 2

```
$ wsl --set-default-version 2
$ wsl --install Ubuntu
```

Then restart computer and run 

```
$ wsl --install
```

That should start your system. Then you can follow the Linux directions:

## linux

Install the pre-requisites:

```
sudo apt install cmake gcc-arm-none-eabi libnewlib-arm-none-eabi libstdc++-arm-none-eabi-newlib git python3 g++
sudo -H python3 -m pip install numpy matplotlib tqdm icecream librosa click
```

Clone this repo and install the Pico SDK:

```
git clone https://github.com/schollz/_core
cd _core
git clone https://github.com/raspberrypi/pico-sdk
cd pico-sdk &&  git submodule update --init
```

Do a build:

```
make clean && PICO_SDK_PATH=../pico-sdk make zeptocore
```

(replace '`zeptocore`' with '`ectocore`' if you are building the latter)

# license

- Apache License 2.0 for no-OS-FatFS (Copyright 2021 Carl John Kugler III)
- MIT license for the SdFat library (Copyright (c) 2011-2022 Bill Greiman)
