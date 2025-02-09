+++
title = 'Uploading firmware'
date = 2024-02-01T12:32:51-08:00
weight = 12
+++

# Uploading firmware

Periodically, new firmwares are released with updated effects or improved performance. If you have the tool installed, you can connect your device to your computer and automatically update the firmware. You can also download the latest release directly from this link:

### Latest firmware: **[zeptocore v6.2.17](https://github.com/schollz/_core/releases/download/v6.2.17/zeptocore_v6.2.17.uf2)**

## Instructions

To update your zeptocore to the latest version, follow these steps:

1. Connect a USB-C cable to the device and hold the `BOOTSEL` button (located near the USB-C jack).
2. While holding the `BOOTSEL` button, press the `NRST` button (also near the USB-C jack).
3. After a few seconds, a new drive named `RPI-RP2` should appear on your computer.
4. Drag and drop the downloaded **zeptocore_v6.2.17.uf2** file onto the `RPI-RP2` drive.
5. Wait momentarily for the drive to disappear, indicating the update is complete, and the device will turn on automatically.


<div style="max-width: 100%; overflow: hidden; padding: 1em; display: flex; justify-content: center;">
<video controls style="width: 100%; height: auto; max-width: 600px;">
    <source src="/img/button.webm" type="video/webm" />
    Your browser does not support the video tag.
</video>
</div>

Congratulations, your zeptocore has been successfully updated to the latest version. 

(_Note:_ If using [zeptoboard](#zeptoboard), follow the same instructions but ensure you download the latest [zeptoboard_v6.2.17.uf2](https://github.com/schollz/_core/releases/download/v6.2.17/zeptoboard_v6.2.17.uf2)).

### Tool uploader

If you have installed the tool on your computer, launch the tool and click on the top right to upload a new firmware while the device is connected. Download the tool compatible with your operating system from the [latest releases](https://github.com/schollz/_core/releases/latest).

## Firmware development


Follow these instructions if you want to develop the firmware yourself and create custom images.

<details><summary>Windows</summary>

Install WSL 2

```
$ wsl --set-default-version 2
$ wsl --install Ubuntu
```

Then restart computer and run 

```
$ wsl --install
```

That should start your system. Then you can follow the Linux directions.

</details>


<details><summary>macOS</summary>


First install homebrew:

```
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install.sh)"
```

You will need to add Homebrew to your PATH. Do so by running the following two commands:

```
which brew
```

will tell you which path your brew is on. then

```
echo 'eval "$([path to homebrew from command above] shellenv)"' >> /Users/USERNAME/.zprofile (remembering to substitute your username)
eval "$(/opt/homebrew/bin/brew shellenv)"
```

Now you can install the toolchain:

```
brew install cmake python
brew tap ArmMbed/homebrew-formulae
brew install gcc-arm-embedded
```

Now clone the repo and install the Pico SDK

```
git clone https://github.com/schollz/_core
cd _core
export PICO_SDK_PATH=$(pwd)/pico-sdk
git clone -b master https://github.com/raspberrypi/pico-sdk.git
cd pico-sdk && git checkout 1.5.1 && git submodule update --init && cd ..
```

Now you should be able to build zeptocore:

```
make clean zeptocore
```

</details>

<details><summary>Linux</summary>

Install the pre-requisites:

```
sudo apt install cmake gcc-arm-none-eabi \
    libnewlib-arm-none-eabi \
    libstdc++-arm-none-eabi-newlib \
    git python3 g++
sudo -H python3 -m pip install numpy \
    matplotlib tqdm icecream librosa click
```

Clone this repo and install the Pico SDK:

```
git clone https://github.com/schollz/_core
cd _core
git clone https://github.com/raspberrypi/pico-sdk
cd pico-sdk && git checkout 1.5.1 && git submodule update --init && cd ..
export PICO_SDK_PATH=$(pwd)/pico-sdk
```

Do a build:

```
make clean zeptocore
```

(replace 'zeptocore' with 'ectocore' or 'zeptoboard' if you are building a different image)

</details>
