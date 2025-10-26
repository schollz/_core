# Ectocore Quickstart

## Part I: Knobs and Buttons

0. **SD Card.** Holds 16 banks with 16 samples each. Prepared using _ectocore.rocks_, which organizes, slices, and assigns Grimoire effects. No sample length limit. Insert to auto-load banks on startup.
1. **Break Knob.** Controls the probability of triggering effects. Effects are determined by the Grimoire Knob’s current selection.
2. **Amen Knob.** Turing Machine–style sequencer. Fully counterclockwise plays normally; fully clockwise randomizes sequence order. Intermediate positions play sequences from 1–16 steps.
3. **Sample Knob.** Changes the active sample. Hold the Bank Button to change banks. Each bank holds up to 16 samples.
4. **Bank Button.** Press to cycle through banks.
5. **Mode Button.** Cycles Trig Out behavior between Kick, Snare, Transient, and Random trigger modes.
6. **Clock Multiplier Button.** Tap to multiply clock; hold to divide clock.
7. **Tap Tempo Button.** Tap multiple times to set tempo. Also functions as a SHIFT modifier (see next section).
8. **Amen Random Knob.** Without CV, moving away from center increases jump probability. Turning left of center causes jumps not to return to the base position. Turning right of center preserves original sequence.
9. **Grimoire Knob.** Selects one of 7 effect mappings (“runes”) that the Break Knob can activate. Each rune can contain any combination of 16 available effects.
10. **Reset Button.** Press to reset the device. Use in combination with back panel button to activate disk mode.

## Part II: Knobs and Buttons (while holding Tap Tempo)

0. **Tap Tempo Button.** These alternate functions apply only while holding this button.
1. **Volume Knob.** Turn clockwise to increase volume. Further rotation adds saturation, then distortion.
2. **DJ Filter Knob.** Turn left of center for low-pass filtering, right of center for high-pass filtering.
3. **Gating Knob.** Turn right to increase slice gating amount.
4. **Calibration Button.** Set all knobs to 12 o’clock. While holding Tap Tempo and this button, press Reset to calibrate.
5. **Toggle Playback Button.** Without Clock In, starts or stops playback. Stopping playback saves the current bank and sample for auto-reload. With Clock In, resets to the start of the sample.

## Part III: CV Inputs

1. **Amen Input.** Controls slice sequencing via CV. Set to Jump or Hold mode on _ectocore.rocks_. Supports unipolar (0–5 V) or bipolar (–5 V to +5 V). When patched, the Amen Knob sets the slice range start, and the Amen Attenuator Knob sets the range end.
2. **Break Input.** Controls probability (0–100%) of activating Grimoire-selected effects. CV polarity can be set to unipolar or bipolar.
3. **Sample Input.** Selects the active sample within the current bank. CV polarity can be set to unipolar or bipolar.
4. **Clock In.** Syncs playback to an external 2 PPQN clock. “Stop if clock stops” or “Continue if clock stops” behavior can be set in settings.
5. **Trig Out.** Sends transient triggers (Kick, Snare, Transient, Random) as defined by the Mode Button.
6. **Clock Out.** Outputs the internal clock for syncing other devices. Can be set to **Trig** (50 ms pulse) or **Gate** (½ eighth-note length).

# Ectocore Youtube Video Transcriptions

## Ectocore - 10 min tutorial

this is the Ectocore. I’m going to tell you a little bit about the overview of the panel, and then I’ll talk about how to load samples onto it. this is just an image of it, but we can start at the top. this first knob adds effects — there are sixteen different effects, and I’ll talk more about them. this knob is a roller sequencer. this knob changes samples, and the button next to it helps you change between the sixteen banks. the button down here is a trig out mode, which I’ll talk more about in a bit. the button next to it is a clock out multiplier, so you can send this clock out to other things and keep everything in sync. the trig out is nice because you can use it to trigger certain transients. this knob up here will make the sequence more random. this knob changes the effects that are being applied. there are cv inputs for sequencing the effects, the clock in, and the sample. you have 16-bit stereo audio output.

now, today I’m going to talk about this sd card. the sd card holds sixteen banks with sixteen samples each, and there’s no sample limit. to get things onto the sd card, you’ll want to go to ectocore.rocks. oh — I almost forgot — there’s also a tap tempo button.

ectocore.rocks is essentially a way to prepare samples for the Ectocore. the Ectocore splits the workflow into performance and preparation. you use the Ectocore hardware to perform, and you use the ectocore.rocks tool to prepare your performance — to organize, splice, and analyze samples. let’s get started by making our own workspace. before I get into that, I just want to mention that up here, you can click to get the latest firmware, download the latest manual, or click down here to use offline versions of this tool if you need them. one caveat about this tool is that it’s online — these URLs are publicly available. but as long as you choose a URL that nobody else knows, no one will be able to access your information. if you’d prefer, you can download one of the offline versions.

I’m going to create one called “Mr. Shoe” — that’s the name of my cat. once you load it, you’ll have sixteen banks. you can select these different banks on the left, and then drop in some audio. let’s drop an audio file into bank one. as you can see on the right, it automatically recognizes OP-1 drum patches, Renoise splices, and Morphagene reels, and it will automatically splice them. if you have those formats, you can use their built-in splicing features. let’s just open up a sample from my library. as you can see, this sample has the BPM in the title — that’s helpful for the tool to recognize it. if you don’t have that, it’ll try to determine BPM by the length of your sample. hopefully, your sample’s length is a multiple of its number of beats — otherwise, the splicing can get a little tricky. once you’ve loaded something, you can click on it, and it shows up in another screen.

