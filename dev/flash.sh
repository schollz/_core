#!/bin/bash

while true
do
if mount | grep RPI-RP2 > /dev/null; then
	sleep 1
	echo "uploading..."
	time pv -batep zeptocore_v2.1.2.uf2  > /media/zns/RPI-RP2/zeptocore.uf2
    echo "uploaded!"
fi
sleep 0.5
done
