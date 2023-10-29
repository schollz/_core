# zeptocore

audio mangler for 16-bit samples straight from an SD card.

![image](https://github.com/schollz/zeptocore/assets/6550035/1d834182-fea8-41aa-830a-b5a894e1f2a2)

## what the spec

- mono or stereo playback of 16-bit audio @ 44.1 kHz sampling rate
- sd-card storage for audio + data
- recall up to 256 audio files (16 banks of 16 tracks)
- digital low-pass and high-pass filter
- realtime sequencer with optional quantization
- two modes for realtime manipulation:
	- jump mode for instant splice cuts (crossfaded)
	- mash mode for instant fx additions
- responsive, latency is between 5-10 ms
- powered by USB or AAA batteries (consumes ~100 mA, should last ~12 hours on batteries)
- can load in pre-spliced files from Renoise (`.xnri`) or OP-1 files (`.aif`)

## what the heck



- (goal) open-source firmare code + electronic schematics
- (goal) simple tactile input with visual input unessecary
- (goal) sample-based manipulation
- (goal) longevity 
- (non-goal) synthesis
- (non-goal) plastic enclosures



there exist a lot of musical instruments. musical instruments exist to provide a bridge between musical ideas and expression. the quintessential instrument, to me, is the piano which amazingly can double as a rhythmic instrument and a melodic instrument. even more amazingly, it can be utilized without visual input - requiring only tactile input.

currently, for me personally, I feel like there is an immense amount of sonic space to explore in generating sounds from pre-recorded samples. there are methods of granulating, soft-cutting, re-recording. there are technical subtlites of interpolation, resampling, filtering. 

I wanted a tool to explore samples in a tactile way, without needing visual input. 

tactile input that lack nessecary visual input.

## ideas

- [ ] every punch in effect can be modulated by the X knob (for example pitch can be dialed in, stutter amount can be dialed in

## todos

- [ ] add timestretching, alt sounds (pre-generated)

## spec

top buttons are **S**, **A**, **B**, and **C**.

buttons: S, A, B, C, H (H = 1-16)

knobs: X, Y, Z



### combo buttons

- [ ] **none** → top shows punch/mash, mute, playback states 
- [ ] **H** → JUMP: do jump, MASH: do fx (momentary)
- [ ] **H** + **H** → JUMP: retrig depending on location
- [ ] **S** → n/a
- [ ] **S** + **H** → JUMP: do fx (toggle), MASH: do jump
- [ ] **S** + **A** → toggle playback
- [ ] **S** + **B** → toggle mute
- [ ] **S** + **C** → toggle jump/mash mode
- [ ] **A** → show current bank (dim) + sample (bright)
- [ ] **A** + **H** + **H** → select bank (1st) + sample (2nd)
- [ ] **A** + **B** → toggle one-shot vs classic
- [ ] **A** + **C** → ?
- [ ] **B** → n/a
- [ ] **B** + **H** + **H**... → create chain of sequences
- [ ] **B** + **A** → toggle play sequence
- [ ] **B** + **C** → toggle record sequence
- [ ] **B** + **H** → select sequence
- [ ] **C** → n/a
- [ ] **C** + **H** → select save slot
- [ ] **C** + **A** load from save slot
- [ ] **C** + **B** → save into save slot

#### needs

- toggle sync mode (no sync, in sync, out sync, in + out sync)

### combo knobs

- [ ] **X** → n/a
- [ ] **Y** → n/a
- [ ] **Z** → n/a
- [ ] **S** + **X** → tempo
- [ ] **S** + **Y** → filter
- [ ] **S** + **Z** → volume
- [ ] **A** + **X** → 
- [ ] **A** + **Y** → 
- [ ] **A** + **Z** → 
- [ ] **B** + **X** → 
- [ ] **B** + **Y** → quantize
- [ ] **B** + **Z** → 
- [ ] **C** + **X** → 
- [ ] **C** + **Y** → 
- [ ] **C** + **Z** → 

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