here, we have a bunch of things we can change. you can switch between stereo and mono — this one’s mono, but we can change it to stereo if we want. if you have a stereo sample, you can also convert it to mono. tempo matching is on, which means that when you change the tempo on the Ectocore, it will change the pitch of the sample to stay in sync. the source tempo for this one is 170 bpm, which is read from the file name — but if your slice has a precise length, it’ll figure out the bpm automatically. now we’re going to do some waveform editing. hit slice — here you can change the slices. you can create evenly spaced slices — before it had 32, so we can go back to 32 if we want, but you don’t have to have a multiple of 16. you can pick any number you like. you’ll see that 32 aligns pretty well with the transients.

you can also create slices from transients. say we wanted only eight slices, but we wanted them on the strongest transients — it’ll grab the best eight for you. then you can click on each one to hear what it sounds like. if you really want to fine-tune your slicing, you can zoom in, click and drag slices around, and adjust the endpoints to shrink or expand them. you can pinpoint them exactly how you want — and click to listen.

you’ll notice tiny little icons here — these are transient trig outs. when I showed you the trig out on the Ectocore panel earlier, these icons represent when those triggers fire. there’s one for kick drum, snare drum, and other transients — all automatically detected and placed where those events occur.

the tool also has something called the grimoire of breaks. if you go to the right-hand menu, you can access the rune effects. these are the sixteen effects that can be mapped to the Ectocore’s break knob. you can select different effects for each rune — things like filtering, reversing, bit reduction, and time stretching. each effect has a few adjustable parameters, and you can test how it sounds in the browser by clicking the preview button.

when you export your workspace to the sd card, the grimoire is written along with your samples, so when you turn the break knob on the Ectocore, it changes which effect is happening in real time. this means the knob directly controls which rune is active, letting you morph the playback of your samples during performance. the effects are stored per bank, so each set of sixteen samples can have its own set of rune mappings.

you can also customize how strong each effect is or whether it responds to cv modulation. for example, you can have subtle filtering on one bank and more extreme glitch or reverse effects on another. once you’re done, you can save your workspace and safely eject the sd card. insert it into the Ectocore, and it’ll load everything automatically.

when performing with the Ectocore, you can simply turn the break knob to change which effect is happening. now once you’re done, you can easily go back into the browser tool to adjust your banks, change splices, or remap effects. the workflow between the Ectocore hardware and the ectocore.rocks site makes it really easy to prepare creative, performance-ready sample sets with detailed control over slices, transients, and modulation behavior.

## Ectocore - How does Amen Knob Work Transcript

The Amen knob basically allows you to randomly sequence slices in your sample. The attenuator also allows you to inject randomness and variation into your sample to have it play through your sample normally. Make sure the Amen knob is all the way counterclockwise and the tenor knob is facing straight up. If we turn the attenuator knob to the right, it’ll randomly jump as it plays through the sample but then jump back to where it was playing, so it stays in time. If you turn it to the left, it does the same thing but it no longer stays in time, as it will continue to play from where it jumped. Both ways are great to make a more chaotic sound or add more variation to your sample.

If it’s straight up, your sample will play straight through unless you change the Amen knob. If you turn up the Amen knob, you’ll see colored lights show, and those lights show the number of slices it’s selecting randomly and looping. Right now, I’m looping seven slices that are selected randomly. To change the selection, you can turn the knob all the way to the right and then back, and now it has selected a different set of slices. If you leave the knob all the way to the right, it will play through sixteen slices randomly and every two bars do a fill like that and change the slices that it selected. You can forget about changing the knob and just let it select random things constantly.

If you turn the knob all the way to the counterclockwise position, it’ll play through normally. These two knobs have completely different functions when you have CV plugged in. I have the Exore right now in the settings changed so that it uses unipolar CV into the Amen CV, and it will do a repeat on CV. That means that the Ecto core, at every pulse—two pulses per quarter note—will probe the CV at the Amen port and then change it to that slice.

Now I’m plugging in a random CV from Pams, and now in these knobs, you want to make sure this one’s all the way counterclockwise and then this one’s all the way clockwise. That will allow you to map the voltage coming into the CV to the entire sample. If you want to change which part of the sample is used, you can turn this knob to the left. You can see I’m selecting just a region of the sample to be used, and then this one will select the start of that region. In this way, you can select just pieces of your sample to be mapped from the voltage coming in.

Right now it’s getting a voltage of two pulses per quarter note, so if I change this instead of 2x into one, it’ll repeat every slice twice. You want to make sure that you’re sending in voltage at the correct 2 ppqn if you want it to change on every slice or every beat. Otherwise, you can set it in the settings on the SD card to just jump on the change in CV instead of on every two ppqn.

You can also set it to be bipolar, and you can have the positive side be a repeat on CV and the negative side be a jump on CV, so you get both actions. If you unplug the CV, it’ll go back to playing normally and have the same functions as the normal Amen knob and the attenuator knob. That’s it. Have fun.
