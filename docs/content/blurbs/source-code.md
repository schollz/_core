+++
title = 'Uploading firmware'
date = 2024-02-01T12:32:51-08:00
weight = 12
+++

# Uploading firmware

There are periodically new firmwares with updated effects or improved performance. If you have the tool installed, you can connect your device to your computer and automatically flash it. Otherwise, you can download the latest release directly using this link:

### Latest firmware: **[zeptocore v2.0.9](https://github.com/schollz/_core/releases/download/v2.0.9/zeptocore_v2.0.9.uf2)**



Plug in a USB-C cable to the device and hold the `BOOTSEL` button (near the USB-C jack). while holding the `BOOTSEL` button, press the `NRST` button (also near the USB-C jack). after a few seconds a new drive should appear on your computer with a name like `RPI-RP2`.

Drag and drop the downloaded **zeptocore_v2.0.9.uf2** file onto the `RPI-RP2` drive that appeared. Wait a few seconds and the drive will disappear and the device will turn on automatically. Congratulations, you have updated your zeptocore to the latest version. (_Note:_ If using [boardcore](#boardcore) you can follow the same instructions, but make sure to download the latest [boardcore_v2.0.9.uf2](https://github.com/schollz/_core/releases/download/v2.0.9/boardcore_v2.0.9.uf2)).


### Tool uploader

If you installed the tool locally, you can simply run the tool and click in the top right to upload a new firmware, while the device is plugged in. Download the tool for your operating system from the [latest releases](https://github.com/schollz/_core/releases/latest).

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
cd pico-sdk &&  git submodule update --init && cd ..
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
cd pico-sdk &&  git submodule update --init && cd ..
export PICO_SDK_PATH=$(pwd)/pico-sdk
```

Do a build:

```
make clean zeptocore
```

(replace 'zeptocore' with 'ectocore' or 'boardcore' if you are building a different image)

</details>
