+++
title = 'Uploading samples'
date = 2024-02-01T12:32:51-08:00
weight = 13
+++

# Uploading samples

Uploading samples is a straightforward process similar to transferring a file onto an SD card. However, as the samples need to be processed to meet the specifications of 44.1 kHz / 16-bit, you will require a tool for this preprocessing step before transferring the samples to the SD card. This tool can be accessed online or downloaded to your computer for local use.


### Use tool online: **[zeptocore v2.0.9](/tool)**


<p style="text-align:center">
<img src="/img/core_tool.png" style="width:70%;  margin:0 auto; padding:1em; text-align:center;">
<br><small>Screenshot of the core tool</small><p>

### Download to use tool offline:

<details><summary>Windows</summary>

#### Download for Windows: **[x64](https://github.com/schollz/_core/releases/download/v2.0.9/core_windows_v2.0.9.exe)**

Once downloaded, double click on the executable file to run it.

</details>


<details><summary>macOS</summary>

To install the tool on macOS, first open a terminal.

Then, if you are on an Intel-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v2.0.9/core_macos_amd64_v2.0.9 > core_macos
```

Or, if you are on a M1/M2-based mac install with:

```
curl -L https://github.com/schollz/_core/releases/download/v2.0.9/core_macos_aarch64_v2.0.9 > core_macos
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

#### Download for Linux: **[x64](https://github.com/schollz/_core/releases/download/v2.0.9/core_linux_amd64_v2.0.9)**

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