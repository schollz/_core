#!/bin/bash
curl -L https://github.com/schollz/_core/releases/download/v5.0.11/zeptocore_v5.0.11.uf2 > zeptocore_v5.0.11.uf2
while true
do
if mount | grep RPI-RP2 > /dev/null; then
	sleep 1
	echo "uploading..."
	time pv -batep zeptocore_v5.0.11.uf2  > /media/zns/RPI-RP2/zeptocore.uf2
    echo "uploaded!"
fi
sleep 0.5
done
