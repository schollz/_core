# _core sample manager

_core sample manager opens a folder and keeps its Core hardware files up to date as you edit. The same folder holds the editable project, immutable imported originals, bank outputs, and settings. The integrated visualizer uses the last completed output, so there is no separate export or reference-folder step.

This is a standalone JUCE 9.0.3 application. It runs without a browser, server, Node, Python, SoX, globally installed JUCE, or a sibling source checkout. The existing website and browser/kiosk visualizer remain available.

## Build and run

From the repository root on Apple Silicon:

```sh
cmake --preset macos-arm64 -S sample-manager
cmake --build sample-manager/build/macos-arm64 --parallel 6
ctest --test-dir sample-manager/build/macos-arm64 --output-on-failure
open 'sample-manager/build/macos-arm64/CoreSampleManager_artefacts/Release/_core sample manager.app'
```

Use the `macos-x86_64` preset for Intel. Both target macOS 11 or newer. Build tools are Xcode command-line tools, CMake 3.22+, and Ninja. CMake downloads the pinned JUCE archive and verifies its SHA-256; an existing `.cache/JUCE-9.0.3.tar.gz` supports offline configuration. Rubber Band, SoundTouch's tempo detector, and the local onset detector are vendored and compiled statically. See [source provenance](Vendor/PROVENANCE.md).

The desktop app uses the web sample manager's purple waveform icon. Its source is [Resources/Icons/app-icon.png](Resources/Icons/app-icon.png), copied from `core/src/server/static/icon-512.png`. During CMake configuration, JUCE generates the macOS `.icns` bundled for Finder and the Dock, or the Windows `.ico` embedded in the executable for Explorer, the taskbar, and window icons. Replace the source PNG and reconfigure/rebuild to update both platforms; no separate icon conversion tools are needed.

`make sample-manager-run` from the repository root (or `make -C sample-manager run`) selects the host preset and captures configuration, verbose compiler output, and runtime diagnostics in **`sample-manager/logs/sample-manager.log`** while also printing them in the terminal. The previous run is kept as `sample-manager.previous.log` in the same directory. The app stays attached to the terminal, including on macOS; quit the app to finish the session. Build and application failures still produce a nonzero exit code.

Share the current log when reporting a problem. It includes timestamps, thread IDs, operation timings, local file paths, project loading/saving, waveform analysis, UI actions and layout bounds, slicing targets/results, playback configuration, MIDI connections, and errors. Runtime diagnostics work in Release builds and are enabled by this run command with `CORE_MANAGER_DEBUG=1`. Logs are ignored by version control. Override the destination with `make sample-manager-run LOG_FILE=/absolute/path/session.log`; `PRESET`, `JOBS`, and `VERSION` are also overridable.

Windows uses the PowerShell scripts, and Linux uses the `linux-x86_64` preset. Native Windows/Linux qualification remains a later host step. [Packaging instructions](Release/README.md) include platform dependencies, signing options, and local archive creation.

```sh
python3 scripts/package_sample_manager.py --platform macos-arm64 --version 0.1.0
python3 scripts/package_sample_manager.py --platform macos-x86_64 --version 0.1.0
```

These packaging commands create local artifacts under `sample-manager/dist/` without uploading or flashing hardware. Local application versioning is independent of firmware versioning. The separate [release workflow and scripts](Release/README.md) clone the latest `main` commit, use the latest published release's version, build the standalone app, sign/notarize where applicable, and upload platform-specific assets to that release. Application updates do not require a new tag.

## Use a portable folder

1. In the empty manager, choose **Create project** and enter a new folder name and location, or choose **Open a project folder** to use an existing folder. Creating a project initializes its settings through the same opening flow; cancellation leaves the project unchanged, and existing destinations are refused. **Project → Open folder…** in the toolbar opens an empty folder or an existing Core card folder. **Project → Create project…** is also available when a project is already open. Later launches reopen the last available project.
2. Select one of the 16 banks and use **Import**, or drop audio files into the window. WAV, AIFF/AIF, FLAC, MP3, Ogg, and Renoise XRNI work offline. The first sample in a successful import batch is selected automatically. Files without embedded slice markers start with 16 even slices (fewer for audio too short to encode 16 nonempty slices). Each bank holds 16 samples; an overflowing import reports an error and never wraps to the first slot.
3. Select samples to edit, preview, reorder, merge, remove, or move them to another bank. Shift/Command selection works in the list. A merge follows list order, uses each sample's current audio settings, and leaves the original entries available.
4. **Ready** means the primary audio and edits are saved and usable. Eight-times companions continue in the background; the footer shows how many remain. Preview, editing, visualization, imports, opening another project, and duplication remain available. Wait until the companion count clears before ejecting the card or using its complete eight-times output on hardware.

