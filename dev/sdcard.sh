#!/bin/bash
sudo umount /media/zns/zepto*
sudo mkdosfs -n zeptocore -s 128 -v /dev/sdd1
sudo mount /media/zns/zeptocore
rsync -avrP /home/zns/Desktop/zeptocore/ /media/zns/zeptocore/