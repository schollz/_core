+++
title = 'About'
date = 2024-02-01T12:32:51-08:00
weight = 4
+++

# About 

The zeptocore is an *[open-source](https://github.com/schollz/_core)* handheld device for playing with samples, featuring:

- stereo playback of 16-bit audio files @ 44.1 kHz sampling rate
- sd-card storage for up to 8 gigabytes of samples
- recalls up to 256 audio files (16 banks of 16 tracks)
- 16 different [effects](#effect-list) (saturate, fuzz, delay, reverb, etc...)
- single-cycle wavetable synthesizer
- realtime sequencer with optional quantization
- clock sync in (or midi sync with [ittybittymidi](https://ittybittymidi.com))
- optional clock [sync out](#sync-out)
- tiny built-in speaker
- powered by two AAA batteries or USB-C

### Goals

1. **Open-source**: The source code is publicly available at [https://github.com/schollz/_core](https://github.com/schollz/_core), allowing anyone to modify this device today or in the future.
2. **Low learning curve**: The device should be enjoyable without the need to read a manual, although reading the manual can enhance the experience.
3. **Unquantized**: By default, the sequencer does not enforce 4/4 time signatures.

### Non-Goals

1. **Similarity**: The intention is not for this device to resemble anything else. Some decisions are highly opinionated, but since it is completely open-source, you can modify the device as you wish.
2. **Audio synthesis**: The CPU is already quite taxed by the current system, and I believe that synthesis requires significantly more cycles to sound good.
3. **Polyphony**: This device only supports two voices—a monophonic sample voice and a monophonic bass voice. These voices are smoothly crossfaded, but the CPU limits the possibility of having more polyphony. (Consider syncing more devices together instead!)

### friends

if you like this device, you might also like these other small devices made by friends:

- Distropolis Goods  <a href="https://www.instagram.com/distropolis/"><i class="fa-brands fa-instagram"></i></a>
  - [prismatic spray](https://www.tindie.com/products/distropolis/prismatic-spray-bytebeat-adventure-synth/) - a bytebeat adventure synth
  - [phantasmal force](https://www.tindie.com/products/distropolis/phantasmal-force-micro-midi-controller/) - a micro midi controller.
- Curious Sound Objects  <a href="https://www.instagram.com/curioussoundobjects/"><i class="fa-brands fa-instagram"></i></a>
  - [bitty](https://www.curioussoundobjects.com/) - an Arduino-compatible device that makes making music fun.
- Denki Oto   <a href="https://www.instagram.com/curioussoundobjects/"><i class="fa-brands fa-instagram"></i></a>
  - [OMX-27](https://www.denki-oto.com/store/p59/omx27-kit.html#/) - a compact MIDI controllers and sequencer
- yzhk instruments  <a href="https://www.instagram.com/joydrums_official/"><i class="fa-brands fa-instagram"></i></a>
  - [joydrums](https://www.yzhkinstruments.com/download) - as if a beat pad and a loop pedal had a baby.
- Gieskes <a href="https://www.instagram.com/gijsgieskes/"><i class="fa-brands fa-instagram"></i></a>
  - [hss2020](http://gieskes.nl/instruments/?file=HSS2020) - a trimmed down version of the hard soft synth that fits in the hands.