Opening a folder displays **Opening folder...x%** as card adoption, file checks, and waveform preparation complete. Progress follows completed work in those stages; it is not an estimate of elapsed time.

File checks read complete SHA-256 hashes through a 256 KiB buffer and use up to four workers. First-time card adoption enumerates each bank once and hashes recognized files concurrently. This retains the full external-change checks while avoiding thousands of unnecessary file probes and tiny disk reads. Waveform analysis also uses buffered reads; its existing cache is reused on later opens.

The toolbar starts with **Import**, followed by the **Undo** and **Redo** arrow buttons. History buttons remain visible and are disabled when their action is unavailable; their shortcuts are Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z. The right-hand group contains **Project**, **Settings**, **Visualizer**, **Device**, and the **More** ellipsis. The current project path appears underneath, with its full path available on hover.

Banks and sample slots occupy two adjacent vertical strips with matching tile sizes. The selected bank shares its background with the sample strip. Sample tiles show 01–16 and a filename when recorded; hover to read a long filename in full. Slots use 0–15 in hardware filenames. Switching banks restores the last selected sample in each bank while that project is open. If there is no remembered sample, the first occupied slot is selected automatically; empty banks stay unselected. This also works when clicking the initially active bank. **Project → Reveal folder** opens the project folder; **Project → Recent projects** reopens a previous folder. Missing recent folders are disabled. **Project → Duplicate project…** copies the complete portable project into a new empty folder after primary saving finishes; background companion generation can continue.

```text
project/
  bank1/0.0.wav
  bank1/0.0.wav.info
  bank1/0.1.wav
  bank1/0.1.wav.info
  bank1/0.name.json
  ...
  settings/
  .core-manager/
    project.json
    sources/
    cache/
    transactions/
    recovery/
```

Keep `.core-manager` when moving or copying an editable project. Its manifest uses relative paths and stable sample IDs. Source files are addressed by content hash and never stretched in place. Existing cards are adopted without rewriting their audio; unpadded `.0.wav` audio becomes a clearly identified recovered source when no original exists. Recording resolution and pre-normalized recordings cannot be recovered from a card. Damaged or unsupported entries remain protected, and unrelated files, savefiles, and unknown settings are preserved.

Copying whole bank folders preserves sample names automatically, even without `.core-manager`. Each numbered sample has a UTF-8 sidecar such as `bank1/0.name.json`, shared by its primary WAV and companion variants. When copying an individual sample, include its sidecar along with its numbered WAV and `.info` files. If you change its slot number, change the sidecar's slot number too. The website includes these sidecars in full sample packs; settings-only packs are unchanged.

The sidecar records an editable display name, the imported basename (including the extension, or the XRNI archive filename), and the SHA-256 of the complete primary `.0.wav`. Renaming changes only the display name. Newly merged samples and older projects with no recorded original filename use an empty original filename. Older cards without recorded names retain generic labels. Existing native projects keep their names and automatically save missing sidecars through the normal transaction.

Recovery uses only the sidecar in the same slot whose audio hash matches. Missing or unusable metadata never prevents audio from opening. Malformed, unsupported, oversized (over 64 KiB), and mismatched sidecars are preserved with a warning that filename persistence was skipped; remove or repair those files outside the app, then reconcile to resume persistence. An intact native project remains authoritative for names while its primary audio is unchanged. Replaced audio gets its matching sidecar names or a generic label. The sidecars do not change numbered WAV filenames, binary `.info` formats, or firmware requirements.

## Edit and preview

Imported samples display a source waveform before primary rendering and saving finish. Once the primary audio is saved, playback and the device visualizer become available and the editor switches to the completed waveform, while eight-times companions are generated in the background. Source peaks are kept in memory and shared for identical imported audio, and pending imports regain their waveform when reopened.

During batch imports, the footer shows the current filename and file count, followed by waveform preparation, primary audio processing, and saving. It then shows **Ready (background companions: N remaining)** until the companions finish. An activity bar remains animated while work is in progress without disabling project actions during companion generation.

