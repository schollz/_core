# `_core`

[![build workflow](https://github.com/schollz/_core/actions/workflows/build.yml/badge.svg)](https://github.com/schollz/_core/actions/workflows/build.yml) [![GitHub Release](https://img.shields.io/github/v/release/schollz/_core)](https://github.com/schollz/_core/releases/latest)


this is the monorepo for *_core* music devices that utilize the rp2040. it includes these pieces - the *firmware* and the *[tool](https://zeptocore.com/tool)*.

# firmware

the firmware is the device-specific firmware.

## uploading

follow these instructions to upload the latest firmware to your device.

### button reset

plug in a USB-C cable to the device and hold the `BOOTSEL` button (near the USB-C jack). while holding the `BOOTSEL` button, press the `NRST` button (also near the USB-C jack). after a few seconds a new drive should appear on your computer with a name like `RPI-RP2`.

download [one of the latest releases](https://github.com/schollz/_core/releases/latest) for zeptocore, boardcore, or ectocore and drag and drop it on the the `RPI-RP2` drive that appeared. wait a few more seconds and the drive will disappear and the device will turn on automatically.


### tool uploader

if you installed the tool locally, you can simply run the tool and click in the top right to upload a new firmware, while the device is plugged in.

## firmware development

follow these instructions if you want to develop the firmware yourself and create custom images.


<details><summary>windows</summary>

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


<details><summary>macos</summary>


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

<details><summary>linux</summary>

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

(replace '`zeptocore`' with '`ectocore` or '`boardcore`'' if you are building the latter)

</details>



# tool

the tool is a program that can be used in the cloud (at [zeptocore.com/tool](https://zeptocore.com/tool)) or downloaded and used locally. this tool is for aiding the uploading of the new files to the *_core* devices and it also contains [the docs](https://zeptocore.com/docs).

## cloud-based

to use the tool without installing anything, just go to [zeptocore.com/tool](https://zeptocore.com/tool).

## installing locally


<details><summary>windows</summary>

goto the [latest release](https://github.com/schollz/_core/releases/latest) and download `core_windows_v2.0.1.exe`.

once downloaded, double click on the `.exe` file to run it.

</details>



<details><summary>macos</summary>

to install the `_core` tool on macOS, first open a terminal.

then, if you are on an Intel-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v2.0.1/core_macos_amd64_v2.0.1 > core_macos
```

or, if you are on a M1/M2-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v2.0.1/core_macos_aarch64_v2.0.1 > core_macos
```

then to run, do:

```
chmod +x core_macos && ./core_macos
```


</details>



<details><summary>linux</summary>

goto the [latest release](https://github.com/schollz/_core/releases/latest) and download `core_linux_amd64_v2.0.1`. 

once downloaded, run it from the terminal.


</details>


## development 

<details><summary>windows</summary>

First [install Scoop](https://scoop.sh/), open PowerShell terminal and type:

```PowerShell
> Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
> Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression
```

Then in the Powershell:

```PowerShell
> scoop update
> scoop install go zig sox
```

Now you can build with:

```PowerShell
> cd core
> $env:CGO_ENABLED=1; $env:CC="zig cc"; go build -v -x
```

Now the upload tool can be run by typing

```
./core.exe
```

</details>



<details><summary>linux</summary>

after cloning this repository go into the `core` folder.

make sure Go is installed and then install `air`:

```
> go install github.com/cosmtrek/air@latest
```

now just run 

```
> air
```

and the website will live-reload when developing.

</details>

# devices
there are a few versions of the "*_core*" devices that utilize this firmware:

## boardcore

breadboard version (see [demo](https://www.instagram.com/p/CvzdZTYtV8H/)).

- [WeAct Raspberry Pi Pico (others should work too)](https://www.aliexpress.us/item/3256803521775546.html?gatewayAdapt=glo2usa4itemAdapt) ($2.50)
- [pcm5102](https://www.amazon.com/Comimark-Interface-PCM5102-GY-PCM5102-Raspberry/dp/B07W97D2YC/) ($9)
- [sdio sd card](https://www.adafruit.com/product/4682) ($3.50)

![PXL_20231225_060210016-removebg-preview](https://github.com/schollz/_core/assets/6550035/a33e5fcb-b052-48ba-ab71-0d95d77dea5c)

![boardcore](docs/static/_core_bb.png)

## zeptocore

hand-held version (see [demo](https://www.instagram.com/p/C1PFLGDvB9I/)).

<center>
<img src="https://github.com/schollz/_core/assets/6550035/05e2b34b-efbc-47d1-8ba0-605ad723f85c" width=40%>
</center>


# license

- Apache License 2.0 for no-OS-FatFS (Copyright 2021 Carl John Kugler III)
- MIT license for the SdFat library (Copyright (c) 2011-2022 Bill Greiman)
