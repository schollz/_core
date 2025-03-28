#!/bin/bash

while true
do
if mount | grep RPI-RP2 > /dev/null; then
	sleep 0.5
	echo "uploading!"
	time pv -batep build/*.uf2 > /media/zns/RPI-RP2/upload.uf2
	exit
fi
sleep 0.1
done