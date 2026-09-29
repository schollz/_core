# Core Sample Manager

Core Sample Manager opens a folder and keeps its Core hardware files up to date as you edit. The same folder holds the editable project, immutable imported originals, bank outputs, and settings. The integrated visualizer uses the last completed output, so there is no separate export or reference-folder step.

This is a standalone JUCE 9.0.3 application. It runs without a browser, server, Node, Python, SoX, globally installed JUCE, or a sibling source checkout. The existing website and browser/kiosk visualizer remain available.

## Build and run

From the repository root on Apple Silicon:

```sh
cmake --preset macos-arm64 -S sample-manager
cmake --build sample-manager/build/macos-arm64 --parallel 6
ctest --test-dir sample-manager/build/macos-arm64 --output-on-failure
open 'sample-manager/build/macos-arm64/CoreSampleManager_artefacts/Release/Core Sample Manager.app'
```

Use the `macos-x86_64` preset for Intel. Both target macOS 11 or newer. Build tools are Xcode command-line tools, CMake 3.22+, and Ninja. CMake downloads the pinned JUCE archive and verifies its SHA-256; an existing `.cache/JUCE-9.0.3.tar.gz` supports offline configuration. Rubber Band and the local onset detector are vendored and compiled statically. See [source provenance](Vendor/PROVENANCE.md).

`make -C sample-manager run` selects the host preset; `PRESET`, `JOBS`, and `VERSION` are overridable. Windows uses the PowerShell scripts, and Linux uses the `linux-x86_64` preset. Native Windows/Linux qualification remains a later host step. [Packaging instructions](Release/README.md) include platform dependencies, signing options, and local archive creation.

```sh
python3 scripts/package_sample_manager.py --platform macos-arm64 --version 0.1.0
python3 scripts/package_sample_manager.py --platform macos-x86_64 --version 0.1.0
```

These commands create local artifacts under `sample-manager/dist/`. Application versioning is independent of firmware versioning. Publication is disabled; packaging never uploads a release or flashes hardware.

## Use a portable folder

1. Choose **Open Folder**, then select an empty folder or an existing Core card folder. The first launch offers a chooser within the manager shell. Later launches reopen the last available project.
2. Select one of the 16 banks and use **Import**, or drop audio files into the window. WAV, AIFF/AIF, FLAC, MP3, Ogg, and Renoise XRNI work offline. Each bank holds 16 samples; an overflowing import reports an error and never wraps to the first slot.
3. Select samples to edit, preview, reorder, merge, remove, or move them to another bank. Shift/Command selection works in the list. A merge follows list order, uses each sample's current audio settings, and leaves the original entries available.
4. Wait for **Ready** before using the hardware output. Processing and Saving indicate unfinished work. Ready means the output transaction and completed manifest have finished.

Slots appear as 1–16 in the app and use 0–15 in hardware filenames. **Reveal** opens the project folder; **Recent** reopens a previous folder. **Duplicate** copies the complete portable project into a new empty folder.

```text
project/
  bank1/0.0.wav
  bank1/0.0.wav.info
  bank1/0.1.wav
  bank1/0.1.wav.info
  ...
  settings/
  .core-manager/
    project.json
    sources/
    cache/
    transactions/
    recovery/
```

Keep `.core-manager` when moving or copying an editable project. Its manifest uses relative paths and stable sample IDs. Source files are addressed by content hash and never stretched in place. Existing cards are adopted without rewriting their audio; unpadded `.0.wav` audio becomes a clearly identified recovered source when no original exists. Original filenames, recording resolution, and pre-normalized recordings cannot be recovered from a card. Damaged or unsupported entries remain protected, and unrelated files, savefiles, and unknown settings are preserved.

## Edit and preview

Click within a slice to audition it. **Play / Stop** previews the whole completed rendering. Right-click adds a slice boundary; drag moves it; double-click removes it. The mouse wheel zooms, and Shift-wheel or middle-drag pans. Select Kick, Snare, or Other in the marker selector to add, move, or remove transient markers. A completed drag is one undo operation.

**Even slices** divides the sample into the requested count. **Auto slice** uses the local onset detector, starting with HFC and a 15 ms refinement window. Detector method and minimum spacing are in the advanced controls. Local slicing never contacts a service. Imports preserve supported WAV cue/loop markers, XRNI slices, OP-1 AIFF markers, and the website's custom AIFF/Ogg slice metadata.

**Source BPM** describes the source. Leave **Render BPM** empty to retain its tempo. Setting a render BPM changes duration by `source BPM / render BPM`; **Preserve pitch** is initially enabled and uses offline Rubber Band's finer engine with stereo channels together. Disabling it explicitly changes speed and pitch. Renders always start from the immutable original. Slice and transient positions follow the rendered timeline, and preview uses the same completed rendering as the output. **Tempo matching** separately controls subsequent behavior on the instrument.

