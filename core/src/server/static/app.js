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
const ccolor = '#dcd6f799';
const ccolor2 = '#dcd6f766';
const wavecolor = '#3919a1';
var hasSavedToCookie = false;



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
    console.log("socketMessageListener", data.action)
    if (data.action == "processed") {
        console.log("processed");
        for (var i = 0; i < data.file.SliceStart.length; i++) {
            data.file.SliceStart[i] = parseFloat(data.file.SliceStart[i].toFixed(3));
            data.file.SliceStop[i] = parseFloat(data.file.SliceStop[i].toFixed(3));
        }
        // append data.file to bank
        app.banks[app.selectedBank].files.push(data.file);
    } else if (data.action == "firmwareuploaded") {
        app.deviceFirmwareUpload = false;
        if (!data.success) {
            app.error_message = data.message;
        }
    } else if (data.action == "devicefound") {
        app.deviceFound = data.boolean;
    } else if (data.action == "copyworkspace") {
        if (data.error != "") {
            app.error_message = data.error;
        } else {
            window.location = "/" + data.message;
        }
        console.log(data);
    } else if (data.action == "slicetype") {
        app.banks[app.selectedBank].files[app.selectedFile].SliceType = data.sliceType;
        if (app.selectedFile != null) {
            // setTimeout(() => {
            //     showWaveform(app.banks[app.selectedBank].files[app.selectedFile].PathToFile,
            //         app.banks[app.selectedBank].files[app.selectedFile].Duration,
            //         app.banks[app.selectedBank].files[app.selectedFile].SliceStart,
            //         app.banks[app.selectedBank].files[app.selectedFile].SliceStop,
            //         app.banks[app.selectedBank].files[app.selectedFile].SliceType,
            //     );
            // }, 100);
        }
    } else if (data.action == "onsetdetect") {
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
        }
    } else if (data.action == "getstate") {
        var savedState = JSON.parse(data.state);
        if (savedState.banks) {
            app.banks = savedState.banks;
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
    try {
        socket.send(JSON.stringify({
            action: "connected"
        }));
    } catch (error) {
        // oh well
    }
};
const socketErrorListener = (e) => {
    console.error(e);
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

app = new Vue({
    el: '#app',
    data: {
        slicesPerBeat: 1,
        banks: Array.from({ length: 16 }, () => ({ files: [], lastSelectedFile: null })), // Add the lastSelectedFile property
        selectedBank: 0,
        selectedFile: null,
        deviceFound: false,
        deviceFirmwareUpload: false,
        lastSelectedFile: null,
        progressBarWidth: '0px',
        oversampling: '1x', // Default to '1x'
        stereoMono: 'mono', // Default to 'mono'
        isMobile: false, // Define isMobile variable
        playingSample: false,
        downloading: false,
        showCookiePolicy: false,
        processing: false,
        error_message: "",
        uploading: false,
        resampling: 'linear',
        title: window.location.pathname,
        disconnected: false,
        previousPages: [],
        randomPages: [generateRandomWord(), generateRandomWord(), generateRandomWord()],
        selectedFiles: [], // New property to store selected files
    },
    watch: {
        // Watch for changes in app properties and save state to cookies
        banks: {
            handler: 'saveState',
            deep: true,
        },
        oversampling: 'saveState',
        stereoMono: 'saveState',
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
        uploadFirmare() {
            console.log("uploading firmware");
            this.deviceFirmwareUpload = true;
            socket.send(JSON.stringify({
                action: "uploadfirmware",
            }));
        },
        updateSlicesPerBeat() {
            if (socket != null) {
                setTimeout(() => {
                    console.log("updateSlicesPerBeat");
                    console.log(this.banks[this.selectedBank].files[this.selectedFile].SpliceTrigger);
                    socket.send(JSON.stringify({
                        action: "setsplicetrigger",
                        filename: this.banks[this.selectedBank].files[this.selectedFile].Filename,
                        number: parseInt(this.banks[this.selectedBank].files[this.selectedFile].SpliceTrigger),
                    }));
                }, 150);
            }
        },
        updateSpliceVariable() {
            console.log("updateSpliceVariable");
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
            window.location.href = window.location.href + data;
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
            console.log('Selected Files:', this.selectedFiles.map(index => this.banks[this.selectedBank].files[index].Filename));
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
                console.log(numRegions, duration, regionDuration);
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
                const bpm = this.banks[this.selectedBank].files[this.selectedFile].BPM;
                const lengthPerBeat = 60 / bpm;
                const lengthPerSliceCurrent = regionDuration;
                console.log('slices per beat', lengthPerBeat / lengthPerSliceCurrent);
                this.banks[this.selectedBank].files[this.selectedFile].SpliceVariable = false;
                setTimeout(() => {
                    this.updateSpliceVariable();
                }, 200);
                this.banks[this.selectedBank].files[this.selectedFile].SpliceTrigger = Math.round((duration / lengthPerBeat) * 192 / numRegions / 24) * 24;
                setTimeout(() => {
                    this.updateSlicesPerBeat();
                }, 300);
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
            console.log('totalBytesRequested', totalBytesRequested);
            if (fadeOutTimeout != null) {
                clearTimeout(fadeOutTimeout);
            }
            fadeInProgressbar();

            const formData = new FormData();
            for (const file of files) {
                formData.append('files', file);
            }

            // Use fetch to send a POST request to the server
            fetch('/upload?id=' + randomID + "&place=" + window.location.pathname, {
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
            }, 100);
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
            fetch('/upload?id=' + randomID + "&place=" + window.location.pathname, {
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
        submitForm() {
            app.uploading = true;
            app.processing = false;
            app.downloading = false;

            // Include other options as needed
            const formData = {
                oversampling: this.oversampling,
                stereoMono: this.stereoMono,
                resampling: this.resampling,
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
            fetch('/download?id=' + randomID + "&place=" + window.location.pathname, {
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


function showWaveform_(filename, duration, sliceStart, sliceEnd, sliceType) {
    if (wsf != null) {
        wsf.destroy();
    }
    console.log('showWaveform', filename);
    console.log('sliceType', sliceType);
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
    // set the width of a WaveSurfer to 100 px


    wsRegions = wsf.registerPlugin(window.RegionsPlugin.create())

    wsf.on('decode', () => {
        console.log("wsf.on('decode')");
        // Regions
        for (var i = 0; i < sliceStart.length; i++) {
            wsRegions.addRegion({
                start: sliceStart[i] * wsf.getDuration(),
                end: sliceEnd[i] * wsf.getDuration(),
                color: (sliceType[i] == 1 ? ccolor2 : ccolor),
                drag: true,
                resize: true,
                loop: false,
            });
        }
        setTimeout(() => {
        }, 100);
        // fix this debounce ot take argument of the region
        const updateRegion = debounce(function (region) {
            let regions = wsf.plugins[0].regions;
            regions.sort((a, b) => (a.start > b.start) ? 1 : -1);
            let sliceStart = [];
            let sliceStop = [];
            for (var i = 0; i < regions.length; i++) {
                if (i == 0 || regions[i].start > regions[i - 1].start) {
                    sliceStart.push(parseFloat((regions[i].start / wsf.getDuration()).toFixed(3)));
                    sliceStop.push(parseFloat((regions[i].end / wsf.getDuration()).toFixed(3)));
                }
            }

            // update locally
            app.banks[app.selectedBank].files[app.selectedFile].SliceStart = sliceStart;
            app.banks[app.selectedBank].files[app.selectedFile].SliceStop = sliceStop;

            // update remotely
            socket.send(JSON.stringify({
                action: "setslices",
                filename: filename,
                sliceStart: sliceStart,
                sliceStop: sliceStop,
            }));
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
            activeRegion = region;
            console.log(region);
            // iterate over wsRegions.regions 
            for (var i = 0; i < wsRegions.regions.length; i++) {
                wsRegions.regions[i].setOptions({ color: (sliceType[i] == 1 ? ccolor2 : ccolor) });
            }
            region.setOptions({ color: "#00770033" });
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
        document.querySelector('input[type="range"]').oninput = (e) => {
            const minPxPerSec = Number(e.target.value)
            wsf.zoom(minPxPerSec)
        };
    });
}

window.addEventListener('load', (event) => {
    // Initialize WebSocket and other event listeners
    socketCloseListener();

    // Check if the window width is less than 768 pixels to set isMobile
    app.isMobile = window.innerWidth < 768;

    // Listen for window resize events to update isMobile
    window.addEventListener('resize', () => {
        app.isMobile = window.innerWidth < 768;
    });
});