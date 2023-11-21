
var app;
var socket;
var randomID = Math.random().toString(36).substring(2, 15) + Math.random().toString(36).substring(2, 15);

const socketMessageListener = (e) => {
    console.log(e.data);
};
const socketOpenListener = (e) => {
    console.log('Connected');
    socket.send(JSON.stringify({ message: "hello, server" }))
};
const socketErrorListener = (e) => {
    console.error(e);
}
const socketCloseListener = (e) => {
    if (socket) {
        console.log('Disconnected.');
    }
    var url = window.origin.replace("http", "ws") + '/ws?id=' + randomID;
    socket = new WebSocket(url);
    socket.onopen = socketOpenListener;
    socket.onmessage = socketMessageListener;
    socket.onclose = socketCloseListener;
    socket.onerror = socketErrorListener;
};
window.addEventListener('load', (event) => {
    socketCloseListener();
});

app = new Vue({
    el: '#app',
    data: {
        banks: Array.from({ length: 16 }, () => ({ files: [] })),
        selectedBank: 0,
        selectedFile: null,
    },
    methods: {
        selectBank(index) {
            this.selectedBank = index;
            this.selectedFile = null; // Clear selected file when changing banks
        },
        openFileInput() {
            // Trigger file input click when the drop area is clicked
            document.getElementById('fileInput').click();
        },
        handleDrop(event) {
            event.preventDefault();
            const files = event.target.files || event.dataTransfer.files;
            this.banks[this.selectedBank].files = [...this.banks[this.selectedBank].files, ...Array.from(files)];


            const formData = new FormData();
            for (const file of files) {
                formData.append('files', file);
            }
            // Use fetch to send a POST request to the server
            fetch('/upload?id=' + randomID, {
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
            this.selectedFile = fileIndex;
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
            }
        },
        moveFileDown() {
            if (this.selectedFile < this.banks[this.selectedBank].files.length - 1) {
                this.swapFiles(this.selectedFile, this.selectedFile + 1);
                this.selectedFile++;
            }
        },
        swapFiles(index1, index2) {
            const temp = this.banks[this.selectedBank].files[index1];
            this.banks[this.selectedBank].files[index1] = this.banks[this.selectedBank].files[index2];
            this.banks[this.selectedBank].files[index2] = temp;
        },
    },
});
