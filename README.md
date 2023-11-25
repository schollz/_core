# zeptocore

a tiny open-source sample wrangler.

![image](https://github.com/schollz/zeptocore/assets/6550035/1d834182-fea8-41aa-830a-b5a894e1f2a2)

## the spec

- mono or stereo playback of 16-bit audio files @ 44.1 kHz sampling rate
- internal 32-bit
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

## goals and non-goals

### goals

firstly, my goal for this project is to be self-sustaining. I am making this project completely open-source (hardware and firmware). 
from my experience, an open-source hardware project will reduce sales and still place a burden on tech-support. 
this is risky, because I already spent a lot of money (>$2k) developing the project. to me, the risk is worth the benefit of making it open-source and empowering/inspirating new projects from this project.
so I ask that, while this is project is open-source and available for remixing, please consider consider this project by buying it or supporting me through [my github sponsorship](TODO).

secondly, my goals for this project is to create a simple tactile input for skipping through a sample with interesting sample-based effects and minimal visuals.

thirdly, my goal for this project is to have it survive into the future. hopefully the rp2040 chip doesn't disappear anytime soon.

fourthly, my goal for this project is to make it possible to mash buttons to achieve fun results without skill or knowledge.

### non-goals

a non-goal for this project is to be emulate a specific musical device. there are few devices that are fun to randomly button mash, with exception of [joydrums](TODO) which I consider an inspiration to this project.

a non-goal for this project is synthesis. though I may change my mind about this.

a non-goal for this project is an enclsoure. I encourage custom enclosures, but I like the aesthetic of the circuit board.


## ideas

- [ ] every punch in effect can be modulated by the X knob (for example pitch can be dialed in, stutter amount can be dialed in


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

#### needs

- toggle sync mode (no sync, clock-in sync, out sync, in + out sync)

### combo knobs

- [ ] **X** → n/a
- [ ] **Y** → n/a
- [ ] **Z** → n/a
- [ ] **S** + **X** → tempo
- [ ] **S** + **Y** → filter
- [ ] **S** + **Z** → volume
- [ ] **A** + **X** → pitch
- [ ] **A** + **Y** → gate
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


### names

- combocore
- combonator
- zeptocore

TODO: use zeptocore/static/download.sh to download offline versions of libraries
TODO: create favicon.ico
TODO: t-shirts
TODO: add sine wave????
