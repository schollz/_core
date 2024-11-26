
### zeptocore spec (draft)

knobs: X, Y, Z

top buttons are **A**, **B**, **C**, and **D**.

the rest of the buttons (1-16) are just called **H**.


#### combo buttons


- [x] **H** + **H** → JUMP: retrig depending on location
- [x] **A** + **H** → JUMP: do fx (toggle), MASH/BASS: do jump
- [x] **A** + **B** → JUMP mode
- [x] **A** + **B** + **B** ... → set tempo via tapping
- [x] **A** + **C** → MASH mode
- [x] **A** + **D** → BASS mode
- [x] **B** → show bank (blinking) + sample (bright)
- [x] **B** + **H** + **H** → select bank (1st) + sample (2nd)
- [x] **B** + **C** → start/stop
- [x] **B** + **D** → mute
- [x] **C** → display which sequence is selected (bright)
- [x] **C** + **H** → select sequence (led dim or bright = used, led off or blinking = unused) 
- [x] **C** + **H** + **H** + **H** ... → chains sequences together, though the first selected must be empty
- [x] **C** + **B** → toggle play sequence
- [x] **C** + **D** → toggle record sequence
- [x] **C** + **D**, **C** + **D** → erase current sequence
- [x] **D** → shows current slot (blinking / bright) and slots with data (dim)
- [x] **D** + **H** → select save slot
- [x] **D** + **B** load from save slot
- [x] **D** + **C** → save into save slot


#### combo knobs

- [x] **A** + **X** → tempo
- [x] **A** + **Y** → pitch
- [x] **A** + **Z** → volume
- [ ] **B** + **X** → 
- [x] **B** + **Y** → filter fc (lowpass/highpass?)
- [ ] **B** + **Z** → 
- [x] **C** + **X** → change bank
- [x] **C** + **Y** → change sample
- [x] **C** + **Z** → quantize
- [x] **D** + **X** → probability of jump
- [x] **D** + **Y** → probability of retrigger
- [x] **D** + **Z** → bass volume
- [o] **H** + **X/Y/Z** -> in MASH mode this edits the parameters of the effect

#### effects 

there are 16 effects in four categories - "shape", "time", "space", and "pitch".
holding an effect and using a knob will change its parameters.

- [x] **1** -> warm (preamp postamp)
- [x] **2** -> loss (type+threshold, postamp)
- [x] **3** -> fuzz (preamp postamp)
- [x] **4** -> crush (frequency, bitdepth)
- [x] **5** -> stretch
- [x] **6** -> delay (delay feedback, delay length)
- [x] **7** -> comb
- [x] **8** -> repeat (repeat length)
- [x] **9** -> tighten (gate amount)
- [x] **10** -> expand (intensity, wet/dry)
- [x] **11** -> circulate (pan speed, depth)
- [x] **12** -> scratch (scratch speed)
- [x] **13** -> lower (depth)
- [x] **14** -> slower (duration, depth)
- [x] **15** -> reverse
- [x] **16** -> stop (duration)

#### cheat codes

- [x] **1**,**2**,**1** -> toggle variable splice playback
- [x] **4**,**5**,**4** -> toggle one-shot mode
- [ ] **6**,**7**,**6** -> toggle play mode
- [x] **5**, **8**, **7**, **6** -> toggle tempo match mode
- [ ] ?? -> toggle sync mode?




