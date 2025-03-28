// to be rendered as a waveform player
// add your audio in the form:
//
// <div data-src="audio.wav" class="audiowaveform"></div>
//
document.addEventListener('DOMContentLoaded', function () { // var els = document.getElementsByClassName("audiowaveform");
    var els = document.querySelectorAll("audio.waveform");
    for (var i = 0; i < els.length; i++) {
        console.log(els[i].src);
        let i_ = i;
        let src_ = els[i].src;
        let newNode = document.createElement("div")
        newNode.innerHTML = `<table style="padding-top:16px;padding-bottom:16px;"><tr><td style="position: relative;top: 4px;width:40px;"><div class="controls" style=""> <button class="audiobtn" data-action="play" style="background: inherit; border: none; padding-left:0;padding-right:0;"> <svg class="playicon" xmlns="http://www.w3.org/2000/svg" width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1" stroke-linecap="round" stroke-linejoin="round" class="feather feather-play-circle"><circle cx="12" cy="12" r="10"></circle><polygon points="10 8 16 12 10 16 10 8"></polygon></svg> <svg class="pauseicon" style="display:none;" xmlns="http://www.w3.org/2000/svg" width="40" height="40" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1" stroke-linecap="round" stroke-linejoin="round" class="feather feather-pause-circle"><circle cx="12" cy="12" r="10"></circle><line x1="10" y1="15" x2="10" y2="9"></line><line x1="14" y1="15" x2="14" y2="9"></line></svg> </button></div></td><td width="100%"><div class="wave"></div></td></tr></table>`;
        els[i_].parentNode.insertBefore(newNode, els[i_])
        els[i].remove();
        let wavesurfer = WaveSurfer.create({
            container: document.getElementsByClassName("wave")[i_],
            waveColor: '#909090',
            progressColor: '#443e3c',
            cursorColor: '#443e3c',
            cursorWidth: 2,
            backend: 'MediaElement',
            mediaControls: false,
            hideScrollbar: true,
            minPxPerSec: 60,
            normalize: true,
            height: 128,
        });
        wavesurfer.once('ready', function () {
            console.log('Using wavesurfer.js ' + WaveSurfer.VERSION);
        });
        wavesurfer.on('error', function (e) {
            console.warn(e);
        });
        wavesurfer.on('play', function (e) {
            document.getElementsByClassName("playicon")[i_].style.display = "none";
            document.getElementsByClassName("pauseicon")[i_].style.display = "block";
        });
        wavesurfer.on('pause', function (e) {
            document.getElementsByClassName("playicon")[i_].style.display = "block";
            document.getElementsByClassName("pauseicon")[i_].style.display = "none";
        });
        newNode.querySelector('[data-action="play"]')
            .addEventListener('click', wavesurfer.playPause.bind(wavesurfer));
        fetch(src_.replace(/\.[^/.]+$/, "") + ".json")
            .then(response => {
                if (!response.ok) {
                    throw new Error("HTTP error " + response.status);
                }
                return response.json();
            })
            .then(peaks => {
                console.log('loaded peaks! sample_rate: ' + peaks.sample_rate);

                // load peaks into wavesurfer.js
                wavesurfer.load(src_, peaks.data, 'metadata');
            })
            .catch((e) => {
                console.error('error', e);
            });
        //wavesurfer.load(src_);
    }
});