Channel mode, playback mode, one-shot behavior, splice ticks, and variable timing retain their card-format meaning. Newly rendered audio is normalized to −6 dB peak, written as canonical 16-bit PCM at 44.1 kHz, and circularly padded by half a second at both ends. Adopted 88.2 kHz settings are retained. The eight-times companion is generated unless one-shot is on and tempo matching is off.

Hardware supports at most 255 slices and 16 markers per transient lane, with a bounded 16-frame transient encoding. The project retains full-resolution editing data. An unrepresentable edit reports an actionable compatibility error and leaves the last completed hardware files intact; reduce or move the markers, or undo the edit, to finish saving.

Keyboard shortcuts: Command/Ctrl-O opens a folder, Command/Ctrl-I imports, Command/Ctrl-Z undoes, Shift-Command/Ctrl-Z redoes, Space previews, and Delete removes selected entries. Text fields keep normal text-editing behavior. Tab navigates controls, and the list/editor divider and main window resize.

## Presentation and device settings

The presentation selector changes branding and the settings controls shown for each product. It remembers your choice and does not convert audio or reconnect MIDI.

| Presentation | Appearance | Effect selection |
| --- | --- | --- |
| Ezeptocore, initial default | Gray website palette | Effect banks: I–VII |
| Zeptocore | Purple website palette | Zeptocore knob/MASH controls and terminology |
| Ectocore | Blue palette, restored logo | Grimoire of Breaks: seven runes |

**Settings** includes brightness, clock behavior, CV behavior/polarity, reset override, product-specific knob/MASH options, and the seven-by-sixteen effect matrix in website order. Changing an exclusive marker replaces its on/off alternatives together.

## Visualizer and local device tools

**Visualizer** is initially off. It opens a dock beside the editor; **Detach** opens a separate window, whose **Full** control enters fullscreen. Close or disable it to stop animation and telemetry lease renewal. Reduced motion is available in the panel and respects macOS's system setting.

- **Device** follows the instrument's MIDI telemetry independently of the sample selected for editing. It preserves estimated playback position, reverse/retrigger handling, effect indicators, physical-pad callouts, stale-state guidance, and source spectrum.
- **Preview** follows the app's actual preview transport and completed sample, without hardware effect or pad activity.

Waveforms, spectra, metadata, and bank mappings are refreshed after completed edits and cached locally. Device visualization remains available when the project card is removed. Editing and unavailable preview reads pause until the folder returns. Opening the visualizer does not open an audio input or request microphone access.

**Device** opens shared MIDI input/output selection, logging, version query, reset, and local UF2 tools. A unique matching product port pair connects automatically; Ectocore may identify as ezeptocore. Visualization needs the existing opt-in telemetry firmware. Normal firmware can supply legacy status but cannot provide full slice tracking. See the unchanged [firmware opt-in notes](../visualizer/AGENTS.md#firmware-opt-in); firmware build/upload targets remain separate from this app.

Flashing requires a user-selected local UF2, a detected RP2040 bootloader volume, successful file/device checks, and an explicit **Flash selected UF2** action. The app never downloads firmware or automatically flashes it. Usable MIDI, motion, and reference-folder preferences from the former native visualizer are imported only when new app preferences do not yet exist.

The display estimates source position and spectrum; it does not measure the instrument's processed audio or model output latency. Existing browser parity fixtures and virtual-MIDI tests have moved into this project. The former standalone/AU/VST3 source and release entrypoints are retired.

## Recovery and external edits

Edits save automatically after a short debounce. Metadata edits leave audio unchanged; slot moves reuse completed audio. Superseded render jobs are canceled, and late results cannot replace a newer revision. Pending changes and undo history are stored separately from active hardware files.

Output replacements use staged files, backups, and a recoverable journal on the destination filesystem. The manifest commits after the output set. Multiple renames are not an atomic filesystem operation: reopening rolls back an interrupted transaction before resuming pending work. Only one manager instance can write a project.

Disk-full, permission, and disconnected-volume errors retain completed output and pending state, including a local recovery copy. After fixing the cause, choose **More → Retry pending save**. Externally changed owned files block replacement. **More → Reload / reconcile completed card** adopts current card contents while preserving edits and originals in recovery; the reload can be undone. **Clean unused originals and recovery history** explicitly clears unused data and undo/redo after confirmation.

On macOS, preferences, local recovery, and the last completed visualizer cache live under `~/Library/com.infinitedigits.coresamplemanager/` (the platform's user application-data directory elsewhere). Portable project data remains inside the selected folder.

## Explicit online analysis

**Analyze drums online** sends the chosen source as mono 44.1 kHz Ogg to `https://tool.getectocore.com/drumextract`. It is the only application action that initiates a network request. Click again to cancel. Timeouts, invalid replies, or a sample revision change leave current markers usable. The response supplies kick/snare/other markers; downloadable separated stems are outside this API contract.

See [validation evidence and remaining host/device checks](VALIDATION.md) and the [implementation map](IMPLEMENTATION.md).
