# _core

this is the source code for *_core* music devices that utilize the rp2040. devices may differ, but the basic specification is as follows:

- mono or stereo playback of 16-bit (internal 32-bit) audio files @ 44.1 kHz sampling rate
- sd-card storage for audio + data
- recall up to 256 audio files (16 banks of 16 tracks)
- digital low-pass and high-pass filter
- basic single-cycle wavetable synthesizer
- realtime sequencer with optional quantization
- low-latency (<10 ms)


## boardcore

breadboard version (see [demo](https://www.instagram.com/p/CvzdZTYtV8H/)).

- raspberry pi pico (various versions)
- pcm5102
- sdio sd card

![PXL_20231225_060210016-removebg-preview](https://github.com/schollz/_core/assets/6550035/a33e5fcb-b052-48ba-ab71-0d95d77dea5c)



## zeptocore

hand-held version (see [demo](https://www.instagram.com/p/C1PFLGDvB9I/)).

<center>
<img src="https://github.com/schollz/_core/assets/6550035/c648be5f-eb1a-438f-979d-3e01331d259b" width=50%>
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

- [ ] **none** → top shows punch/mash, mute, playback states 
- [ ] **H** → (JUMP): do jump, (MASH): toggle fx, (BASS): play bass
- [ ] **H** + **H** → JUMP: retrig depending on location
- [ ] **A** → n/a
- [ ] **A** + **H** → JUMP: do fx (toggle), MASH/BASS: do jump
- [ ] **A** + **B** → JUMP mode
- [ ] **A** + **C** → MASH mode
- [ ] **A** + **D** → BASS mode
- [ ] **B** → show bank (blinking) + sample (bright)
- [ ] **B** + **H** + **H** → select bank (1st) + sample (2nd)
- [ ] **B** + **C** → ?
- [ ] **B** + **D** → ?
- [ ] **C** → show which sequence is selected (bright)
- [ ] **C** + **H** + **H**... → create chain of sequences
- [ ] **C** + **B** → toggle play sequence
- [ ] **C** + **D** → toggle record sequence
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
- [ ] **B** + **X** → gate
- [x] **B** + **Y** → filter fc (lowpass/highpass?)
- [ ] **B** + **Z** → filter q
- [ ] **C** + **X** → pitch
- [ ] **C** + **Y** → quantize
- [ ] **C** + **Z** → 
- [ ] **D** + **X** → 
- [ ] **D** + **Y** → 
- [ ] **D** + **Z** → 

#### effects 

there are 16 effects in four categories - "shape", "time", "space", and "pitch".
holding an effect and using a knob will change its parameters.

- [ ] **1** -> warm (preamp multiplier)
- [ ] **2** -> loss (preamp multiplier, threshold)
- [ ] **3** -> fuzz (preamp multliplier)
- [ ] **4** -> crush (frequency, crushing)
- [ ] **5** -> reverse 
- [ ] **6** -> stretch
- [ ] **7** -> delay (delay length, feedback)
- [ ] **8** -> repeat (repeat length)
- [ ] **9** -> tighten (gate amount)
- [ ] **10** -> heighten (tremelo speed, depth)
- [ ] **11** -> circulate (pan speed, depth)
- [ ] **12** -> waltz
- [ ] **13** -> lower (depth)
- [ ] **14** -> slower (duration, depth)
- [ ] **15** -> faster (duration, depth)
- [ ] **16** -> stop (duration)

#### cheat codes

- [ ] **4**,**5**,**4** -> toggle one-shot mode
- [ ] **7**,**8**,**7** -> toggle play mode
- [ ] **9**, **12**, **10**, **11** -> toggle tempo match mode
- [ ] **13**,**16**,**14**,**15** -> change resampling (linear or quadratic)
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

# license

- Apache License 2.0 for no-OS-FatFS (Copyright 2021 Carl John Kugler III)
- MIT license for the SdFat library (Copyright (c) 2011-2022 Bill Greiman)
