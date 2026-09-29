# Native application packaging

Packaging is local by default and these scripts contain no upload operation. The app version (`CORE_MANAGER_VERSION`, initially `0.1.0`) is independent of firmware. No sibling checkout or global JUCE installation is needed. Python/PowerShell, CMake and build tools are only used to build and package; the application has no dependency on those runtimes.

## macOS

```sh
python3 sample-manager/Release/package.py --platform macos-arm64 --version 0.1.0
python3 sample-manager/Release/package.py --platform macos-x86_64 --version 0.1.0
```

Requires macOS, Xcode command-line tools, Python 3.9+, CMake 3.22+ and Ninja. Both binaries target macOS 11+. Separate Apple Silicon and Intel app bundles are produced. On Apple Silicon, the Intel preset cross-compiles; that does not qualify execution on a physical Intel Mac. Use the same command on an Intel Mac for native build and test execution. `--skip-build` packages an existing matching build after checking its version and architecture.

Following Tape's packaging approach, the script audits Mach-O dependencies, checks bundle metadata, signs the app, verifies the signature and writes an archive, manifest and SHA-256 sums. By default the signature is ad-hoc for local acceptance. Public distribution can explicitly opt into `--sign-identity 'Developer ID Application: ...' --notary-profile PROFILE`. That invokes the existing Keychain credentials, notarizes, staples and checks Gatekeeper. No credentials are read from files, and no signing or notarization was requested for the initial local delivery.

Artifacts appear under `sample-manager/dist/<platform>-<version>-<timestamp>/`. Only an entirely successful package receives `complete.json`. Copy/extract the payload outside the repository before acceptance testing. The ZIP includes the standalone app and required notices; no AU or VST3 is built.

## Windows x64

```powershell
./sample-manager/Release/build-windows.ps1 -Version 0.1.0
./sample-manager/Release/package-windows.ps1 -Version 0.1.0
```

The build script locates Visual Studio's x64 compiler, CMake and Ninja, following Tape's native runner pattern. CMake selects `/MT` (`/MTd` for Debug), including JUCE and Rubber Band. The packaging script verifies the executable version and can require a separately applied Authenticode signature and timestamp with `-RequireSignature`. Unsigned local packaging is the default. Signing credentials and external publication are never implicit. Native Windows execution and signature qualification remain a separate host step.

## Linux x86_64

On an Ubuntu 22.04 or similarly conservative baseline, install build tools and JUCE development dependencies:

```sh
sudo apt-get install build-essential cmake ninja-build pkg-config libasound2-dev \
  libx11-dev libxext-dev libxinerama-dev libxrandr-dev libxcursor-dev libxrender-dev \
  libfreetype6-dev libfontconfig1-dev libcurl4-openssl-dev
python3 sample-manager/Release/package.py --platform linux-x86_64 --version 0.1.0
```

The package includes all resolved non-glibc runtime libraries, including libstdc++, libgcc when needed, ALSA, X11, FreeType, Fontconfig and libcurl plus their dependencies. A relative launcher sets the package library path. The loader and glibc remain host requirements; the manifest records the build baseline. Use `core-sample-manager` to launch the extracted package. System audio devices, display services, fonts and trusted CA certificates remain host services. Packaging requires a Debian/Ubuntu host with `dpkg-query`; it copies each bundled library's installed copyright notice and records package ownership, failing if a notice cannot be identified. Native Linux build/runtime qualification remains a later host step.

## Acceptance

`ctest` runs deterministic format, import, audio, worker/recovery, visualizer and local-tool checks. macOS also runs virtual-MIDI integration tests. These do not reset or flash physical hardware. The packaged `--self-test` uses embedded fixtures and isolated temporary state, so it also runs outside the checkout. `--midi-test` creates temporary virtual MIDI endpoints only. Use the normal app to verify real file drag/drop, themes, slice audition, output playback, editing, settings and docked/detached visualization. Physical instrument playback, SD-card removal timing, UF2 copying and Windows/Linux native-host behavior need deliberate device/host acceptance.
