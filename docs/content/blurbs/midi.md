+++
title = 'MIDI'
date = 2024-02-01T12:32:51-08:00
weight = 9
+++

# MIDI

## MIDI Out

The zeptocore is a MIDI-compliant device that outputs midi when buttons are pressed or knobs are turned.

## MIDI In

The zeptocore will automatically interpret MIDI start/stop/continue messages and automatically sync to the MIDI timing of the incoming signal.

<small>

*Note:* Interpretting other messages like button presses is not yet implemented but planned for a future update.

</small>

There are two ways to do MIDI input to the zeptocore. 

### USB MIDI

Simply plug in a USB cable from the zeptocore to a computer. The zeptocore will show up as a MIDI device on the computer. You can then forward MIDI messages from the computer to the zeptocore. For example, you can use [**midi2midi**](https://schollz.github.io/midi2midi/) in a Chrome browser to forward MIDI messages from a MIDI controller to the zeptocore.

Using MIDI you can also easily sync up with Ableton or other DAWs.

<div class="plyr__video-embed" id="player">
  <iframe
    src="https://www.youtube.com/embed/JMmxfW2QY3Y?origin=https://plyr.io&amp;iv_load_policy=3&amp;modestbranding=1&amp;playsinline=1&amp;showinfo=0&amp;rel=0&amp;enablejsapi=1"
    allowfullscreen
    allowtransparency
    allow="autoplay"
  ></iframe>
</div>


### Itty Bitty MIDI

I created a device called [**the itty bitty midi**](https://ittybittymidi.com) which you can use directly with a MIDI controller to send MIDI messages to the zeptocore. Simply plug in the MIDI controller to the "IN" side of the itty bitty midi and then plug the "OUT" side to the `CLOCK` input of the zeptocore. The itty bitty midi will automatically forward MIDI messages to the zeptocore.

*Note*: By default the zeptocore expects clock signals in the `CLOCK` input. You can switch to `MIDI` input by pressing the following combination: `13`+`10`+`11`+`16`. More information [here](#midi-with-ittybittymidi).

<figure class="imgcombo">
<img src="/img/combo_midi.webp">
<figcaption>Combo for activating itty bitty midi MIDI input through the CLOCK in</figcaption>
</figure>

## Computer keyboard

The MIDI layer allows you to communicate with the zeptocore via a browser and computer keyboard. Simply open a browser (*Note: Chrome only*) to [zeptocore.com](https://zeptocore.com) and plug in the zeptocore to the computer. 

If successful, a small screen will pop up that lets you see the current state of the zeptocore and allows you to interact with the zeptocore directly via the keyboard:

<p style="max-width:80%; margin: auto;">
<img src="/img/zeptoboard_midi.webp" style="width:100%;">
</p>