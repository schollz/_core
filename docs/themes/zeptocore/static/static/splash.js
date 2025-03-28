
// // Get the modal
// var modal2 = document.getElementById("myModal2");

// // Get the <span> element that closes the modal
// var span2 = document.getElementsByClassName("close2")[0];


// // When the user clicks on <span> (x), close the modal
// span2.onclick = function () {
//     modal2.style.opacity = 0;
//     setTimeout(function () {
//         modal2.style.display = "none";
//     }, 1000);
// }

// // When the user clicks anywhere outside of the modal, close it
// window.onclick = function (event) {
//     if (event.target == modal2) {
//         modal2.style.opacity = 0;
//         setTimeout(function () {
//             modal2.style.display = "none";
//         }, 1000);
//     }
// }

// // When the user clicks any link in the modal, close it
// var links = document.querySelectorAll('#myModal2 a');
// for (var i = 0; i < links.length; i++) {
//     links[i].addEventListener('click', function () {
//         modal2.style.opacity = 0;
//         setTimeout(function () {
//             modal2.style.display = "none";
//         }, 1000);
//     });
// }

// // if splash=1 cookie not found
// // if URL contains "buy"
// if (window.location.href.indexOf("buy") > -1) {
//     // show modal after 1 second
//     setTimeout(function () {
//         // fade in the modal
//         modal2.style.opacity = 1;
//         modal2.style.display = "block";
//         var earElements = document.querySelectorAll('.ear');
//         for (var i = 0; i < earElements.length; i++) {
//             earElements[i].style.display = "none";
//         }
//         console.log("showing buy button");
//         document.getElementsByClassName("buybutton")[0].style.display = "inline-block";
//         // add listener for buy button
//         document.getElementsByClassName("buybutton")[0].addEventListener('click', function () {
//             // show modal
//             modal2.style.opacity = 1;
//             modal2.style.display = "block";
//             var earElements = document.querySelectorAll('.ear');
//             for (var i = 0; i < earElements.length; i++) {
//                 earElements[i].style.display = "none";
//             }
//             document.getElementsByClassName("buybutton")[0].style.display = "inline-block";
//         });
//     }, 100);
//     // hide ears
//     // show buy button
// }



// // if (localStorage.getItem('splash') !== '2') {
// //     // show modal after 1 second
// //     setTimeout(function () {
// //         // fade in the modal
// //         modal2.style.opacity = 1;
// //         localStorage.setItem('splash', '1');
// //     }, 100);
// // }


// wait for page load
// window.addEventListener('load', function () {
//     // goto #buy when clicking buy button
//     document.getElementsByClassName("buybutton")[0].addEventListener('click', function () {
//         console.log("buy button clicked");
//         window.location.href = "#buy";
//     });
//     if (window.location.href.indexOf("buy") > -1) {
//         // scroll to buy section
//         var buy = document.getElementById("buy");
//         buy.scrollIntoView();
//     }

// });