Below the waveform, interaction tips come first, followed by editing tools, source/channel/playback settings, playback options and sample actions. **Advanced** starts collapsed and remembers its expansion across sample and bank selections for the current session. It contains tempo processing (Render BPM and Preserve pitch), variable splice timing, auto-slice tuning and online analysis. Collapsing it keeps all settings and lets the waveform grow. At narrow widths, related controls wrap together; when expanded controls need more height, they scroll below the waveform.

Click within a slice to audition it. **Play / Stop** previews the whole completed rendering. Right-click adds a slice boundary; drag moves it; double-click removes it. Manual slice-boundary edits enable **Variable splice timing**, which stays on through subsequent edits until you turn it off or choose **Even slices**. Editing transient markers alone leaves that setting unchanged. The mouse wheel zooms, and Shift-wheel or middle-drag pans. Select Kick, Snare, or Other in the marker selector to add, move, or remove transient markers. A completed drag and its timing setting form one undo operation.

**Even slices** divides the sample into the requested count, recalculates the fixed splice interval from Source BPM and sample length using the website's 24-tick rounding, and turns off **Variable splice timing**. **Auto slice**, beside it, uses the same count as a target and turns on **Variable splice timing**. Auto slicing uses the local onset detector, starting with HFC and a 15 ms refinement window, and selects the strongest detected attacks. The first slice starts at zero, so a target of 16 needs 15 internal cuts. Sparse audio or minimum-spacing limits can yield fewer slices. Detector method and minimum spacing remain in the advanced controls. Slice markers, their interval and timing toggle change together, including through undo/redo. Local slicing never contacts a service. Imports preserve supported WAV cue/loop markers, XRNI slices, OP-1 AIFF markers, and the website's custom AIFF/Ogg slice metadata.

**Source BPM** is initialized from an explicit filename label such as `135 bpm`, `bpm135`, or `123.5BPM`, then embedded tempo metadata. For unlabeled loops, the app first checks whether duration fits one plausible integer tempo with 2/4/8/16/... beats. Ambiguous lengths use the vendored SoundTouch BPM detector and even-beat loop alignment, with at most 45 seconds of audio analyzed in bounded blocks. Half/double-time ambiguity is inherent; check the result for unusual meters or very slow/fast loops. Silence, very short audio, or failed detection retain 120 as a fallback; `[TEMPO]` logs identify the source of the estimate. Existing project tempos are retained.

Leave **Render BPM** empty to retain the source tempo. Setting a render BPM changes duration by `source BPM / render BPM`; **Preserve pitch** is initially off, so this changes speed and pitch. Enabling it uses offline Rubber Band's finer engine with stereo channels together. Renders always start from the immutable original. Slice and transient positions follow the rendered timeline, and preview uses the same completed rendering as the output. **Tempo matching** separately controls subsequent behavior on the instrument.

Channel mode, playback mode, one-shot behavior, and variable timing retain their card-format meaning. Splice ticks remain stored in the card format but are hidden in the editor, matching the website. New imports calculate that interval using 192 ticks per beat and the rounded loop beat count from Source BPM and the unpadded source duration; one-shot imports use one beat, matching the website's import calculation. Primary and eight-times companion metadata share the interval. Existing saved or adopted intervals are retained; choose **Even slices** again to recalculate an older sample's interval. The sample subtitle shows duration and output sample rate. Newly rendered audio is normalized to −6 dB peak, written as canonical 16-bit PCM at 44.1 kHz, and circularly padded by half a second at both ends. Adopted 88.2 kHz settings are retained. The eight-times companion is generated unless one-shot is on and tempo matching is off.

Hardware supports at most 255 slices and 16 markers per transient lane, with a bounded 16-frame transient encoding. Counted zero positions are valid loop-start markers and are retained when adopting or editing cards. The project retains full-resolution editing data. An unrepresentable edit reports an actionable compatibility error identifying the bank and sample, and leaves the last completed hardware files intact; reduce or move the markers, or undo the edit, to finish saving.

Keyboard shortcuts: Command/Ctrl-O opens a folder, Command/Ctrl-I imports, Command/Ctrl-Z undoes, Shift-Command/Ctrl-Z redoes, Space previews, and Delete removes selected entries. Text fields keep normal text-editing behavior. Tab navigates controls, and the visualizer/editor divider and main window resize.

