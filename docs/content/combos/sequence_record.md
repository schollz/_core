+++
title = 'Record sequence'
date = 2024-02-01T12:33:06-08:00
short = 'Toggle sequence recording for current mode'
buttons = ['C','D']
weight = 30
icon = 'pen'
mode = 'any'
+++

This key combination will toggle sequence recording. In the [JUMP mode](#mode-jump), it will record a sequence of jumps. In the [MASH mode](#mode-mash), it will record a sequence of toggling effects. In the [BASS mode](#mode-bass), it will record a bass sequence.

<figure class="imgcombo">
<img src="/img/sequence_rec.webp">
<figcaption>Combo for recording/deleting a sequence slot.</figcaption>
</figure>


Recording a sequence on Zeptocore differs slightly from other devices. The key distinction is that Zeptocore records *the time between presses*. Therefore, to record a sequence of *4* steps, you will need to make *5* presses. The final press will not be counted as a sequenced step, but it is necessary to determine the duration of the penultimate press.

It is important to note that Zeptocore *does not have a concept of time signature*. All recorded actions are quantized to 1/96th of a beat, ensuring precise preservation of the recorded press timing. For quantizing the press timing with less resolution, you can utilize the [C + Knob Z](#quantize) key combination.

Please be aware that the button combination of `C` + `D` also functions to delete a sequence currently stored in the selected slot.

