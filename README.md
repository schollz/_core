# _core

this is the source code for *_core* music devices that utilize the rp2040. devices may differ, but the basic specification is as follows:

- mono or stereo playback of 16-bit (internal 32-bit) audio files @ 44.1 kHz sampling rate
- sd-card storage for audio + data
- recall up to 256 audio files (16 banks of 16 tracks)
- digital low-pass and high-pass filter
- basic single-cycle wavetable synthesizer
- realtime sequencer with optional quantization
- low-latency (<10 ms)


## breadboard

these are the minimal requirements, which will be outlined below (TODO). ([demo](https://www.instagram.com/p/CvzdZTYtV8H/))

- raspberry pi pico (various versions)
- pcm5102
- sdio sd card

![PXL_20231225_060210016-removebg-preview](https://github.com/schollz/_core/assets/6550035/a33e5fcb-b052-48ba-ab71-0d95d77dea5c)



## zeptocore

the hand-held version of the _core software (see [demo1](https://www.instagram.com/p/C1PFLGDvB9I/)).

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

top buttons are **S**, **A**, **B**, and **C**.

buttons: S, A, B, C, H (H = 1-16)

knobs: X, Y, Z



#### combo buttons

- [ ] **none** → top shows punch/mash, mute, playback states 
- [ ] **H** → JUMP: do jump, MASH: do fx (momentary)
- [ ] **H** + **H** → JUMP: retrig depending on location
- [ ] **S** → n/a
- [ ] **S** + **H** → JUMP: do fx (toggle), MASH: do jump
- [ ] **S** + **A** → toggle playback
- [ ] **S** + **B** → toggle mute
- [ ] **S** + **C** → toggle jump/mash/bass mode
- [ ] **A** → show current bank (dim) + sample (bright)
- [ ] **A** + **H** + **H** → select bank (1st) + sample (2nd)
- [ ] **A** + **B** → toggle one-shot vs classic
- [ ] **A** + **C** → toggle sync mode (none, clock-in, clock-out, clock-in+out)
- [ ] **B** → n/a
- [ ] **B** + **H** + **H**... → create chain of sequences
- [ ] **B** + **A** → toggle play sequence
- [ ] **B** + **C** → toggle record sequence
- [ ] **B** + **H** → select sequence
- [ ] **C** → n/a
- [ ] **C** + **H** → select save slot
- [ ] **C** + **A** load from save slot
- [ ] **C** + **B** → save into save slot


#### combo knobs

- [ ] **X** → n/a
- [ ] **Y** → n/a
- [ ] **Z** → n/a
- [x] **S** + **X** → tempo
- [ ] **S** + **Y** → 
- [x] **S** + **Z** → volume
- [ ] **A** + **X** → gate
- [x] **A** + **Y** → filter fc (lowpass/highpass?)
- [ ] **A** + **Z** → filter q
- [ ] **B** + **X** → pitch
- [ ] **B** + **Y** → quantize
- [ ] **B** + **Z** → 
- [ ] **C** + **X** → 
- [ ] **C** + **Y** → 
- [ ] **C** + **Z** → 

#### effects 

effects for the 16 buttons 

- [ ] **0** -> timestretch 
- [ ] **1** -> reverse
- [ ] **2** -> pitch down
- [ ] **3** -> pitch up
- [ ] **4** -> volume
- [ ] **5** -> filter
- [ ] **6** -> 
- [ ] **7** -> 
- [ ] **8** -> saturate
- [ ] **9** -> bitcrush
- [ ] **10** -> tighten
- [ ] **11** -> 
- [ ] **12** -> TODO repeat
- [ ] **13** -> tremelo
- [ ] **14** -> panning
- [ ] **15** -> tape stop/start

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
