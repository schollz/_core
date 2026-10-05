# EZEPTOCORE for VCV Rack 2

One 8 HP module with the original black-and-gold EZEPTOCORE artwork by default. **Ectocore appearance** in the module's right-click menu selects the original blue hardware artwork and its control layout, including the large Break knob and illuminated Tap button. Parameters, cables, sound, and saved automation keep their identities. The plugin slug is `InfiniteDigits` and the module slug is `EZEPTOCORE`.

The EZEPTOCORE panel in `res/ezeptocore.svg` is adapted from the supplied design, preserving its decorative traces, double Amen ring, mode icons, gold output groups, and Infinite Digits / Maneco Labs attribution. Its live knobs, buttons, jacks, and LED lenses follow the artwork's original centers. Monaspace Neon lettering is outlined and the small footer marks are vector contours, so the panel needs no system fonts or embedded bitmaps. The SD slot is decorative.

The Ectocore panel uses the supplied hardware SVG's original vector lettering, ghost ring, geometric background, runes, and output groups. The artwork is cropped to 8 HP in `res/ectocore.svg`; controls and LEDs align with the hardware coordinates. Mock controls, drill guides, the embedded thumbnail, and the hardware-only MIDI legend are omitted. The SD slot is decorative; use **Choose sample folder…** to load samples. No system font installation is needed.

Around Break, the same sixteen RGB LED widgets used by the EZEPTOCORE appearance illuminate the original spiral lines. The frosted surface shows their live colors and brightness spreading softly across adjacent lines, including at the default 50% device brightness. **Device settings → LED brightness** adjusts the illumination immediately, including while a knob indication is held; 0% turns it off. A subtle edge glow follows Rack's halo brightness. The tagged `break-diffuser-*` paths in the panel SVG define these apertures; Ectocore does not draw separate LED bulbs over them.

The plugin compiles the existing firmware's fixed-point renderer, sixteen effects, clock/trigger logic, CV interpretation, performance gestures, and LED logic from `../lib`. It does not run the sample manager or convert samples. This initial version targets Rack 2.6.6 and later compatible Rack 2 releases. MIDI is deferred.

## Install and play

When building from source with the prerequisites below, run `make install-vcv` from the repository root (or `make install-vcv` inside `rack/`). It builds and packages the plugin, creates Rack's platform-specific plugin directory if needed, and copies the `.vcvplugin` there. Restart Rack afterward.

1. Copy the `.vcvplugin` file for your OS and CPU to your Rack user folder's `plugins-<platform>` directory, then restart Rack. The platform names are `mac-arm64`, `mac-x64`, `win-x64`, and `lin-x64`. Use **Help → Open user folder** to locate it.
2. Add **Infinite Digits → EZEPTOCORE**.
3. Right-click its panel and choose **Choose sample folder…**. Select the exported folder containing `bank1`, `bank2`, and so on. Wait for the sample manager to finish preparing the files before loading them.
4. Connect **L OUT** and **R OUT** to your Rack audio interface. Audio uses ±5 V full scale. Clock and transient outputs use 0/10 V; the clock input uses a 1 V rising threshold and a 0.1 V falling threshold.

The Sample/Tunnel knob selects samples. Break controls effect probability, Grimoire/Effects selects one of seven effect banks, Amen controls sequence behavior, and Random/Jump controls jump probability or the Amen CV range. The firmware's Mode, Mult, Bank, and Tap button combinations and shifted knob functions remain available. See the hardware manuals in the repository's main README for their complete performance mappings.

A mouse can hold one button while turning a knob. For combinations involving multiple buttons, right-click a button and select **Temporarily hold**; an orange outline shows the hold. Release it from the same menu or use **Release held buttons** on the module menu. Holds clear when Rack loses focus, when a patch loads, and when the module reboots. They are never saved in a patch.

CV inputs are calibrated virtual inputs: nominal bipolar −5 to +5 V and unipolar 0 to +5 V are translated to the original ADC scale and then pass through the original control processing. Cable presence comes from Rack. **Device settings** controls polarity, Amen CV behavior, sample mapping (Bank divisions or 1 V/oct), reset-input assignment, clock behavior, and brightness. **Grimoire / effect banks** edits the seven-by-sixteen effect matrix.

**Device settings → Start tempo** sets the tempo used when opening a patch, loading
a first or different sample folder, or rebooting the module. **Default (no override)**
keeps the existing behavior. For a fixed tempo, enter a whole-number BPM from 30
to 300 and press Enter. For example, a patch saved while playing at 145 BPM with
Start tempo set to 130 opens at 130 BPM. Normal tap tempo, sample-tempo reset, and
external clock control remain available afterward.

Start tempo is saved per module in the patch and survives **Reboot module**.
Changing the setting takes effect on the next startup. Reloading a sample folder,
importing settings while playing, or switching samples/banks keeps the playing
tempo. Choosing a different folder imports and applies its startup setting.
Older patches without this setting retain their saved playing tempo.

