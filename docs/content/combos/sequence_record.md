+++
title = 'Record sequence'
date = 2024-02-01T12:33:06-08:00
short = 'Toggle sequence recording for current mode'
buttons = ['C','D']
weight = 30
icon = 'pen'
mode = 'any'
+++

This combo will toggle sequence recording. If you are in the [JUMP mode](#mode-jump) then this will record a sequence of jumps. If you are in the [MASH mode](#mode-mash) then this will record a sequence of toggling effects. If you are in the [BASS mode](#mode-bass) then it will record a bass sequence.

Recording a sequence on a zeptocore is a little different than other devices. The main difference is that the zeptocore records *the time between presses*. This means, to record a sequence of *4* steps you will need to make *5* presses. The final press will not count as a sequenced step, but it is necessary to provide the duration of the penultimate press. 

Also, its worth keeping in mind that the zeptocore *has no concept of time-signature*. Everything that is recorded is quantized to 1/96th of a beat, which helps to preserve almost exactly the press timing that is recorded. The press timing can be quantized with less resolution by [using the `C` + `Knob Z`](#quantize) combo. 
