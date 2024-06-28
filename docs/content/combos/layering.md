+++
title = 'Kick layering'
date = 2024-02-01T12:33:06-08:00
short = 'Layer kick drums with preloaded samples'
buttons = ['A','D','1-16']
weight = 89
mode = 'any'
icon = 'drum'
+++

*Note: v4.0.0 and later firmware required for this feature.*

This is an experimental mode that lets you choose from 15 different kick samples (keys `1` to `15`). Hold down the `A` button and then hold down the `D` button and then press one of the 15 keys to layer the selected kick sample with the currently loaded sample. The selected kick sample will be played at the same time as the detected kick drums in the sample. This can be useful for adding more punch to your kick drums. 

To exit this mode, hold down the `A` button and then hold down the `D` button and then press the `16` key.

## Kick detection

Unlike the slices which are manually set or defined by intervals, the kicks (and snares too) are detected using a machine learning approach. The detection is based of an ML model of drums and placement of kicks and snares. An original audio file of drums can be separated into kicks and snares:

Original audio:

<audio src="/wave/amen_beats8_bpm172.mp3" class="waveform"></audio>

Kick stem:

<audio src="/wave/bombo.mp3" class="waveform"></audio>

Snare stem:

<audio src="/wave/redoblante.mp3" class="waveform"></audio>
<script src="/wave/wavesurfer.js"></script>
<script src="/wave/waveform.js"></script>

The stems are used to determine the position of kicks or snares within a piece of audio. The model if far from perfect, but it is surprsingly good. 