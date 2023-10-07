# zeptocore

audio mangler for 16-bit stereo samples straight from an SD card.

![image](https://github.com/schollz/zeptocore/assets/6550035/1d834182-fea8-41aa-830a-b5a894e1f2a2)


## spec

buttons: S, A, B, C, H (H = 1-16)

knobs: X, Y, Z

S is nominally "shift"


### combo buttons

- [ ] *H* → JUMP: do jump, MASH: do fx (momentary)
- [ ] *H* + *H* → JUMP: retrig depending on location
- [ ] *S* + *H* → JUMP: do fx (toggle), MASH: do jump
- [ ] *S* → n/a
- [ ] *S* + *A* → toggle punch-in/jump mode
- [ ] *S* + *B* → toggle mute
- [ ] *S* + *C* → toggle playback
- [ ] *A* → show current bank (dim) + sample (bright)
- [ ] *A* + *B →* select bank mode
- [ ] *A* + *C →* select sample mode
- [ ] *A* + *H* → select bank/sample (depending on mode)
- [ ] *B* → n/a
- [ ] *B* + *A →* toggle play sequence
- [ ] *B* + *C →* toggle record sequence
- [ ] *B* + *H* → select sequence
- [ ] *C* → n/a
- [ ] *C* + *A →* load from save slot
- [ ] *C* + *B* → save into save slot
- [ ] *C* + *H* → select save slot

### combo knobs

- [ ] *X → all probabilities (chaos)*
- [ ] *Y →* filter
- [ ] *Z →* volume
- [ ] *S* + *X* → tempo
- [ ] *S* + *Y →* pitch
- [ ] *S* + *Z* → gate
- [ ] *A* + *X* → distortion level
- [ ] *A* + *Y →* distortion wet
- [ ] *A* + *Z* → saturation wet
- [ ] *B* + *X* → probability jump away
- [ ] *B* + *Y →* probability reverse
- [ ] *B* + *Z* → probability probability retrig
- [ ] *C* + *X* → probability tunnel
- [ ] *C* + *Y →* probability pitch down
- [ ] *C* + *Z* → probability jump stay

### visualization

- [ ] A light: bright on playback
- [ ] B light: blink on sequence recording, bright on sequence playing
- [ ] C light:


### knob functions


## known bugs

- when uploading new files, the audio plays but buttons do not work. when resetting, everything works. (blinking z)