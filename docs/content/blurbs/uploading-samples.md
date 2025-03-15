+++
title = 'Uploading samples'
date = 2024-02-01T12:32:51-08:00
weight = 13
+++

# Uploading samples

Uploading samples is a straightforward process similar to transferring a file onto an SD card. However, as the samples need to be processed to meet the specifications of 44.1 kHz / 16-bit. 

The sample tool is required for the sample preprocessing step, before transferring the samples to the SD card. This tool can be [accessed online](/tool) or [downloaded to your computer](#download-to-use-tool-offline) for local use.


### Use tool online: **[zeptocore v6.3.7](/tool)** (Chrome and Firefox only)


<p style="text-align:center">
<img src="/img/core_tool.webp" style="width:70%;  margin:0 auto; padding:1em; text-align:center;">
<br><small>Screenshot of the core tool</small><p>

### Stock samples

The zeptocore arrives with "stock" samples on it. If you ever want to remove them and replace them you can get them here by redownloading and transfering them back to the SD card. 

- [zeptocore stock samples pack 1 (drums)](https://infinitedigits.co/zeptocore_default_samples_v6.zip) (393 MB). 
- [zeptocore stock samples pack 2 (melodic + drums)](https://infinitedigits.co/melodic_stock_v6.0.1.zip) (853 MB). 

Make sure you have the firmware updated to v5+ to use these samples.

### Allowable formats

The sample tool will allow a variety of audio formats to be uploaded, including: **.wav**, **.aif** (OP-1 kits), **.flac**, **.mp3**, **.xrni** (Renoise splices), and even **.wav** (morphagene reels).

### Transfering samples to SD card

Once you finish using the tool to collect your samples, click the **Download .zip** button in the lower left corner. This will download a zip file containing all of your samples.

Unzip this **.zip** file extract all the **bankX** folders into the SD card. The SD card structure should look like this:

<div style="text-align:center;">
<img src="/img/folders.webp" style="max-width:100%;">
</div>

Each of those **bankX** folders contains all the nessecary audio files. The **bank0** folder corresponds to the first bank (they are "0-indexed"). If you want a bank to be in a different slot, you can simply rename it. For example, you could rename **bank0** to **bank2** and it would be loaded into the third bank slot.


### Download to use tool offline:

<details><summary>Windows</summary>

#### Download for Windows: **[x64](https://github.com/schollz/_core/releases/download/v6.3.7/core_windows_v6.3.7.exe)**

Once downloaded, double click on the executable file to run it.

</details>


<details><summary>macOS</summary>

To install the tool on macOS, first open a terminal.

Then, if you are on an Intel-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v6.3.7/core_macos_amd64_v6.3.7 > core_macos
```

Or, if you are on a M1/M2-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v6.3.7/core_macos_aarch64_v6.3.7 > core_macos
```

Then to enable the program do:

```
chmod +x core_macos 
xattrc -c core_macos
```

Now to run, you can just type

```
./core_macos
```

A window should pop up in the browser with the offline version of the tool.

</details>


<details><summary>Linux</summary>

#### Download for Linux: **[x64](https://github.com/schollz/_core/releases/download/v6.3.7/core_linux_amd64_v6.3.7)**

After downloading, run it directly from the terminal.

</details>


## Building from source


<details><summary>Windows</summary>

First [install Scoop](https://scoop.sh/) by opening PowerShell terminal and type:

```
> Set-ExecutionPolicy -ExecutionPolicy RemoteSigned -Scope CurrentUser
> Invoke-RestMethod -Uri https://get.scoop.sh | Invoke-Expression
```

Then in the Powershell:

```
> scoop update
> scoop install go zig sox
```

Now you can build directly from Powershell with:

```
> cd core
> $env:CGO_ENABLED=1; $env:CC="zig cc"; go build -v -x
```

Now the upload tool can be run by typing

```
./core.exe
```

</details>



<details><summary>Linux</summary>

Make sure Go is installed and then install *air*:

```
> go install github.com/cosmtrek/air@latest
```

Now you can build the tool using the following:

```
> git clone https://github.com/schollz/_core
> cd _core/core
> go build
```

For development, you will need to have a SSL certificate installed on localhost, otherwise the MIDI won't work.

To get a local SSL server that redirects to the core server, follow these instructions. First install mkcert:

```
sudo apt install libnss3-tools
git clone https://github.com/FiloSottile/mkcert && cd mkcert
go build -ldflags "-X main.Version=$(git describe --tags)"
go install -v
mkcert -install
mkcert localhost
```

Then install and run `local-ssl-proxy`:

```
npm install -g local-ssl-proxy
local-ssl-proxy --key localhost-key.pem --cert localhost.pem --source 8000 --target 8101
```

Now you can run the `core` software:

```
cd core && air
```

And now the server can be accessed at `https://localhost:8000`.

</details>


