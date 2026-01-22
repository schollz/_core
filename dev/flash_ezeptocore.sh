#!/bin/bash

set -e

DISK="sdd"
PART="/dev/sdd1"
MOUNT="/mnt/ezcore"
LABEL="EZEPTOCORE"
SRC="$HOME/Downloads/ezeptocore-data"
MAX_SIZE=$((33 * 1024 * 1024 * 1024))

echo "Waiting for /dev/$DISK..."

while ! lsblk | grep -q "^$DISK"; do
    sleep 1
done

while true; do

    STATUS=$(lsblk -o NAME,RO,SIZE,TYPE | grep "^$DISK")
    echo "$STATUS"

    SIZE=$(lsblk -b -o NAME,SIZE | grep "^$DISK" | awk '{print $2}')
    RO=$(lsblk -o NAME,RO | grep "^$DISK" | awk '{print $2}')

    if [ "$SIZE" = "0" ]; then
        echo "Drive not ready (0B). Replug card..."
        while true; do
            sleep 1
            SIZE=$(lsblk -b -o NAME,SIZE | grep "^$DISK" | awk '{print $2}')
            [ "$SIZE" != "0" ] && [ -n "$SIZE" ] && break
        done
        continue
    fi

    if [ "$SIZE" -gt "$MAX_SIZE" ]; then
        echo "PANIC: Drive too large. Aborting."
        exit 1
    fi

    if [ "$RO" = "1" ]; then
        echo "Drive read-only. Replug card..."
        while true; do
            sleep 1
            RO=$(lsblk -o NAME,RO | grep "^$DISK" | awk '{print $2}')
            [ "$RO" = "0" ] && break
        done
        continue
    fi

    break

done

sudo umount "$PART" 2>/dev/null || true
sudo mkfs.fat -F 32 -s 128 -S 512 -n "$LABEL" "$PART"
sudo mkdir -p "$MOUNT"
sudo mount -o noatime,nodiratime "$PART" "$MOUNT"
sync
echo 3 | sudo tee /proc/sys/vm/drop_caches > /dev/null

cd "$SRC"
find . -type f -print0 | xargs -0 ls -S | while read -r f; do
    sudo cp --parents "$f" "$MOUNT/"
done

sync
sudo umount "$MOUNT"

echo "Done"
