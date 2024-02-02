+++
title = 'Repeat'
date = 2024-02-01T12:33:06-08:00
short = 'Repeat has one knob to control the repeat size.'
buttons = ['8']
buttons_alt = ['A','8']
weight = 8
mode = 'mash'
mode_alt = 'jump'
knobx = 'duration'
icon = 'redo-alt'
+++


The repeat will find zero-crossings and loop the last segment of audio that matches best to the requested duration. This is slightly different than the [delay](#delay) because it will overtake the audio instead of add to it. It can go quite fast to audio rate.

