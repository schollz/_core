+++
title = 'Uploading samples'
date = 2024-02-01T12:32:51-08:00
weight = 13
+++

# Uploading samples

The process for uploading samples is about as easy as transfering a file onto an SD card. However, since the samples need to be processed to make them 44.1 kHz / 16-bit, you will need a tool to process the samples before transfering them to the SD card. You can either use the tool online or download it to your computer to use it locally.


### Use tool online: **[zeptocore v2.0.8](/tool)**


<p style="text-align:center">
<img src="/img/core_tool.png" style="width:70%;  margin:0 auto; padding:1em; text-align:center;">
<br><small>Screenshot of the core tool</small><p>

### Download to use tool offline:

<details><summary>Windows</summary>

#### Download for Windows: **[x64](https://github.com/schollz/_core/releases/download/v2.0.8/core_windows_v2.0.8.exe)**

Once downloaded, double click on the executable file to run it.

</details>


<details><summary>macOS</summary>

To install the tool on macOS, first open a terminal.

Then, if you are on an Intel-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v2.0.8/core_macos_amd64_v2.0.8 > core_macos
```

Or, if you are on a M1/M2-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v2.0.8/core_macos_aarch64_v2.0.8 > core_macos
```

Then to run, do:

```
chmod +x core_macos && ./core_macos
```

A window should pop up in the browser with the offline version of the tool.

</details>


<details><summary>Linux</summary>

#### Download for Linux: **[x64](https://github.com/schollz/_core/releases/download/v2.0.8/core_linux_amd64_v2.0.8)**

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

Now you can bulid the tool using the following:

```
> git clone https://github.com/schollz/_core
> cd _core/core
> air
```

And the website will live-reload when developing.

</details>