The manager exports `settings/start_tempo` as `default` or a decimal BPM followed
by a newline. Rack also accepts a root-level `start_tempo`; the settings-directory
file takes precedence. Invalid values produce a warning and disable the override.
Rack reads these files without changing the source folder.

**Stop when clock stops** silences playback and holds clock output at 0 V after the external clock times out (about two input-clock intervals). This applies to square and trigger output, including when the clock follows slices. Enable it on every module in a clock chain to propagate stops; each module waits for its own timeout. Once the chain has stopped, the returning clock restarts each module at the beginning in normal forward playback. Slice sequences, random jumps, and reverse playback retain their selected behavior. With no external clock acquired, the internal clock continues normally.

Clock tempo is retained across stops and isolated irregular intervals. Tempo estimation updates only when two consecutive eligible intervals agree within 2%, then uses the existing smoothing to follow the measured rate. After a restart, playback begins immediately at the previous tempo; confirmation takes two complete input intervals (about half a second at 120 BPM). A rapidly varying clock holds the previous estimate until two intervals agree, while gentle tempo changes continue to track.

## Prepared sample folders

A typical export is:

```
my-card/
  bank1/
    0.0.wav
    0.0.wav.info
    0.1.wav
    0.1.wav.info
    0.name.json          # optional; not needed for playback
    15.0.wav            # sparse slots are supported
    ...
  bank16/
    ...
  settings/
    brightness-50
    amen_cv-bipolar
    sample_cv_mapping
    grimoire/rune1/effect1-on
    ...
```

The reader accepts the manager's PCM16 mono/stereo files at 44.1 or 88.2 kHz, metadata versions 0 and 1, up to 16 banks and 16 slots per bank, up to 255 slices, and 16 transient markers per lane. It validates the RIFF chunks, metadata boundaries, channel/rate agreement, and circular half-second head/tail padding. All playback modes accept primary-only samples; old companions are ignored. Optional `.name.json` files are ignored in this version; the menu identifies samples by bank and slot.

A malformed entry is skipped with a message in the module menu. Other valid samples remain available. If a new folder cannot load, the previous bank stays playable. The active primary bank is loaded into RAM by a worker. Time Stretch reads that resident primary audio through the shared granular engine, with no companion cache or streaming. Continuous stretch reaches 16×; the triggered effect applies an 8× minimum while active. Bank changes are asynchronous, with the previous bank available until the next bank is ready; the new bank starts at its loop boundary.

The plugin opens the source folder for reading only. It does not write WAVs, `.info` files, settings, caches, savefiles, or seek maps there. **Reload sample folder** rereads files while retaining patch settings. Choosing a different folder imports that folder's settings. **Import settings from sample folder** explicitly replaces the patch's device settings and effect banks. Missing rune directories get the engine's initial random effect-bank assignments, matching the firmware's fallback.

## Patches and resets

Rack saves the absolute folder path, panel choice, knob parameters, sample selection, tempo/division, transport/mute, device settings, effect parameters, effect banks, sequence, and random-generator state. The schema uses named JSON fields and explicit integer ranges, so it does not depend on C struct layout or CPU architecture. Unknown future fields are ignored; invalid known values are rejected.

The sample folder is external: move it with your patch, then use **Choose sample folder…** to relink it. Reverb/delay buffers, stream-cache contents, current sample phase, button holds, and hardware flash contents are not embedded. Playback restarts from a loop boundary on reload. **Reboot module** recreates the engine on a worker, including when the module has no UI. Rack's Initialize command also resets the Rack parameters.

## Build

Build from this repository or the matching source archive. Required tools: Python 3.10 or later with NumPy, Clang with JSON AST support, a C/C++17 compiler, GNU Make, jq, and zstd. The build generates its DSP tables from the shared firmware scripts; no prior hardware build is needed. macOS also needs the Xcode command-line tools; Linux/Windows builds use the corresponding Rack SDK and native toolchain. Generation must run for the target OS, since the generated translation unit contains that platform's C library declarations.

The generated C engine is compiled with the same Clang used for preprocessing (`CLANG=clang` by default). `CC` and `CXX` select the other C and C++ compilers and may use GCC on Linux/Windows.

From the repository root on Apple Silicon:

```sh
python3 rack/scripts/sdk.py mac-arm64
make install-vcv -j4 CC=clang CXX=clang++
python3 rack/scripts/source.py
```

On Linux x64, from the repository root:

```sh
python3 rack/scripts/sdk.py lin-x64
make install-vcv -j4
```

`make install-vcv` finds the SDK downloaded into `artifacts/rack-sdk/Rack-SDK`, with `artifacts/Rack-SDK` as a fallback. To use another SDK, pass `RACK_DIR=/path/to/Rack-SDK`; a relative path is resolved from the directory where you run Make. To package without installing, use `make -C rack dist`.

