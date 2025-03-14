+++
title = 'Kick layering'
date = 2024-02-01T12:33:06-08:00
short = 'Layer kick drums with preloaded samples'
buttons = ['A','D','1-16']
buttons_alt = ['13','10','7','16']
weight = 89
mode = 'any'
mode_alt = 'any'
icon = 'drum'
+++

*Note: v6.3.6 and later firmware required for this feature.*

This is an experimental mode that lets you choose from 16 different kick samples (keys `1` to `16`). Hold down the `A` button and then hold down the `D` button and then press one of the 16 keys to layer the selected kick sample with the currently loaded sample. Continue to hold down the `A` and `D` buttons and press another of the 16 keys to set the volume of the selected kick sample. 

You can turn this mode off by holding down the `A` button and then holding down the `D` button and then pressing the `1` key twice. 

This mode can also be toggled using the button combo `13`, `10`, `7`, `16`.


When activated, the selected kick sample will be played at the same time as the detected kick drums in the sample. This can be useful for adding more punch to your kick drums. 


## Kick detection in web tool

Use the web tool in *v4+* to generate information for the placement of kick samples. Determined placements are shown with a small kick drum icon "<svg style="display:inline-block; width:15px" width="100%" height="100%" viewBox="0 0 20 19" version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" xml:space="preserve" xmlns:serif="http://www.serif.com/" style="fill-rule:evenodd;clip-rule:evenodd;stroke-linecap:round;stroke-linejoin:round;stroke-miterlimit:1.5;">
    <g transform="matrix(1,0,0,1,-24.5,-564.5)">
        <g>
            <g transform="matrix(1,0,0,0.957143,-1,23.2786)">
                <ellipse cx="35.25" cy="575.25" rx="8.25" ry="8.75" style="fill:none;stroke:black;stroke-width:2.04px;"></ellipse>
            </g>
            <g transform="matrix(1,0,0,1.33333,0,-191.5)">
                <ellipse cx="34.5" cy="575.25" rx="1" ry="0.75" style="fill:none;stroke:black;stroke-width:1.7px;"></ellipse>
            </g>
            <g transform="matrix(1,0,0,0.857143,0,82.2143)">
                <path d="M34.5,575.5L34.5,582.5" style="fill:none;stroke:black;stroke-width:2.15px;"></path>
            </g>
            <path d="M25.5,581.5L28,579.5" style="fill:none;stroke:black;stroke-width:2px;"></path>
            <path d="M43.5,581.5L40.5,579.5" style="fill:none;stroke:black;stroke-width:2px;"></path>
        </g>
    </g>
</svg>" placed at the position of each estimated kick.

<figure class="imgcombo">
<img src="/img/kicks.webp">
<figcaption>In this sample, there were four detected locations for kick samples.</figcaption>
</figure>

## Kick detection

Unlike the slices which are manually set or defined by intervals, the kicks (and snares too) are detected using a machine learning approach. The detection is based of an ML model of drums and placement of kicks and snares. An original audio file of drums can be separated into just kick drums:

Original audio:

<audio src="/wave/amen_beats8_bpm172.mp3" class="waveform"></audio>

Kick stem:

<audio src="/wave/bombo.mp3" class="waveform"></audio>


The transients in the kick stem are used to calculate the best placement for kick drums.

<!-- Snare stem:

<audio src="/wave/redoblante.mp3" class="waveform"></audio> -->




<script src="/wave/wavesurfer.js"></script>
<script src="/wave/waveform.js"></script>
<!-- 
The stems are used to determine the position of kicks or snares within a piece of audio. The model if far from perfect, but it is surprsingly good.  -->


<div class="plyr__video-embed" id="player">
  <iframe
    src="https://www.youtube.com/embed/313Va6h9Ldc?origin=https://plyr.io&amp;iv_load_policy=3&amp;modestbranding=1&amp;playsinline=1&amp;showinfo=0&amp;rel=0&amp;enablejsapi=1"
    allowfullscreen
    allowtransparency
    allow="autoplay"
  ></iframe>
</div>