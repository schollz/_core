var wsf = null;
var wsRegions = null;
var totalBytesUploaded = 0;
var totalBytesRequested = 0;
var activeRegion = null;
var app;
var socket;
var serverID = "";
var randomID = Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15);
var fadeOutTimeout = null;
var disconnectedTimeout = null;
// get color from `:root` css
var ccolor, ccolor2, wavecolor, selected_color;
var hasSavedToCookie = false;


function GetLatestReleaseInfo() {
    fetch("https://zeptocore.com/get_info")
        .then(response => response.json())
        .then(json => {
            app.latestVersion = json.version;
            console.log(`[GetLatestReleaseInfo] Latest version: ${app.latestVersion}`)
        })
        .catch(error => console.error('Error fetching release:', error));
}

var inputMidiDevice = null;
var outputMidiDevice = null;
var checkMidiInterval = null;


function midiStartup() {
    app.midiIsSetup = true;
    midiGetVersion();
}
function listMidiPorts() {
    if (!navigator.requestMIDIAccess) {
        console.log('Web MIDI API is not supported in this browser.');
        return;
    }

    navigator.requestMIDIAccess({ sysex: true }) // Enable Sysex messages
        .then(midiAccess => {
            midiAccess.inputs.forEach(input => {
                if (input.name.toLowerCase().includes("zeptocore") || input.name.toLowerCase().includes("ectocore")) {
                    inputMidiDevice = input; // Ensure global scope if needed
                    console.log(`Selected input MIDI device: ${input.name}`);
                    setupMidiInputListener();
                    if (outputMidiDevice) {
                        midiStartup();
                    }
                }
            });

            midiAccess.outputs.forEach(output => {
                if (output.name.toLowerCase().includes("zeptocore") || output.name.toLowerCase().includes("ectocore")) {
                    outputMidiDevice = output; // Ensure global scope if needed
                    console.log(`Selected output MIDI device: ${output.name}`);
                    if (inputMidiDevice) {
                        midiStartup();
                    }
                }
            });

        })
        .catch(error => {
            // console.error('Error accessing MIDI devices:', error);
        });
}

function addToMidiConsole(message) {
    var consoleElement = document.getElementById('consoleprint');
    var scrollableElement = document.getElementById('scrollable-content');

    // Check if the scrollbar is at the bottom before adding new content
    var isScrolledToBottom = scrollableElement.scrollHeight - scrollableElement.clientHeight <= scrollableElement.scrollTop + 1; // +1 for rounding tolerance

    consoleElement.innerHTML += `<br>${message}`;

    // If it was at the bottom, scroll to the new bottom
    if (isScrolledToBottom) {
        scrollableElement.scrollTop = scrollableElement.scrollHeight;
    }
}

function setupMidiInputListener() {
    if (window.inputMidiDevice) {
        window.inputMidiDevice.onmidimessage = (midiMessage) => {
            // check if sysex
            if (midiMessage.data[0] == 0xf0) {
                // convert the sysex to string 
                var sysex = "";
                for (var i = 1; i < midiMessage.data.length - 1; i++) {
                    sysex += String.fromCharCode(midiMessage.data[i]);
                }
                addToMidiConsole(sysex);
                // see if it starts with verion=
                if (sysex.startsWith("version=")) {
                    console.log(sysex);
                    app.deviceVersion = sysex.split("=")[1];
                    console.log(`[setupMidiInputListener] Device version: ${app.deviceVersion}`)
                }
            } else if (midiMessage.data[0] != 0xf8) {
                addToMidiConsole(midiMessage.data);
            }
        };
        // receive SySex messages
        window.inputMidiDevice.onstatechange = (event) => {
            console.log('MIDI device state changed:', event.port.name, event.port.state);
            if (event.port.state === 'disconnected') {
                inputMidiDevice = null;
                app.midiIsSetup = false;
            }
        };
    }
}

function midiGetVersion() {
    sendToOutputMidiDevice([0xB0, 1, 0]);
}

function midiResetDevice() {
    sendToOutputMidiDevice([0xB0, 0, 0]);
}

function sendToOutputMidiDevice(data) {
    if (window.outputMidiDevice && data) {
        console.log("sending ", data);
        window.outputMidiDevice.send(data);
    }
}

// sendToOutputMidiDevice([0x90, 60, 100]); // Example MIDI note-on message

function formatBytes(bytes, decimals) {
    if (bytes == 0) return '0 Bytes';
    var k = 1024,
        dm = decimals || 2,
        sizes = ['Bytes', 'KB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'],
        i = Math.floor(Math.log(bytes) / Math.log(k));
    return parseFloat((bytes / Math.pow(k, i)).toFixed(dm)) + ' ' + sizes[i];
}

function fadeInCircle() {
    var circle = document.getElementById('fadeCircle');
    circle.style.opacity = '1';
}

function fadeOutCircle() {
    var circle = document.getElementById('fadeCircle');
    circle.style.opacity = '0';
}


const debounce = (callback, wait) => {
    let timeoutId = null;
    return (...args) => {
        window.clearTimeout(timeoutId);
        timeoutId = window.setTimeout(() => {
            callback(...args);
        }, wait);
    };
}

const fadeOut = debounce(function () {
    fadeOutCircle();
}, 1000);

function fadeInProgressbar() {
    console.log('fadeInProgressbar');
    var circle = document.getElementsByClassName('progress-bar')[0];
    circle.style.opacity = '1';
}

function fadeOutProgressbar() {
    console.log('fadeOutProgressbar');
    var circle = document.getElementsByClassName('progress-bar')[0];
    circle.style.opacity = '0';
}


// Function to read cookies
function readCookie(name) {
    const match = document.cookie.match(new RegExp('(^| )' + name + '=([^;]+)'));
    return match ? match[2] : null;
}

// Function to write cookies
function writeCookie(name, value, sameSite = 'Lax') {
    let cookieText = `${name}=${value}; path=/`;
    if (sameSite === 'None') {
        cookieText += '; SameSite=None; Secure';
    } else {
        cookieText += `; SameSite=${sameSite}`;
    }
    document.cookie = cookieText;
}


// Function to load previousPages from cookies
function loadPreviousPages() {
    const previousPagesString = readCookie('previousPages');
    let previousPages = previousPagesString ? JSON.parse(previousPagesString) : [];
    return Array.from(new Set(previousPages));
}

