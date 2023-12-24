# *core

source code for rp2040-based sample wrangler's.

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

# ectocore

the eurorack version of the _core software, made in collaboration with [Toadstool Tech](https://www.instagram.com/the_izaak_guy/).

![image](https://github.com/schollz/_core/assets/6550035/7fee4176-5166-4b33-8182-aa8e343e2ab7)


# zeptocore

the hand-held version of the _core software

![image](https://github.com/schollz/zeptocore/assets/6550035/1d834182-fea8-41aa-830a-b5a894e1f2a2)

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

a non-goal for this project is to be emulate a specific musical device. there are few devices that are fun to randomly button mash, with exception of  which I consider an inspiration to this project.

a non-goal for this project is synthesis. though I may change my mind about this.

a non-goal for this project is an enclsoure. I encourage custom enclosures, but I like the aesthetic of the circuit board.


## similar devices

if you like this device, you might also like:

- [joydrums](https://www.yzhkinstruments.com/download), as if a beat pad and a loop pedal had a baby.
- [bitty](https://www.curioussoundobjects.com/), n Arduino-compatible device that makes making music fun.
- [hss2020](http://gieskes.nl/instruments/?file=HSS2020), a trimmed down version of the hard soft synth that fits in the hands.
- [phantasmal force](https://www.tindie.com/products/distropolis/phantasmal-force-micro-midi-controller/), a micro midi controller.


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

#### needs

- toggle sync mode (no sync, clock-in sync, out sync, in + out sync)

### combo knobs

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

### effects 

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

### cheat codes

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


### names

- combocore
- combonator
- zeptocore

TODO: use zeptocore/static/download.sh to download offline versions of libraries
TODO: create favicon.ico
TODO: t-shirts
TODO: add sine wave????