## Presentation and device settings

Opening a new folder initializes its `settings` files before any audio is imported. General settings use the existing app defaults; effect bank 1 enables only Time Stretch, and banks 2–7 use the website presets. The initial settings and project manifest are written in one recoverable transaction. Existing card settings are retained.

The presentation selector changes branding and the settings controls shown for each product. It remembers your choice and does not convert audio or reconnect MIDI.

Hover over controls for help with their behavior and relevant shortcuts, including in Settings and Device tools. Waveform help follows the selected slice or transient lane. Tooltips, dropdown fields, open menus and text inputs use the current theme's background, with contrasting text, selection highlights and focus borders. The visualizer uses its own dark theme for its menus and tooltips, both docked and detached.

**One-shot** is visible on Zeptocore and hidden on Ezeptocore and Ectocore. Saved one-shot values are preserved when switching presentations or editing other sample settings while the toggle is hidden. Zeptocore hides the Kick/Snare/Other waveform lanes and their marker-selector options, and the waveform uses the freed lane space. Switching presentations preserves transient markers; Ezeptocore and Ectocore show those lanes again, with a thin separator above Kick that matches the waveform outline.

Ezeptocore and Zeptocore display a header rune from the website's seven symbols, with a stable choice for each project. Ectocore uses its logo.

| Presentation | Appearance | Effect selection |
| --- | --- | --- |
| Ezeptocore, initial default | Gray website palette | Effect banks: I–VII |
| Zeptocore | Purple website palette | Zeptocore knob/MASH controls and terminology |
| Ectocore | Blue palette, restored logo | Grimoire of Breaks: seven runes |

**Settings** includes brightness, clock behavior, CV behavior/polarity, reset override, product-specific knob/MASH options, and the seven-by-sixteen effect matrix in website order. Changing an exclusive marker replaces its on/off alternatives together.

**MIDI receive channel** appears in Zeptocore Settings. Choose 1–16 (default 1);
notes and performance CCs on USB and serial MIDI follow this setting. Clock and
transport remain channel-independent. The manager saves `settings/midi_channel`
automatically; restart the device with that card to apply it. There is no live
channel change or Omni mode. Other presentations hide the control and retain
its value. The web manager writes the same file in full and settings-only packs.
Older firmware ignores the setting.

Firmware with configurable channels uses SysEx for management so all 16 channels,
including channel 10, are safe for musical input. Update the manager and firmware
together: older manager commands no longer work with new Zeptocore firmware.
This manager detects the protocol and supports older firmware after a read-only
version query succeeds. See [MIDI channels and management protocol](../docs/midi-slice-triggering.md).

## Visualizer and local device tools

**Visualizer** is initially off. It opens a dock beside the editor; **Detach** opens a separate window, whose **Full** control enters fullscreen. Close or disable it to stop animation and telemetry lease renewal. Reduced motion is available in the panel and respects macOS's system setting.

- **Device** follows the instrument's MIDI telemetry independently of the sample selected for editing. It preserves estimated playback position, reverse/retrigger handling, effect indicators, physical-pad callouts, stale-state guidance, and source spectrum.
- **Preview** follows the app's actual preview transport and completed sample, without hardware effect or pad activity.

Waveforms, spectra, metadata, and bank mappings are refreshed after completed edits and cached locally. Device visualization remains available when the project card is removed. Editing and unavailable preview reads pause until the folder returns. Opening the visualizer does not open an audio input or request microphone access.