function saveCurrentPage() {
    let currentAddress = window.location.pathname === '/' ? '' : window.location.pathname;
    // remove preceding slash in current address
    currentAddress = currentAddress.replace(/^\//, '');
    if (currentAddress) {
        let previousPages = loadPreviousPages();
        previousPages.push(currentAddress);
        previousPages = Array.from(new Set(previousPages));
        writeCookie('previousPages', JSON.stringify(previousPages));
    }
}

function generateRandomWord() {
    const minLength = 5;
    const maxLength = 10;
    const vowels = 'aeiou';
    const consonants = 'bcdfghjklmnpqrstvwxyz';

    // Generate a random length between minLength and maxLength
    const length = Math.floor(Math.random() * (maxLength - minLength + 1)) + minLength;

    let word = '';

    for (let i = 0; i < length; i++) {
        // Alternate between consonants and vowels
        if (i % 2 === 0) {
            word += consonants[Math.floor(Math.random() * consonants.length)];
        } else {
            word += vowels[Math.floor(Math.random() * vowels.length)];
        }
    }

    return word;
}

const socketMessageListener = (e) => {
    data = JSON.parse(e.data);
    // console.log("socketMessageListener", data.action)
    if (data.action == "processed") {
        console.log("processed");
        for (var i = 0; i < data.file.SliceStart.length; i++) {
            data.file.SliceStart[i] = parseFloat(data.file.SliceStart[i].toFixed(3));
            data.file.SliceStop[i] = parseFloat(data.file.SliceStop[i].toFixed(3));
        }
        // append data.file to bank
        app.banks[app.selectedBank].files.push(data.file);
    } else if (data.action == "firmwareuploaded") {
        if (!data.success) {
            app.error_message = data.message;
        }
    } else if (data.action == "firmwareprogress") {
        app.deviceFirmwareUpload = data.deviceFirmwareUpload;
    } else if (data.action == "devicefound") {
        console.log(data);
        if (data.deviceType != "") {
            app.deviceFirmwareUpload = "";
            app.deviceType = data.deviceType;
            app.deviceVersion = data.deviceVersion;
            app.latestVersion = data.latestVersion;
        }
    } else if (data.action == "copyworkspace") {
        if (data.error != "") {
            app.error_message = data.error;
        } else {
            window.location = "/" + data.message;
        }
        console.log(data);
    } else if (data.action == "onsetdetect") {
        console.log(data);
        if (wsf != null) {
            app.deleteAllRegions();
            const duration = wsf.getDuration();
            for (var i = 1; i < data.sliceStart.length; i++) {
                wsRegions.addRegion({
                    start: data.sliceStart[i - 1],
                    end: data.sliceStart[i],
                    color: ccolor,
                    drag: true,
                    resize: true,
                    loop: false,
                });
            }
            wsRegions.addRegion({
                start: data.sliceStart[data.sliceStart.length - 1],
                end: duration,
                color: ccolor,
                drag: true,
                resize: true,
                loop: false,
            });
            app.drawTransients();
            updateAllRegions();
        }
    } else if (data.action == "getstate") {
        var savedState = JSON.parse(data.state);
        if (savedState.banks) {
            app.banks = savedState.banks;
        }
        if (savedState.resampling) {
            app.resampling = savedState.resampling;
        }
        if (savedState.settingsBrightness) {
            app.settingsBrightness = savedState.settingsBrightness;
        }
        if (savedState.settingsClockStop) {
            app.settingsClockStop = savedState.settingsClockStop;
        }
        if (savedState.settingsGrimoireEffects) {
            app.settingsGrimoireEffects = savedState.settingsGrimoireEffects;
        }
        if (savedState.settingsClockOutput) {
            app.settingsClockOutput = savedState.settingsClockOutput;
        }
        if (savedState.settingsClockBehaviorSync) {
            app.settingsClockBehaviorSync = savedState.settingsClockBehaviorSync;
        }
        if (savedState.settingsKnobXSample) {
            app.settingsKnobXSample = savedState.settingsKnobXSample;
        }
        if (savedState.oversampling) {
            app.oversampling = savedState.oversampling;
        }
        if (savedState.stereoMono) {
            app.stereoMono = savedState.stereoMono;
        }
        if (savedState.selectedBank !== undefined) {
            app.selectedBank = savedState.selectedBank;
        }
        if (savedState.selectedFile !== undefined) {
            app.selectedFile = savedState.selectedFile;
        }
        if (app.selectedFile != null) {
            setTimeout(() => {
                showWaveform(app.banks[app.selectedBank].files[app.selectedFile].PathToFile,
                    app.banks[app.selectedBank].files[app.selectedFile].Duration,
                    app.banks[app.selectedBank].files[app.selectedFile].SliceStart,
                    app.banks[app.selectedBank].files[app.selectedFile].SliceStop,
                    app.banks[app.selectedBank].files[app.selectedFile].SliceType,
                    app.banks[app.selectedBank].files[app.selectedFile].Transients,
                );
            }, 100);
        }
    } else if (data.action == "connected") {
        if (serverID == "" || serverID == data.message) {
            serverID = data.message;
        } else {
            // do hot reload to get the latest
            location.reload();
        }
    } else if (data.action == "processingstart") {
        app.uploading = false;
        app.processing = true;
        app.downloading = false;
    } else if (data.action == "processingstop") {
        app.uploading = false;
        app.processing = false;
        app.downloading = true;
    } else if (data.action == "isworking") {
        app.isworking = true;
    } else if (data.action == "notworking") {
        app.isworking = false;
    } else if (data.action == "transients") {
        console.log(data)
        app.banks[data.bankNum].files[data.fileNum].Transients = data.transients;
        setTimeout(() => {
            showWaveform(app.banks[app.selectedBank].files[app.selectedFile].PathToFile,
                app.banks[app.selectedBank].files[app.selectedFile].Duration,
                app.banks[app.selectedBank].files[app.selectedFile].SliceStart,
                app.banks[app.selectedBank].files[app.selectedFile].SliceStop,
                app.banks[app.selectedBank].files[app.selectedFile].SliceType,
                app.banks[app.selectedBank].files[app.selectedFile].Transients,
            );
        }, 100);
    } else if (data.action == "progress") {
        totalBytesUploaded = data.number;
        var maxWidth = window.innerWidth;
        app.progressBarWidth = `${Math.floor(totalBytesUploaded / totalBytesRequested * maxWidth)}px`;
        console.log(`bytes uploaded: ${totalBytesUploaded}/${totalBytesRequested}`);
        if (totalBytesUploaded >= totalBytesRequested) {
            app.progressBarWidth = `${maxWidth}px`;
            fadeOutProgressbar();
            fadeOutTimeout = setTimeout(() => {
                app.progressBarWidth = '0px';
            }, 5000);
        } else {
            var circle = document.getElementsByClassName('progress-bar')[0];
            circle.style.opacity = '1';
        }
    } else {
        if (data.error != "") {
            app.error_message = data.error;
            // strip white space from the front and back:
            app.error_message = app.error_message.trim();
            // remove the error message after 30 seconds
            setTimeout(() => {
                app.error_message = "";
            }, 30000);
        } else {
            console.log(`unknown action: ${data.action}`);
        }
    }
};
var isProcesingInterval;
const socketOpenListener = (e) => {
    console.log('Connected');
    if (disconnectedTimeout != null) {
        clearTimeout(disconnectedTimeout);
    }
    app.disconnected = false;
    setTimeout(() => {
        if (socket != null) {
            socket.send(JSON.stringify({
                action: "getstate",
                place: window.location.pathname,
            }));
        }
    }, 50);
    // check if processing 
    clearInterval(isProcesingInterval);
    isProcesingInterval = setInterval(() => {
        if (socket != null) {
            socket.send(JSON.stringify({
                action: "isprocessing",
                place: window.location.pathname,
            }));
        }
    }, 1000);
    try {
        socket.send(JSON.stringify({
            action: "connected"
        }));
    } catch (error) {
        // oh well
    }
};
const socketErrorListener = (e) => {
    // console.error(e);
}
const socketCloseListener = (e) => {
    if (socket) {
        console.log('Disconnected.');
        disconnectedTimeout = setTimeout(() => {
            app.disconnected = true;
        }, 100);
    }
    var url = window.origin.replace("http", "ws") + '/ws?id=' + randomID + "&place=" + window.location.pathname;
    socket = new WebSocket(url);
    socket.onopen = socketOpenListener;
    socket.onmessage = socketMessageListener;
    socket.onclose = socketCloseListener;
    socket.onerror = socketErrorListener;
};
window.addEventListener('load', (event) => {
    // Load previousPages from cookies into the app
    app.previousPages = loadPreviousPages();

    socketCloseListener();
});

const updateAllRegions = () => {
    let regions = wsf.plugins[0].regions;
    regions.sort((a, b) => (a.start > b.start) ? 1 : -1);
    let sliceStart = [];
    let sliceStop = [];
    for (var i = 0; i < regions.length; i++) {
        // filter out transients
        if (regions[i].id.startsWith('transient-')) {
            continue;
        }

        // console.log(`updateAllRegions: ${i} ${regions[i].start} ${regions[i].end}`);
        if (sliceStart.length == 0 || regions[i].start > sliceStart[sliceStart.length - 1]) {
            // console.log(`updateAllRegions: added`);
            sliceStart.push(parseFloat((regions[i].start / wsf.getDuration()).toFixed(3)));
            sliceStop.push(parseFloat((regions[i].end / wsf.getDuration()).toFixed(3)));
        }
    }
    // console.log(`updateAllRegions: ${sliceStart.length} slices`);

    // update locally
    app.banks[app.selectedBank].files[app.selectedFile].SliceStart = sliceStart;
    app.banks[app.selectedBank].files[app.selectedFile].SliceStop = sliceStop;

    // update remotely
    socket.send(JSON.stringify({
        action: "setslices",
        filename: app.banks[app.selectedBank].files[app.selectedFile].Filename,
        sliceStart: sliceStart,
        sliceStop: sliceStop,
    }));
}

app = new Vue({
    el: '#app',
    data: {
        midiIsSetup: false,
        slicesPerBeat: 1,
        banks: Array.from({ length: 16 }, () => ({ files: [], lastSelectedFile: null })), // Add the lastSelectedFile property
        selectedBank: 0,
        selectedFile: null,
        deviceType: "",
        transientDoAdd: [false, false, false],
        latestVersion: "",
        deviceVersion: "",
        deviceFirmwareUpload: "",
        lastSelectedFile: null,
        settingsBrightness: 50,
        settingsClockStop: false,
        settingsClockOutput: false,
        settingsClockBehaviorSync: false,
        settingsKnobXSample: false,
        progressBarWidth: '0px',
        dropaudiofilemode: 'default',
        oversampling: '1x', // Default to '1x'
        stereoMono: 'mono', // Default to 'mono'
        isMobile: false, // Define isMobile variable
        playingSample: false,
        downloading: false,
        showCookiePolicy: false,
        processing: false,
        isworking: false,
        error_message: "",
        regular_message: "",
        uploading: false,
        resampling: 'linear',
        titleName: "_core",
        title: window.location.pathname == '/' ? "" : window.location.pathname,
        disconnected: false,
        previousPages: [],
        randomPages: [generateRandomWord(), generateRandomWord(), generateRandomWord()],
        selectedFiles: [], // New property to store selected files
        grimoireSelected: 0,
        grimoireKnobDegrees: 0,
        settingsGrimoireEffects: [
            // fuzz, loss,  bitcr, filte, times, delay, combf, repea, rever, panni, slowd, speed, rever, retri, retri, stop 
            [true, true, true, true, false, false, false, false, false, false, false, false, false, false, false, false],
            [false, false, false, false, false, true, false, false, false, false, false, false, true, false, false, false],
            [false, false, false, false, true, true, false, false, true, true, false, false, false, false, false, false],
            [false, false, false, false, false, true, true, false, true, false, false, false, true, true, false, false],
            [false, false, false, false, false, false, false, false, false, false, true, true, true, false, true, true],
            [false, false, false, false, true, true, false, false, false, true, false, false, false, false, false, false],
            [true, false, true, false, true, true, true, true, false, true, false, false, true, true, true, false],
        ],
        regionClickBehavior: 'clickCreateRegion',
    },
    watch: {
        // Watch for changes in app properties and save state to cookies
        banks: {
            handler: 'saveState',
            deep: true,
        },
        oversampling: 'saveState',
        stereoMono: 'saveState',
        resampling: 'saveState',
        settingsBrightness: 'saveState',
        settingsClockStop: 'saveState',
        settingsClockOutput: 'saveState',
        settingsClockBehaviorSync: 'saveState',
        settingsGrimoireEffects: 'saveState',
        settingsKnobXSample: 'saveState',
        selectedFile: 'saveState',
        selectedBank: 'saveState',
        selectedFile: 'saveLastSelected',
    },
    computed: {

        diskUsage: function () {
            // loop through all banks
            var total = 0;
            for (var i = 0; i < this.banks.length; i++) {
                // loop through all files in the bank
                for (var j = 0; j < this.banks[i].files.length; j++) {
                    total += this.banks[i].files[j].Duration * 44100 * 2 * 10 * (1 + this.banks[i].files[j].Channels);
                }
            }
            return total;
        }
    },
    methods: {
        waveformClick(seconds) {
            console.log(`waveform clicked at ${seconds} (${this.regionClickBehavior})`);
            if (this.regionClickBehavior == "clickCreateKick") {
                this.addTransient(0, seconds);
            } else if (this.regionClickBehavior == "clickCreateSnare") {
                this.addTransient(1, seconds);
            } else if (this.regionClickBehavior == "clickCreateTransient") {
                this.addTransient(2, seconds);
            }
        },
        addTransient(j, seconds) {
            // TODO
            for (var i = 0; i < this.banks[this.selectedBank].files[this.selectedFile].Transients[j].length; i++) {
                if (this.banks[this.selectedBank].files[this.selectedFile].Transients[j][i] == 0) {
                    this.banks[this.selectedBank].files[this.selectedFile].Transients[j][i] = Math.round(seconds * 44100.0);
                    break;
                }
            }
            this.drawTransients();
        },
        clearTransients(j) {
            for (var i = 0; i < this.banks[this.selectedBank].files[this.selectedFile].Transients[j].length; i++) {
                this.banks[this.selectedBank].files[this.selectedFile].Transients[j][i] = 0;
            }
            this.drawTransients();
        },
        clearTransientsLast(j) {
            for (var i = 0; i < this.banks[this.selectedBank].files[this.selectedFile].Transients[j].length; i++) {
                if (this.banks[this.selectedBank].files[this.selectedFile].Transients[j][i] == 0) {
                    if (i > 0) {
                        this.banks[this.selectedBank].files[this.selectedFile].Transients[j][i - 1] = 0;
                    }
                    break;
                }
            }
            this.drawTransients();
        },
        deleteTransients() {
            if (this.regionClickBehavior == 'clickCreateKick') {
                this.clearTransients(0);
            } else if (this.regionClickBehavior == 'clickCreateSnare') {
                this.clearTransients(1);
            } else if (this.regionClickBehavior == 'clickCreateTransient') {
                this.clearTransients(2);
            }
        },
        deleteTransientsLast() {
            if (this.regionClickBehavior == 'clickCreateKick') {
                this.clearTransientsLast(0);
            } else if (this.regionClickBehavior == 'clickCreateSnare') {
                this.clearTransientsLast(1);
            } else if (this.regionClickBehavior == 'clickCreateTransient') {
                this.clearTransientsLast(2);
            }
        },
        drawTransients() {
            // remove all transients by iterating over all wsRegions
            var hasTransients = true;
            while (hasTransients) {
                hasTransients = false;
                for (var i = 0; i < wsRegions.regions.length; i++) {
                    if (wsRegions.regions[i].id.startsWith('transient-')) {
                        console.log(`removing ${wsRegions.regions[i].id}`);
                        wsRegions.regions[i].remove();
                        hasTransients = true;
                        break;
                    }
                }
            }

            let transients = this.banks[this.selectedBank].files[this.selectedFile].Transients;
            let transient_colors = ['rgb(255,0,0,0.25)', 'rgb(0,255,0,0.25)', 'rgb(0,0,255,0.25)'];
            let span = ` <span class="regionsvg" style="display: flex;
    justify-content: center;
    align-items: center;
    min-width: 20px; position:relative; top:5px;" >`;
            let transient_content = [`
                 <svg width="100%" height="100%" viewBox="0 0 20 19" version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" xml:space="preserve" xmlns:serif="http://www.serif.com/" style="fill-rule:evenodd;clip-rule:evenodd;stroke-linecap:round;stroke-linejoin:round;stroke-miterlimit:1.5;">
    <g transform="matrix(1,0,0,1,-24.5,-564.5)">
        <g>
            <g transform="matrix(1,0,0,0.957143,-1,23.2786)">
                <ellipse cx="35.25" cy="575.25" rx="8.25" ry="8.75" style="fill:none;stroke:black;stroke-width:2.04px;"/>
            </g>
            <g transform="matrix(1,0,0,1.33333,0,-191.5)">
                <ellipse cx="34.5" cy="575.25" rx="1" ry="0.75" style="fill:none;stroke:black;stroke-width:1.7px;"/>
            </g>
            <g transform="matrix(1,0,0,0.857143,0,82.2143)">
                <path d="M34.5,575.5L34.5,582.5" style="fill:none;stroke:black;stroke-width:2.15px;"/>
            </g>
            <path d="M25.5,581.5L28,579.5" style="fill:none;stroke:black;stroke-width:2px;"/>
            <path d="M43.5,581.5L40.5,579.5" style="fill:none;stroke:black;stroke-width:2px;"/>
        </g>
    </g>
</svg></span>`, `<svg width="100%" height="100%" viewBox="0 0 20 19" version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" xml:space="preserve" xmlns:serif="http://www.serif.com/" style="fill-rule:evenodd;clip-rule:evenodd;stroke-linecap:round;stroke-linejoin:round;stroke-miterlimit:1.5;">
    <g transform="matrix(1,0,0,1,-57,-564)">
        <g>
            <g transform="matrix(1.45833,0,0,1.27273,-34.6042,-156)">
                <ellipse cx="69.5" cy="569.25" rx="6" ry="2.75" style="fill:none;stroke:black;stroke-width:1.46px;"/>
            </g>
            <path d="M58,579C58,579 65.56,584.338 75.5,579" style="fill:none;stroke:black;stroke-width:2px;"/>
            <path d="M58,579L58,569" style="fill:none;stroke:black;stroke-width:2px;"/>
            <path d="M61,580.186L61,571.5" style="fill:none;stroke:black;stroke-width:2px;"/>
            <path d="M66.5,581L66.5,572.5" style="fill:none;stroke:black;stroke-width:2px;"/>
            <path d="M72,580L72,572" style="fill:none;stroke:black;stroke-width:2px;"/>
            <g transform="matrix(1,0,0,1.05263,0,-29.9474)">
                <path d="M75.5,578.5L75.5,569" style="fill:none;stroke:black;stroke-width:1.95px;"/>
            </g>
        </g>
    </g>
</svg></span>`, `<svg width="100%" height="100%" viewBox="0 0 21 21" version="1.1" xmlns="http://www.w3.org/2000/svg" xmlns:xlink="http://www.w3.org/1999/xlink" xml:space="preserve" xmlns:serif="http://www.serif.com/" style="fill-rule:evenodd;clip-rule:evenodd;stroke-linecap:round;stroke-linejoin:round;stroke-miterlimit:1.5;">
    <g transform="matrix(1,0,0,1,-87.5,-563)">
        <g>
            <g transform="matrix(1,0,0,0.973684,-0.5,15.8158)">
                <circle cx="98.5" cy="572.5" r="9.5" style="fill:none;stroke:black;stroke-width:2.03px;"/>
            </g>
            <path d="M89,573L94,573L97,567.5L99,579L101,571L103.5,573.25L107,573" style="fill:none;stroke:black;stroke-width:2px;"/>
        </g>
    </g>
</svg></span>`]
            for (var i = 0; i < transients.length; i++) {
                for (var j = 0; j < transients[i].length; j++) {
                    let htmlElement = document.createElement('span');
                    htmlElement.innerHTML = span + transient_content[i];
                    htmlElement.style.position = 'relative';
                    let color = 'rgb(255,255,255,0.3)';
                    if (transients[i][j] > 0) {
                        let start = parseFloat(transients[i][j]) / 44100.0;
                        console.log(`transient ${i} ${j} ${start}`);
                        wsRegions.addRegion({
                            start: start - 0.007,
                            end: start + 0.007,
                            // color: transient_colors[i],
                            color: color,
                            opacity: 0.5,
                            height: 0.5,
                            drag: true,
                            resize: false,
                            loop: false,
                            content: htmlElement,
                            id: `transient-${i}-${j}`,
                        });
                    }
                }
                // if (isZeptocore) {
                //     break;
                // }
            }


            // find all the parents of `.regionsvg` and add to the style
            let regionsvg = document.querySelectorAll('#waveform > div')[0].shadowRoot.querySelectorAll('.regionsvg');
            for (var i = 0; i < regionsvg.length; i++) {
                regionsvg[i].parentElement.style.display = 'flex';
                regionsvg[i].parentElement.style.justifyContent = 'center';
                regionsvg[i].parentElement.style.alignItems = 'center';
            }


        },
        isGrimoireEffectSelected(num) {
            let v = this.settingsGrimoireEffects[this.grimoireSelected][num];
            console.log(v);
            return v;
        },
        grimoireClick(num) {
            this.grimoireSelected = num;
            this.grimoireKnobDegrees = num * (505 - 220) / 6 + 220;
        },
        grimoireEffectClick(num) {
            this.settingsGrimoireEffects[this.grimoireSelected][num] = !this.settingsGrimoireEffects[this.grimoireSelected][num];
            // update vue2 
            this.$forceUpdate();
            // update state
            this.saveState();
        },
        resetDevice() {
            midiResetDevice();
            this.regular_message = "zeptocore is resetting now.<br>";
            this.regular_message += "A new drive will appear in a few seconds.<br>"
            this.regular_message += `Download this firmware to the new drive: <a href='https://github.com/schollz/_core/releases/download/${app.latestVersion}/zeptocore_${app.latestVersion}.uf2'>${app.latestVersion}</a>`;
        },
        uploadFirmare() {
            this.deviceFirmwareUpload = true;
            socket.send(JSON.stringify({
                action: "uploadfirmware",
            }));
        },
        updateSlicesPerBeat() {
            if (socket != null) {
                setTimeout(() => {
                    socket.send(JSON.stringify({
                        action: "setsplicetrigger",
                        filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                        number: parseInt(this.banks[this.selectedBank].files[this.selectedFile].SpliceTrigger),
                    }));
                }, 150);
            }
        },
        updateSpliceVariable() {
            setTimeout(() => {
                // update the server file
                socket.send(JSON.stringify({
                    action: "setsplicevariable",
                    filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                    boolean: this.banks[this.selectedBank].files[this.selectedFile].SpliceVariable,
                }));
                this.saveState();
            }, 100);
        },
        newURL(evt) {
            var data = evt.target.value;
            // get base url
            window.location.href = window.location.origin + "/" + data;
        },
        isSelected(fileIndex) {
            return this.selectedFiles.includes(fileIndex);
        },
        isSelectedAndOpen(fileIndex) {
            return this.selectedFile === fileIndex;
        },
        toggleSamplePlayback() {
            if (wsf == null) {
                return;
            }
            this.playingSample = !this.playingSample;
            if (this.playingSample) {
                activeRegion = null;
                wsf.setTime(0);
                wsf.setVolume(1);
                wsf.play();
            } else {
                wsf.pause();
                wsf.setVolume(0);
            }
        },
        mergeSelectedFiles() {
            // organize selected files by their index
            this.selectedFiles.sort((a, b) => (a > b) ? 1 : -1);
            socket.send(JSON.stringify({
                action: "mergefiles",
                filenames: this.selectedFiles.map(index => this.banks[this.selectedBank].files[index].Filename),
            }));

        },
        deleteAllRegions() {
            if (wsf != null) {
                wsf.plugins[0].regions.forEach(region => {
                    region.remove();
                });

                // Clear the sliceStart and sliceStop arrays in the selected file
                app.banks[app.selectedBank].files[app.selectedFile].SliceStart = [];
                app.banks[app.selectedBank].files[app.selectedFile].SliceStop = [];

            }
        },
        createRegionsAutomatically() {

            socket.send(JSON.stringify({
                action: "onsetdetect",
                filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                number: parseInt(document.getElementById("onsetSlices").value),
            }));
            setTimeout(() => {
                this.banks[this.selectedBank].files[this.selectedFile].SpliceVariable = true;
                this.updateSpliceVariable();
            }, 350);
        },
        createRegionsEvenly() {
            // create regions evenly
            this.deleteAllRegions();
            if (wsf != null) {
                const numRegions = parseInt(document.getElementById("onsetSlices").value);
                const duration = wsf.getDuration();
                const regionDuration = duration / numRegions;
                for (var i = 0; i < numRegions; i++) {
                    wsRegions.addRegion({
                        start: i * regionDuration,
                        end: (i + 1) * regionDuration,
                        color: ccolor,
                        drag: true,
                        resize: true,
                        loop: false,
                    });

                }
                this.drawTransients();

                const bpm = this.banks[this.selectedBank].files[this.selectedFile].BPM;
                const lengthPerBeat = 60 / bpm;
                const lengthPerSliceCurrent = regionDuration;
                this.banks[this.selectedBank].files[this.selectedFile].SpliceVariable = false;
                setTimeout(() => {
                    this.updateSpliceVariable();
                }, 200);
                this.banks[this.selectedBank].files[this.selectedFile].SpliceTrigger = Math.round((duration / lengthPerBeat) * 192 / numRegions / 24) * 24;
                setTimeout(() => {
                    this.updateSlicesPerBeat();
                }, 300);
                updateAllRegions();
            }

        },
        openFileInput() {
            // Trigger file input click when the drop area is clicked
            document.getElementById('fileInput').click();
        },
        handleFileInputChange(event) {
            // Handle selected files when the file input changes
            const files = event.target.files;
            this.progressBarWidth = '0px';
            totalBytesUploaded = 0;
            totalBytesRequested = 0;
            for (var i = 0; i < files.length; i++) {
                totalBytesRequested += files[i].size;
            }
            if (fadeOutTimeout != null) {
                clearTimeout(fadeOutTimeout);
            }
            fadeInProgressbar();

            const formData = new FormData();
            for (const file of files) {
                formData.append('files', file);
            }

            // Use fetch to send a POST request to the server
            fetch('/upload?id=' + randomID + "&place=" + window.location.pathname + "&dropaudiofilemode=" + app.dropaudiofilemode, {
                method: 'POST',
                body: formData,
            })
                .then(response => response.json())
                .then(data => {
                    console.log(data.message);
                    // Optionally, you can handle the server response
                })
                .catch(error => {
                    console.error('Error uploading files:', error);
                });

            // Clear the file input value to allow selecting the same file again
            event.target.value = null;
        },
        saveLastSelected() {
            this.lastSelectedFile = this.selectedFile;
        },
        saveState() {
            const savedState = {
                banks: app.banks,
                oversampling: app.oversampling,
                stereoMono: app.stereoMono,
                resampling: app.resampling,
                selectedBank: app.selectedBank,
                selectedFile: app.selectedFile,
                settingsBrightness: app.settingsBrightness,
                settingsClockStop: app.settingsClockStop,
                settingsClockOutput: app.settingsClockOutput,
                settingsClockBehaviorSync: app.settingsClockBehaviorSync,
                settingsGrimoireEffects: app.settingsGrimoireEffects,
                settingsKnobXSample: app.settingsKnobXSample,
            };
            if (!hasSavedToCookie) {
                saveCurrentPage();
                hasSavedToCookie = true;
            }
            if (socket != null) {
                fadeInCircle();
                fadeOut();
                socket.send(JSON.stringify({
                    action: "updatestate",
                    place: window.location.pathname,
                    state: JSON.stringify(savedState),
                }));
            }
        },
        clearCurrentBank() {
            this.banks[this.selectedBank].files = [];
            this.banks[this.selectedBank].lastSelectedFile = null;
            this.selectedFile = null;
        },
        selectBankAll() {
            this.selectedFiles = [];
            for (var i = 0; i < this.banks[this.selectedBank].files.length; i++) {
                this.selectedFiles.push(i);
            }
        },
        selectBankNone() {
            this.selectedFiles = [];
        },
        clearSelectedFiles() {
            // for each index of the seleced files, remove from the current bank
            for (var i = 0; i < this.selectedFiles.length; i++) {
                this.banks[this.selectedBank].files.splice(this.selectedFiles[i], 1);
            }
            this.selectedFiles = [];
            this.selectedFile = null;
        },

        removeSelectedFiles() {
            // Remove selected files from the current bank
            const updatedFiles = this.banks[this.selectedBank].files.filter((file, index) => !this.selectedFiles.includes(index));

            // Update the files array in the current bank
            this.$set(this.banks[this.selectedBank], 'files', updatedFiles);

            // Clear the selected files array
            this.selectedFiles = [];

            // Deselect the currently selected file
            this.selectedFile = null;
        },
        selectBank(index) {
            // Save the currently selected file index in the current bank
            this.banks[this.selectedBank].lastSelectedFile = this.selectedFile;

            // Switch to the new bank
            this.selectedBank = index;

            // Set the selectedFile to the last selected file in the new bank
            this.selectedFile = this.banks[index].lastSelectedFile;
            if (this.selectedFile != null) {
                setTimeout(() => {
                    showWaveform(this.banks[this.selectedBank].files[this.selectedFile].PathToFile,
                        this.banks[this.selectedBank].files[this.selectedFile].Duration,
                        this.banks[this.selectedBank].files[this.selectedFile].SliceStart,
                        this.banks[this.selectedBank].files[this.selectedFile].SliceStop,
                        this.banks[this.selectedBank].files[this.selectedFile].SliceType,
                        this.banks[this.selectedBank].files[this.selectedFile].Transients,
                    );
                }, 100);
            }
        },
        openFileInput() {
            // Trigger file input click when the drop area is clicked
            document.getElementById('fileInput').click();
        },
        deleteRegion() {
            if (activeRegion != null) {
                activeRegion.remove();
                activeRegion = null;
            }
        },
        updateBPM() {

            setTimeout(() => {
                // convert to number 
                this.banks[this.selectedBank].files[this.selectedFile].BPM = parseInt(this.banks[this.selectedBank].files[this.selectedFile].BPM);
                // update the server file
                socket.send(JSON.stringify({
                    action: "setbpm",
                    filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                    number: this.banks[this.selectedBank].files[this.selectedFile].BPM,
                }));
            }, 100);
        },
        updateSplicePlayback() {
            setTimeout(() => {
                // convert to number 
                this.banks[this.selectedBank].files[this.selectedFile].SplicePlayback = parseInt(this.banks[this.selectedBank].files[this.selectedFile].SplicePlayback);
                // update the server file
                socket.send(JSON.stringify({
                    action: "setspliceplayback",
                    filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                    number: this.banks[this.selectedBank].files[this.selectedFile].SplicePlayback,
                }));
            }, 200);
        },
        updateChannels() {
            setTimeout(() => {
                // update the server file
                socket.send(JSON.stringify({
                    action: "setchannels",
                    filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                    boolean: this.banks[this.selectedBank].files[this.selectedFile].Channels,
                }));
            }, 100);

        },
        updateOneshot() {
            if (document.getElementById("oneshot").checked) {
                this.banks[this.selectedBank].files[this.selectedFile].SplicePlayback = 1;
            } else {
                this.banks[this.selectedBank].files[this.selectedFile].SplicePlayback = 0;
            }
            this.updateSplicePlayback();
            setTimeout(() => {
                // update the server file
                socket.send(JSON.stringify({
                    action: "setoneshot",
                    filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                    boolean: this.banks[this.selectedBank].files[this.selectedFile].OneShot,
                }));
            }, 100);
        },
        updateTempomatch() {
            setTimeout(() => {
                // update the server file
                socket.send(JSON.stringify({
                    action: "settempomatch",
                    filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                    boolean: this.banks[this.selectedBank].files[this.selectedFile].TempoMatch,
                }));
            }, 100);
        },
        handleDrop(event) {
            event.preventDefault();
            const files = event.target.files || event.dataTransfer.files;
            this.progressBarWidth = '0%';
            fadeInProgressbar();
            totalBytesUploaded = 0;
            totalBytesRequested = 0;
            for (var i = 0; i < files.length; i++) {
                totalBytesRequested += files[i].size;
            }
            console.log('totalBytesRequested', totalBytesRequested);

            const formData = new FormData();
            for (const file of files) {
                formData.append('files', file);
            }
            // Use fetch to send a POST request to the server
            fetch('/upload?id=' + randomID + "&place=" + window.location.pathname + "&dropaudiofilemode=" + app.dropaudiofilemode, {
                method: 'POST',
                body: formData,
            })
                .then(response => response.json())
                .then(data => {
                    console.log(data.message);
                    // Optionally, you can handle the server response
                })
                .catch(error => {
                    console.error('Error uploading files:', error);
                });
        },
        showFileDetails(fileIndex) {
            const index = this.selectedFiles.indexOf(fileIndex);
            if (index === -1) {
                this.selectedFile = fileIndex;
                this.selectedFiles.push(fileIndex); // File is not selected, so add it to the selectedFiles array
            } else {
                this.selectedFile = null;
                this.selectedFiles.splice(index, 1); // File is selected, so remove it from the selectedFiles array
            }

            if (this.selectedFile != null) {
                this.banks[this.selectedBank].lastSelectedFile = this.selectedFile;
                // wait 100 milliseconds
                setTimeout(() => {
                    showWaveform(this.banks[this.selectedBank].files[this.selectedFile].PathToFile,
                        this.banks[this.selectedBank].files[this.selectedFile].Duration,
                        this.banks[this.selectedBank].files[this.selectedFile].SliceStart,
                        this.banks[this.selectedBank].files[this.selectedFile].SliceStop,
                        this.banks[this.selectedBank].files[this.selectedFile].SliceType,
                        this.banks[this.selectedBank].files[this.selectedFile].Transients,
                    );
                }, 100);
            }
        },
        removeSelectedFile() {
            if (this.selectedFile !== null) {
                this.banks[this.selectedBank].files.splice(this.selectedFile, 1);
                this.selectedFile = null;
            }
        },
        moveFileUp() {
            if (this.selectedFile > 0) {
                this.swapFiles(this.selectedFile, this.selectedFile - 1);
                this.selectedFile--;
                this.selectedFiles = [];
                this.selectedFiles.push(this.selectedFile);
            }
        },
        moveFileDownIndex(index) {
            this.selectedFile = null;
            this.selectedFiles = [];
            if (index < this.banks[this.selectedBank].files.length - 1) {
                this.swapFiles(index, index + 1);
            }
        },
        moveFileUpIndex(index) {
            this.selectedFile = null;
            this.selectedFiles = [];
            if (index > 0) {
                this.swapFiles(index, index - 1);
            }
            if (this.selectedFile == index) {
                this.selectedFile--;
                this.selectedFiles = [];
                this.selectedFiles.push(this.selectedFile);
            }
        },
        moveFileDown() {
            if (this.selectedFile < this.banks[this.selectedBank].files.length - 1) {
                this.swapFiles(this.selectedFile, this.selectedFile + 1);
                this.selectedFile++;
                this.selectedFiles = [];
                this.selectedFiles.push(this.selectedFile);
            }
        },
        swapFiles(index1, index2) {
            const temp = this.banks[this.selectedBank].files[index1];
            this.banks[this.selectedBank].files[index1] = this.banks[this.selectedBank].files[index2];
            this.banks[this.selectedBank].files[index2] = temp;
        },
        doSubmitForm() {
            this.submitForm(false);
        },
        submitForm(settingsOnly) {
            app.uploading = true;
            app.processing = false;
            app.downloading = false;

            // Include other options as needed
            // see the pack.Data struct in pack.go
            const formData = {
                oversampling: this.oversampling,
                stereoMono: this.stereoMono,
                resampling: this.resampling,
                settingsBrightness: parseInt(app.settingsBrightness),
                settingsClockStop: app.settingsClockStop,
                settingsClockOutput: app.settingsClockOutput,
                settingsClockBehaviorSync: app.settingsClockBehaviorSync,
                settingsGrimoireEffects: app.settingsGrimoireEffects,
                settingsKnobXSample: app.settingsKnobXSample,
                banks: [],
            };
            for (var i = 0; i < this.banks.length; i++) {
                const bankData = {
                    files: [],
                };

                for (var j = 0; j < this.banks[i].files.length; j++) {
                    // Assuming you want to push file names into the files array
                    bankData.files.push(this.banks[i].files[j].Filename);
                }

                // Push the bank data into the formData
                formData.banks.push(bankData);
            }

            // Use fetch to send a POST request to the server
            fetch('/download?id=' + randomID + "&place=" + window.location.pathname + "&settingsOnly=" + settingsOnly, {
                method: 'POST',
                headers: {
                    'Content-Type': 'application/json',
                },
                body: JSON.stringify(formData),
            })
                .then(response => {
                    const contentDisposition = response.headers.get('Content-Disposition');
                    if (contentDisposition) {
                        const filenameMatch = /attachment; filename=(.*)/.exec(contentDisposition);
                        if (filenameMatch && filenameMatch[1]) {
                            return { filename: filenameMatch[1], blob: response.blob() };
                        }
                    }
                    return { filename: 'output.zip', blob: response.blob() };
                })
                .then(({ filename, blob }) => {
                    return blob.then(actualBlob => ({ filename, actualBlob }));
                })
                .then(({ filename, actualBlob }) => {
                    const url = window.URL.createObjectURL(new Blob([actualBlob]));
                    const link = document.createElement('a');
                    link.href = url;
                    link.download = filename; // Set the filename dynamically
                    document.body.appendChild(link);
                    link.click();
                    document.body.removeChild(link);
                    window.URL.revokeObjectURL(url);
                    console.log("downloaded");
                    app.uploading = false;
                    app.processing = false;
                    app.downloading = false;

                })
                .catch(error => {
                    console.error('Error downloading files:', error);
                });
        },
    },
});


const showWaveform = debounce(showWaveform_, 50);


function showWaveform_(filename, duration, sliceStart, sliceEnd, sliceType, transients) {
    if (wsf != null) {
        wsf.destroy();
    }
    // check if transients are empty
    let isEmpty = true;
    for (var i = 0; i < transients.length; i++) {
        for (var j = 0; j < transients[i].length; j++) {
            if (transients[i][j] > 0) {
                isEmpty = false;
                break;
            }
        }
        if (!isEmpty) {
            break;
        }
    }
    if (isEmpty) {
        // console.log("no transients?");
        // send message to server
        socket.send(JSON.stringify({
            action: "gettransients",
            filename: filename,
            bankNum: app.selectedBank,
            fileNum: app.selectedFile,
        }));
    }
    // console.log('showWaveform', filename, transients.size);
    // console.log('sliceType', sliceType);
    var banksSelectWidth = document.getElementsByClassName('banks-selector')[0].clientWidth;
    var newWidth = `width:${document.getElementById('waveform-parent').parentElement.clientWidth - 50}px`;
    document.getElementById('waveform-parent').style = newWidth;
    wsf = window.WaveSurf.create({
        container: '#waveform',
        waveColor: wavecolor,
        progressColor: wavecolor,
        cursorColor: wavecolor,
        hideScrollbar: false,
        autoScroll: false,
        autoCenter: true,
        url: filename + ".ogg",
    });
    // resize whenever a zoom
    wsf.on('zoom', () => {
        console.log('zoom');
    });

    wsf.on('interaction', () => {
        const progress = wsf.getCurrentTime() / wsf.getDuration();  // Get the progress as a percentage
        const timeClicked = progress * wsf.getDuration();
        app.waveformClick(timeClicked);
    });

    wsRegions = wsf.registerPlugin(window.RegionsPlugin.create())


    wsf.on('decode', () => {


        console.log("wsf.on('decode')");
        // Regions
        for (var i = 0; i < sliceStart.length; i++) {
            wsRegions.addRegion({
                start: sliceStart[i] * wsf.getDuration(),
                end: sliceEnd[i] * wsf.getDuration(),
                color: ccolor, // (sliceType[i] == 1 ? ccolor2 : ccolor),
                drag: true,
                resize: true,
                loop: false,
            });
        }
        app.drawTransients();

        // fix this debounce ot take argument of the region
        const updateRegion = debounce(function (region) {
            // check if region has transient prefix
            console.log(region.id.startsWith('transient-'));
            if (region.id.startsWith('transient-')) {
                let parts = region.id.split('-');
                let i = parseInt(parts[1]);
                let j = parseInt(parts[2]);
                console.log("updating transient", i, j, region.start);
                // update the transient
                let samplePosition = Math.round((0.007 + region.start) * 44100);
                app.banks[app.selectedBank].files[app.selectedFile].Transients[i][j] = samplePosition;
                socket.send(JSON.stringify({
                    action: "settransient",
                    filename: filename,
                    i: i,
                    j: j,
                    n: samplePosition,
                }));
                setTimeout(() => {
                    app.saveState();
                }, 100);
                return;
            }
            updateAllRegions();
        }, 200);
        var regionTriggers = ['region-updated', 'region-created']
        for (regionTrigger of regionTriggers) {
            wsRegions.on(
                regionTrigger, (region) => {
                    updateRegion(region);
                });
        }
    });

    wsRegions.enableDragSelection({
        color: ccolor,
    })

    {
        var playing = false;
        wsRegions.on('region-in', (region) => {
            console.log('in', region.id);
        })
        wsRegions.on('region-out', (region) => {
            console.log('out', region.id);
            if (activeRegion != null) {
                if (activeRegion.id === region.id) {
                    wsf.pause();
                    playing = false;
                }
            }
        })
        wsRegions.on('region-clicked', (region, e) => {
            e.stopPropagation() // prevent triggering a click on the waveform


            // Get the click position in the waveform in pixels
            const regionElement = region.element;
            const rect = regionElement.getBoundingClientRect(); // Get region's bounding rectangle
            const clickX = e.clientX - rect.left; // X position relative to the region's left side
            const clickPercent = clickX / rect.width; // Percentage of the region clicked

            // Calculate the exact time clicked within the region
            const regionDuration = region.end - region.start;
            const timeClickedInRegion = region.start + clickPercent * regionDuration;

            app.waveformClick(timeClickedInRegion);


            if (app.regionClickBehavior != "clickCreateRegion") {
                return;
            }
            activeRegion = region;
            // print the seconds of the region clicked
            console.log(region.start, region.end, e);
            // iterate over wsRegions.regions 
            console.log(activeRegion)
            for (var i = 0; i < wsRegions.regions.length; i++) {
                wsRegions.regions[i].setOptions({
                    color: (activeRegion.id == wsRegions.regions[i].id ? ccolor2 : ccolor),
                    // color: ccolor, // (sliceType[i] == 1 ? ccolor2 : ccolor)
                });
            }
            region.setOptions({ color: selected_color });
            if (playing) {
                console.log('pausing');
                wsf.pause();
                playing = false;
            } else {
                region.play();
                wsf.setVolume(1);
                playing = true;
                for (var i = 0; i < 10; i++) {
                    let j = i;
                    setTimeout(() => {
                        console.log(j / 10);
                        wsf.setVolume(j / 10);
                    }, (region.end - region.start) * 1000 - j * 5);
                }
            }
            // region.setOptions({ color: randomColor() })
        })
        // Reset the active region when the user clicks anywhere in the waveform
        wsf.on('interaction', () => { activeRegion = null })
    }

    // Update the zoom level on slider change
    wsf.once('decode', () => {
        document.querySelector('#wsfzoom').oninput = (e) => {
            const minPxPerSec = Number(e.target.value)
            wsf.zoom(minPxPerSec)
        };
    });

}

window.addEventListener('load', (event) => {
    // initialize the colors
    const url = window.location.href;
    const root = document.documentElement;

    ccolor = getComputedStyle(document.documentElement).getPropertyValue('--header-footer-background') + "33";
    ccolor2 = getComputedStyle(document.documentElement).getPropertyValue('--other-color') + "00";
    wavecolor = getComputedStyle(document.documentElement).getPropertyValue('--header-footer-background');
    selected_color = getComputedStyle(document.documentElement).getPropertyValue('--highlight-color') + "44";


    // Initialize WebSocket and other event listeners
    socketCloseListener();

    // Check if the window width is less than 768 pixels to set isMobile
    app.isMobile = window.innerWidth < 768;
    app.grimoireClick(0);

    // Listen for window resize events to update isMobile
    window.addEventListener('resize', () => {
        app.isMobile = window.innerWidth < 768;
    });

    // get latest release info
    GetLatestReleaseInfo();

    // disable if on firefox
    if (navigator.userAgent.indexOf("Firefox") != -1) {
        console.log("Firefox is not supported for MIDI, please use Chrome or Safari.");
    } else {
        // check if midi is available
        listMidiPorts();
        checkMidiInterval = setInterval(() => {
            if (!app.midiIsSetup) {
                listMidiPorts();
            }
        }, 250);
    }

    setTimeout(() => {
        tippy("#editingSplice", {
            content: "Click waveform and drag splice regions or double click to add splice."
        });
        tippy("#editingKick", {
            content: "Click waveform to add kick trig, hold to drag around."
        });
        tippy('#windows_core', {
            content: 'Click to download an offline version of this tool for windows.'
        });
        tippy('#macos_core', {
            content: 'Click to download an offline version of this tool for macs.'
        });
        tippy('#linux_core', {
            content: 'Click to download an offline version of this tool for linux.'
        });
        tippy('#downloadZipButton', {
            content: 'After downloading, extract the contents to the SD card.',
        });
        tippy('#dropaudiofilemode', {
            content: 'The "oneshot" mode will set properties to make it easy to merge.',
        });
        tippy('#show-dialog', {
            content: 'Copy this workspace to a new location.',
        });
        tippy('#firmwareDownloadLink', {
            content: 'Click to download the latest firmware.',
        });
        tippy('#show-dialog-settings', {
            content: 'Click to edit global settings on the ectocore.',
        });


        let grimoireList = [
            "amalgam",
            "alum",
            "tree",
            "azurite",
            "hematite",
            "sulphur",
            "brimstone",
        ];
        for (let ii = 0; ii < grimoireList.length; ii++) {
            if (ii < 3) {
                // show on the left
                tippy('#grimoire' + ii, {
                    content: grimoireList[ii],
                    zIndex: 99999999,
                    appendTo: "parent",
                    hideOnClick: false,
                    placement: 'left',
                });
            } else if (ii > 3) {
                // show on the right
                tippy('#grimoire' + ii, {
                    content: grimoireList[ii],
                    zIndex: 99999999,
                    appendTo: "parent",
                    hideOnClick: false,
                    placement: 'right',
                });
            } else {
                tippy('#grimoire' + ii, {
                    content: grimoireList[ii],
                    zIndex: 99999999,
                    appendTo: "parent",
                    hideOnClick: false,
                });

            }
        }

        let effectList = [
            ['distortionEffect', 'Fuzz distortion'],
            ['lossEffect', 'Lossy compression effect'],
            ['bitcrushEffect', 'Bitcrushing reduction'],
            ['filterEffect', 'Frequency filtering'],
            ['timestretchEffect', 'Time stretching'],
            ['delayEffect', 'Delay echo effect'],
            ['combEffect', 'Comb filtering effect'],
            ['beatRepeatEffect', 'Beat repetition effect'],
            ['reverbEffect', 'Reverberation effect'],
            ['autopanEffect', 'Automatic panning'],
            ['slowDownEffect', 'Slow down tempo'],
            ['speedupEffect', 'Speed up tempo'],
            ['reverseEffect', 'Reverse audio'],
            ['retriggerEffect', 'Retrigger effect'],
            ['retriggerPitchEffect', 'Retrigger with pitch shift'],
            ['tapestopEffect', 'Tape stop effect']
        ];
        for (let effect of effectList) {
            tippy('#' + effect[0], {
                content: effect[1],
                zIndex: 99999999,
                appendTo: "parent",
                hideOnClick: false,
            });
        }
        // find all elements ".banks-selector > ul > li"
        var desktopBankLabels = document.getElementsByClassName('banks-selector')[0].getElementsByTagName('li');
        for (var i = 0; i < desktopBankLabels.length; i++) {
            tippy(desktopBankLabels[i], {
                content: 'Select Bank ' + (i + 1),
                // show on the right
                placement: 'right',
            });
        }


    }, 1000);


});