The SDK chooses the destination from the build's OS and CPU: `~/Library/Application Support/Rack2/plugins-mac-arm64/` on Apple Silicon, `plugins-mac-x64/` alongside it on Intel Macs, `$XDG_DATA_HOME/Rack2/plugins-lin-x64/` on Linux (default `~/.local/share/Rack2/plugins-lin-x64/`), and `$LOCALAPPDATA/Rack2/plugins-win-x64/` on Windows. For a custom Rack user folder, run `make install-vcv RACK_USER_DIR="/absolute/path/to/Rack2"`. No `sudo` is needed.

Substitute `mac-x64`, `lin-x64`, or `win-x64` for your native platform. For Windows use the MSYS2 MINGW64 shell with GCC, Clang, Python, NumPy (`mingw-w64-x86_64-python-numpy`), Make, jq, zstd, and mingw-w64 toolchain packages. CI builds each platform in `.github/workflows/build-rack.yml` and uploads artifacts; it does not publish a release. Use `CC=clang CXX=clang++` on macOS. Mac Intel can also be built on Apple Silicon with its SDK and `CROSS_COMPILE=x86_64-apple-darwin` after a clean rebuild.

Rack CI runs only on demand, not on pushes or pull requests. Start it from GitHub **Actions → Build Rack plugin → Run workflow**, or run `gh workflow run build-rack.yml --ref main`.

To publish the Windows package, use **Actions → Release Rack plugin - Windows → Run workflow**, or run `gh workflow run release-rack-windows.yml --ref main`. This separate workflow also runs only on demand. It checks out `main`, runs the engine/storage tests, builds Windows x64, and uploads a ZIP plus its SHA-256 checksum to the latest published GitHub release at upload time. The ZIP contains the `.vcvplugin`, matching source, installation instructions, and build metadata. The plugin version comes from `rack/plugin.json`, independently of the release tag; reruns replace only the matching Windows ZIP and checksum assets. The workflow downloads the published assets again and verifies them before reporting success.

For native releases, use `make rack-release-macosarm`, `make rack-release-macos11`, or `make rack-release-linux` from the repository root. Apple Silicon and Linux build locally; Intel builds on `zns@192.168.0.44` over SSH, with packaging and upload on the controller Mac. These commands build a fresh, pinned copy of `main`, keep the plugin version from `rack/plugin.json`, and publish a ZIP with matching source and a checksum to the latest stable release. See [native release instructions](Release/README.md) for prerequisites, host overrides, retained logs, and `--no-upload`.

`make dist` signs macOS binaries ad hoc and adjusts their Rack-library path. Install packaged artifacts instead of copying a raw `plugin.dylib`: a raw SDK-linked binary can load a second Rack library. No developer certificate is required. The native release scripts and manual Windows workflow publish to GitHub; ordinary local build commands do not. These paths do not perform notarization or submit to the VCV Library.

## Architecture and verification

`lib/core_engine/prepare.py` expands the original musical headers, substitutes the desktop hardware boundary, converts the perpetual input loop to a timed step, and uses Clang declaration/reference identities to move mutable globals and persistent locals into `CoreEngine`. The generated state-field inventory is written alongside the generated C. A scoped thread-local pointer selects the current instance only during a synchronous engine call; musical state is owned by each instance. Calls for a given engine must not run concurrently, as with Rack's normal per-module processing contract.

The engine runs at 44,100 frames/sec with the original 441-frame renderer. The host advances its clock from audio frames and compensates for the firmware's hardware timer calibration. Rack's Speex converter handles output resampling. File parsing, primary-bank allocation, engine allocation and reclamation happen off the audio thread. The audio path uses fixed queues, atomics, and resident buffers. Source I/O and errors are handled by the storage worker.

Run:

```sh
python3 test/rack/run.py
python3 test/rack/run.py --sanitize undefined
python3 test/rack/run.py --sanitize address,undefined
python3 test/rack/module.py --sdk artifacts/rack-sdk/Rack-SDK
```

The standalone suite exercises all sixteen effect paths with CV/buttons/clock input, deterministic interleaving and parallel processing of independent engines, thread migration, state validation, settings updates, truncated metadata, sparse slots across all sixteen banks, mono 88.2 kHz samples, missing-folder recovery, primary-only tempo-matched samples, ignored malformed companions, and resident PCM reads against file bytes. It checks source-folder hashes before and after loading. The Rack integration suite checks audible finite output at 32/44.1/48/96/192 kHz, skin and state serialization, and headless reboot. Existing DSP, audio-source and sample-CV regression suites remain applicable.

On Ubuntu, the integration suite also needs the Rack runtime libraries: `libx11-6 libgl1 libasound2 libjack-jackd2-0 libpulse0`.

This is a source-sharing desktop port, not an assertion of measured analog equivalence: ADC noise, DAC output circuitry, hardware bootloader/calibration storage, and USB/TRS MIDI are outside this release. A hardware capture comparison and Windows/Linux runtime listening checks remain release qualification steps. The implementation shares the hardware control paths, but the automated suite does not exhaust every possible button timing combination.

## License

GPL-3.0-only. See `LICENSE.txt` and `THIRD_PARTY.md`. Ship matching source alongside binary packages. No sample packs are bundled.
