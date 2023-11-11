function humanFileSize(bytes, si) {
    var thresh = si ? 1000 : 1024;
    if (Math.abs(bytes) < thresh) {
        return bytes + ' B';
    }
    var units = si ? ['kB', 'MB', 'GB', 'TB', 'PB', 'EB', 'ZB', 'YB'] : ['KiB', 'MiB', 'GiB', 'TiB', 'PiB', 'EiB', 'ZiB', 'YiB'];
    var u = -1;
    do {
        bytes /= thresh;
        ++u;
    } while (Math.abs(bytes) >= thresh && u < units.length - 1);
    return bytes.toFixed(1) + ' ' + units[u];
}

var Name = '';
var filesize = 0;

(function(Dropzone) {
    Dropzone.autoDiscover = false;

    let drop = new Dropzone('div#filesBox', {
        maxFiles: 1,
        url: '/',
        method: 'post',
        createImageThumbnails: false,
        previewTemplate: '<div id=\'preview\' class=\'.dropzone-previews\'>#</div>',
        chunking: true,
        forceChunking: true,
        parallelChunkUploads: true,
        timeout: 3000000,
        chunkSize: 2000000,
    });

    drop.on('uploadprogress', function(file, progress, bytesSent) {
        console.log(progress,bytesSent);
        try {
            var width = document.getElementById('preview').offsetWidth - 70;
            var repeatTimes = Math.round(width / 14 * progress / 100);
            document.getElementById('preview').innerHTML =
                `${Name} (${humanFileSize(filesize)})
<p>${'#'.repeat(repeatTimes)} ${Math.round(progress)}%</p>`;

        } catch (err) {}
    });

    drop.on('success', function(file, response) {
        console.log('success');
        console.log(response);
        response = JSON.parse(file.xhr.response);
        console.log(file)
        // if (response.id != 'none') {
        //     location.replace('/' + response.id);
        // }
    });

    drop.on('error', function(file, response) {
        console.log('error');
        console.log(response);
        document.getElementById('errormessage').innerText = response.message;
        // TODO: display this error
        drop.removeAllFiles();
    });

    drop.on('addedfile', function(file) {
        console.log(file);
        Name = file.name;
        filesize = file.size;
        document.getElementById('preview').innerText =
            `${file.name} (${humanFileSize(file.size)})
`;
    })

    drop.on('removedfile', function(file) {
        console.log(file);
    });
})(Dropzone);