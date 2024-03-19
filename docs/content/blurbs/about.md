+++
title = 'About'
date = 2024-02-01T12:32:51-08:00
weight = 2
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

### goals

1. **open-source**. [the source code](https://github.com/schollz_core) is publicly available so that anyone can change this device today or years from now.
2. **low learning curve**. the device should be fun without reading a manual, but reading the manual only adds to the fun.
3. **unquantized**. by default, the sequencer does not enforce "quantization" and leave it to the user to make their own timings.

### non-goals

1. **similarity**. I do not intend this device to be like something else. some of the decisions are highly opinionated, but since it is completely [open-source](https://github.com/schollz/_core) you could change the device to your heart's content.
2. **audio synthesis**. the CPU is already pretty taxes with the current system, and synthesis requires way more cycles to sound good in my opinion.
3. **polyphony**. this device only supports two voices: a monophonic sample voice and a monophonic bass voice. these voices are smooth crossfaded, but the CPU limits having more polyphony (consider having more devices synced together instead!).

### friends

if you like this device, you might also like these other small devices made by friends:

- [joydrums](https://www.yzhkinstruments.com/download), as if a beat pad and a loop pedal had a baby.
- [bitty](https://www.curioussoundobjects.com/), an Arduino-compatible device that makes making music fun.
- [hss2020](http://gieskes.nl/instruments/?file=HSS2020), a trimmed down version of the hard soft synth that fits in the hands.
- [phantasmal force](https://www.tindie.com/products/distropolis/phantasmal-force-micro-midi-controller/), a micro midi controller.
