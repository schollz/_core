#!/bin/bash

MAGIC="magic"
BAD="bad"
URL="https://duct.schollz.com/notify"

while [ 1 ]
do
X="$(curl -q $URL)"
if [[ $X =~ ^$MAGIC ]]; then
        Y="$(echo "$X" | sed "s/$MAGIC*//")"
    # check if $X contains $BAD
    if [[ "$X" == *"$BAD"* ]]; then
        notify-send -i face-worried "Oh no!" "$Y"
        paplay /usr/share/sounds/freedesktop/stereo/service-logout.oga
    else
        notify-send -i face-smile "Hurray!" "$Y"
        paplay /usr/share/sounds/freedesktop/stereo/service-login.oga
    fi
else
        sleep 1
fi
  
done