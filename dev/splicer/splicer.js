// Regions plugin

import RegionsPlugin from 'https://unpkg.com/wavesurfer.js@7/dist/plugins/regions.esm.js'
import WaveSurfer from 'https://unpkg.com/wavesurfer.js@7/dist/wavesurfer.esm.js'

// Create an instance of WaveSurfer
const ws = WaveSurfer.create({
    container: '#waveform',
    waveColor: 'rgb(200, 0, 200)',
    progressColor: 'rgb(100, 0, 100)',
    url: '/amen.wav',
    height: 300,
})
// Initialize the Regions plugin
const wsRegions = ws.registerPlugin(RegionsPlugin.create())

// Give regions a random color when they are created
const random = (min, max) => Math.random() * (max - min) + min
const randomColor = () =>
    `rgba(${random(0, 255)}, ${random(0, 255)}, ${random(0, 255)}, 0.5)`

// Create some regions at specific time ranges
ws.on('decode', () => {
    // Regions
    for (var i = 0; i < 32; i++) {
        wsRegions.addRegion({
            start: 0.1765 * i,
            end: 0.1765 * (i + 1),
            color: `rgba(200,200,200,0.5)`,
            drag: true,
            resize: true,
        });
    }
})

wsRegions.enableDragSelection({
    color: 'rgba(255, 0, 0, 0.1)',
})

wsRegions.on(
    'region-updated', (region) => { console.log('Updated region', region) })

// Loop a region on click
let loop = true
// Toggle looping with a checkbox
document.querySelector('input[type="checkbox"]').onclick =
    (e) => {
        loop = e.target.checked
    }

{
    let activeRegion = null
    wsRegions.on('region-in', (region) => { activeRegion = region })
    wsRegions.on('region-out', (region) => {
        if (activeRegion === region) {
            if (loop) {
                ws.pause()
            } else {
                activeRegion = null
            }
        }
    })
    var playing = false;
    wsRegions.on('region-clicked', (region, e) => {
        e.stopPropagation() // prevent triggering a click on the waveform
        activeRegion = region;
        if (playing) {
            console.log('pausing');
            ws.pause();
            playing = false;
        } else {
            region.play();
            playing = true;
        }
        // region.setOptions({ color: randomColor() })
    })
    // Reset the active region when the user clicks anywhere in the waveform
    ws.on('interaction', () => { activeRegion = null })
}

// Update the zoom level on slider change
ws.once('decode', () => {
    document.querySelector('input[type="range"]').oninput = (e) => {
        const minPxPerSec = Number(e.target.value)
        ws.zoom(minPxPerSec)
    };
});