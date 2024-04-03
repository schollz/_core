var total_slices = 0;
var time_received_total_slices = 0;
var key_to_jump = [49, 50, 51, 52, 113, 119, 101, 114,
    97, 115, 100, 102, 122, 120, 99, 118];
var fx_to_jump = [53, 54, 55, 56, 116, 121, 117, 105,
    103, 104, 106, 107, 98, 110, 109, 44];


// Get the modal
var modal = document.getElementById("myModal");

// Get the <span> element that closes the modal
var span = document.getElementsByClassName("close")[0];


// When the user clicks on <span> (x), close the modal
span.onclick = function () {
    modal.style.display = "none";
}

// When the user clicks anywhere outside of the modal, close it
window.onclick = function (event) {
    if (event.target == modal) {
        modal.style.display = "none";
    }
}


const Keyboard = window.SimpleKeyboard.default;

const myKeyboard = new Keyboard({

});

function activateKey(key) {
    // Find the button element with the specific data-skbtn attribute
    const button = document.querySelector(`[data-skbtn="${key}"]`);

    // Check if the button exists
    if (button) {
        // Add the 'hg-activeButton' class to the element
        button.classList.add('hg-activeButton');

        // Optionally, remove the class after some time to simulate the key press effect
        setTimeout(() => {
            button.classList.remove('hg-activeButton');
        }, 200); // 200 milliseconds delay
    } else {
        console.log(`Key "${key}" not found.`);
    }
}
function toggleKey(key, on) {
    // Find the button element with the specific data-skbtn attribute
    const button = document.querySelector(`[data-skbtn="${key}"]`);

    // Check if the button exists
    if (button) {
        if (on) {
            // Add the 'hg-activeButton' class to the element
            button.classList.add('hg-activeButton');
        } else {
            button.classList.remove('hg-activeButton');
        }

    } else {
        console.log(`Key "${key}" not found.`);
    }
}

function addToMidiConsole(message) {
    console.log('MIDI message:', message);
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
                // see if it starts with version=
                if (sysex.startsWith("version=")) {
                    console.log(sysex);
                    app.deviceVersion = sysex.split("=")[1];
                    console.log(`[setupMidiInputListener] Device version: ${app.deviceVersion}`);
                } else if (sysex.startsWith("slices=")) {
                    total_slices = sysex.split("=")[1];
                    console.log(`[setupMidiInputListener] Total slices: ${total_slices}`);
                    // get currrent time 
                    time_received_total_slices = Date.now();
                } else if (sysex.startsWith("slice=")) {
                    let slice = sysex.split("=")[1];
                    let key_slice = key_to_jump[slice % 16];
                    // determine letter for that keyboard number
                    let letter = String.fromCharCode(key_slice);
                    activateKey(letter);
                } else if (sysex.startsWith("fx=")) {
                    // format fx=0,1
                    let fx = sysex.split("=")[1];
                    let fx_num = fx.split(",")[0];
                    let fx_val = fx.split(",")[1];
                    let letter = String.fromCharCode(fx_to_jump[fx_num % 16]);
                    toggleKey(letter, fx_val > 0);
                } else {
                    addToMidiConsole(sysex);
                }
            } else {
                addToMidiConsole(midiMessage.data);
            }
        };
    }
}

function setupMidi() {
    navigator.requestMIDIAccess({ sysex: true })
        .then((midiAccess) => {
            // Input setup
            const inputs = midiAccess.inputs.values();
            for (let input of inputs) {
                console.log(input.name);
                if (input.name.includes("boardcore") || input.name.includes("zeptocore")) {
                    window.inputMidiDevice = input;
                    setupMidiInputListener();
                    console.log("input device connected");
                    break;
                }
            }

            // Output setup
            const outputs = midiAccess.outputs.values();
            for (let output of outputs) {
                console.log(output.name);
                if (output.name.includes("boardcore") || output.name.includes("zeptocore")) {
                    window.boardcoreDevice = output;
                    console.log("output device connected");
                    // show modal
                    modal.style.display = "block";
                    break;
                }
            }


        })
        .catch((error) => {
            console.error("Could not access MIDI devices.", error);
        });
}

document.addEventListener('DOMContentLoaded', () => {
    setupMidi();

    setInterval(() => {
        window.boardcoreDevice.send([0x90, 57, 11]);
    }, 500);
    setInterval(() => {
        let current_time = Date.now();
        if (current_time - time_received_total_slices > 1200) {
            modal.style.display = "none";
            setupMidi();
        }
    }, 600);

    // Listen for keypress events
    document.addEventListener('keypress', (e) => {
        window.boardcoreDevice.send([0x90, e.key.charCodeAt(0), 11]);
    });

    // relabel keys

    document.querySelector(`[data-skbtn="5"]`).innerHTML = '<i class="fas fa-sun"></i>';
    document.querySelector(`[data-skbtn="6"]`).innerHTML = '<i class="fas fa-radio"></i>';
    document.querySelector(`[data-skbtn="7"]`).innerHTML = '<i class="fas fa-cat"></i>';
    document.querySelector(`[data-skbtn="8"]`).innerHTML = '<i class="fas fa-wave-square"></i>';
    document.querySelector(`[data-skbtn="t"]`).innerHTML = '<i class="fas fa-hourglass-half"></i>';
    document.querySelector(`[data-skbtn="y"]`).innerHTML = '<i class="fas fa-history"></i>';
    document.querySelector(`[data-skbtn="u"]`).innerHTML = '<i class="fas fa-brush"></i>';
    document.querySelector(`[data-skbtn="i"]`).innerHTML = '<i class="fas fa-redo-alt"></i>';
    document.querySelector(`[data-skbtn="g"]`).innerHTML = '<i class="fas fa-vector-square"></i>';
    document.querySelector(`[data-skbtn="h"]`).innerHTML = '<i class="fas fa-water"></i>';
    document.querySelector(`[data-skbtn="j"]`).innerHTML = '<i class="fas fa-headphones"></i>';
    document.querySelector(`[data-skbtn="k"]`).innerHTML = '<i class="fas fa-compact-disc"></i>';
    document.querySelector(`[data-skbtn="b"]`).innerHTML = '<i class="fas fa-filter"></i>';
    document.querySelector(`[data-skbtn="n"]`).innerHTML = '<i class="fas fa-music"></i>';
    document.querySelector(`[data-skbtn="m"]`).innerHTML = '<i class="fas fa-undo-alt"></i>';
    document.querySelector(`[data-skbtn=","]`).innerHTML = '<i class="fas fa-tape"></i>';

    document.querySelector(`[data-skbtn="q"]`).innerHTML = '5';
    document.querySelector(`[data-skbtn="w"]`).innerHTML = '6';
    document.querySelector(`[data-skbtn="e"]`).innerHTML = '7';
    document.querySelector(`[data-skbtn="r"]`).innerHTML = '8';
    document.querySelector(`[data-skbtn="a"]`).innerHTML = '9';
    document.querySelector(`[data-skbtn="s"]`).innerHTML = '10';
    document.querySelector(`[data-skbtn="d"]`).innerHTML = '11';
    document.querySelector(`[data-skbtn="f"]`).innerHTML = '12';
    document.querySelector(`[data-skbtn="z"]`).innerHTML = '13';
    document.querySelector(`[data-skbtn="x"]`).innerHTML = '14';
    document.querySelector(`[data-skbtn="c"]`).innerHTML = '15';
    document.querySelector(`[data-skbtn="v"]`).innerHTML = '16';

});