**Device** opens on the **Firmware** tab by default, with model-specific UF2 downloads, a four-step visual installation guide, bootloader reset, and local UF2 tools. **Device → Connection** opens shared MIDI input/output selection, logging, and version query. A unique matching product port pair connects automatically; Ectocore may identify as ezeptocore. Visualization needs the existing opt-in telemetry firmware. Normal firmware can supply legacy status but cannot provide full slice tracking. See the unchanged [firmware opt-in notes](../visualizer/AGENTS.md#firmware-opt-in); firmware build/upload targets remain separate from this app.

In **Device → Firmware**, select your physical **Hardware model** and **Firmware build**, then click **Download UF2**. The initial model follows the app presentation, but you can change it independently; later presentation changes do not change the download selection. Ectocore and Ezeptocore use different firmware even when their MIDI names match. Normal is the default build; choose Visualizer for full device visualization. The build description explains latency and overclocking tradeoffs.

Downloads use the exact version-specific links from the root README bundled when the application was built. They do not check for a newer release or substitute another version when an asset is unavailable. Updating the README links and rebuilding updates the catalog. The app downloads only after you click **Download UF2**, reports progress, and supports **Cancel**. Files are saved to Downloads with their release filenames; existing filenames receive a numbered suffix. Only complete, validated UF2 files are selected for installation. **Show in folder** reveals the completed download. Closing the Device window cancels an active download. Unavailable release assets, interrupted transfers, and invalid files leave the previous selection intact.

Validation recognizes both MIDI builds and the USB serial builds used by normal Ectocore/Ezeptocore firmware. Diagnostic runs (`make sample-manager-run` from the repository root) log the requested build, HTTP status, received byte count, validation result, and saved path under `[FIRMWARE]`.

Follow the illustrated guide: choose hardware and build, download the UF2, connect USB and enter bootloader mode, then flash and wait for reconnection. Use **Reset to bootloader** when MIDI is connected, or open the selected model’s **bootloader guide** for manual entry. Click **Refresh drives** and select the detected `RPI-RP2` volume.

Flashing requires a downloaded or user-selected local UF2, a detected RP2040 bootloader volume, successful file/device checks, and an explicit **Flash selected UF2** action followed by confirmation. Downloading never resets or automatically flashes the device. Usable MIDI, motion, and reference-folder preferences from the former native visualizer are imported only when new app preferences do not yet exist.

The display estimates source position and spectrum; it does not measure the instrument's processed audio or model output latency. Existing browser parity fixtures and virtual-MIDI tests have moved into this project. The former standalone/AU/VST3 source and release entrypoints are retired.

## Recovery and external edits

Edits save automatically after a short debounce. Metadata edits leave audio unchanged; slot moves reuse completed audio. All primary audio in an import batch is saved before companion work begins. Companion jobs yield to queued actions and primary saves, and late results cannot replace a newer revision or recreate a removed sample. Each companion is saved in its own recoverable transaction. Pending companions are recorded in the completed manifest and resume when the project is reopened, including in a duplicate. Closing or switching projects does not require waiting for them. Pending changes and undo history are stored separately from active hardware files.

A companion failure leaves primary playback and editing available. The footer reports the cause; after fixing it, choose **More → Retry pending save** to resume companion generation. Replacing a sample's audio removes its outdated companion until the new one is ready.

Before saving, full file verification uses up to four workers and reuses those verified hashes while planning the same save. Hash reads cancel between buffered chunks when a new edit arrives, allowing queued actions such as Auto slice to proceed. Unchanged filename sidecars are reused. Actual replacements still undergo transaction conflict checks and verification of staged bytes.

Output replacements use staged files, backups, and a recoverable journal on the destination filesystem. The manifest commits after the output set. Multiple renames are not an atomic filesystem operation: reopening rolls back an interrupted transaction before resuming pending work. Only one manager instance can write a project.

Disk-full, permission, and disconnected-volume errors retain completed output and pending state, including a local recovery copy. After fixing the cause, choose **More → Retry pending save**. Externally changed owned files block replacement. **More → Reload / reconcile completed card** adopts current card contents while preserving edits and originals in recovery; the reload can be undone. **Clean unused originals and recovery history** explicitly clears unused data and undo/redo after confirmation.

On macOS, preferences, local recovery, and the last completed visualizer cache live under `~/Library/com.infinitedigits.coresamplemanager/` (the platform's user application-data directory elsewhere). Portable project data remains inside the selected folder.

## Explicit online analysis

**Advanced → Analyze drums online** sends the chosen source as mono 44.1 kHz Ogg to `https://tool.getectocore.com/drumextract`. Like firmware downloads, it initiates network requests only after an explicit user action; the app performs no automatic update checks. Collapsing Advanced leaves analysis running and shows an activity indicator beside its label. Reopen it and click **Cancel online analysis** to cancel. Timeouts, invalid replies, or a sample revision change leave current markers usable. The response supplies kick/snare/other markers; downloadable separated stems are outside this API contract.

See [validation evidence and remaining host/device checks](VALIDATION.md) and the [implementation map](IMPLEMENTATION.md).
