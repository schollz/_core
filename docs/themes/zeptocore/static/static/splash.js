
// Get the modal
var modal2 = document.getElementById("myModal2");

// Get the <span> element that closes the modal
var span2 = document.getElementsByClassName("close2")[0];


// When the user clicks on <span> (x), close the modal
span2.onclick = function () {
    modal2.style.opacity = 0;
}

// When the user clicks anywhere outside of the modal, close it
window.onclick = function (event) {
    if (event.target == modal2) {
        modal2.style.opacity = 0;
    }
}

// When the user clicks any link in the modal, close it
var links = document.querySelectorAll('#myModal2 a');
for (var i = 0; i < links.length; i++) {
    links[i].addEventListener('click', function () {
        modal2.style.opacity = 0;
    });
}

// if splash=1 cookie not found
if (localStorage.getItem('splash') !== '2') {
    // show modal after 1 second
    setTimeout(function () {
        // fade in the modal
        modal2.style.opacity = 1;
        localStorage.setItem('splash', '1');
    }, 100);
}
