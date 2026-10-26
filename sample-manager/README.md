# _core sample manager

_core sample manager opens a folder and keeps its Core hardware files up to date with autosave or the Save button. The same folder holds the editable project, immutable imported originals, bank outputs, and settings. The integrated visualizer uses the last completed output, so there is no separate export or reference-folder step.

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
4. **Saved** means audio and edits are saved and usable. There is no companion generation to wait for. New cards contain primary audio only and require firmware with granular-only Time Stretch support; update the firmware before using newly prepared samples.

Opening a folder displays **Opening folder...x%** as card adoption, file checks, and waveform preparation complete. Progress follows completed work in those stages; it is not an estimate of elapsed time.

Opening a project also prepares compact `.0.wav.wfm` companions for Zeptocore
screen firmware. These contain a low-resolution waveform derived from the same
completed audio used by the integrated visualizer. Missing or stale companions
are prepared automatically, including with Autosave off; pending audio edits
remain pending. Saves update the companion together with rendered audio and
metadata. Older firmware ignores these files. A waveform warning leaves audio
usable. See [screen firmware and file format](../docs/zeptocore-screen.md).

Choose **Project → Default samples → Ezeptocore** or **Ectocore** to download and open the complete default banks. The empty project screen also offers **Default samples…** with both choices. Ezeptocore contains 96 samples in six banks; Ectocore contains 86 samples in nine banks. Both choices are available in every presentation and leave your presentation and device selection unchanged.

Each selection creates a fresh editable project in your Downloads folder, named `Ezeptocore Default Samples` or `Ectocore Default Samples`, with a numbered suffix if that folder already exists. Previous projects and their edits are retained. The manager verifies the ZIP against its pinned SHA-256, caches it in the application's `default-samples` state directory, and checks the cache again before reuse. A verified cache works offline. Downloading happens only after selecting a collection; progress and Cancel appear in the footer. Opening or creating another project cancels the pending automatic switch.

The downloaded project opens automatically after current edits and online analysis finish. If current edits cannot save, the footer gives the new project's location so you can finish or reconcile the current project before opening it. Each installed copy has its own recovery identity and retains its sources, previews, bank settings, sample names, and fresh drum analysis. No additional analysis upload is needed. Copy the project's root contents to an SD card, including hidden `.core-manager` data to retain editability; these primary-only banks require firmware with granular-only Time Stretch support.

File checks read complete SHA-256 hashes through a 256 KiB buffer and use up to four workers. First-time card adoption enumerates each bank once and hashes recognized files concurrently. This retains the full external-change checks while avoiding thousands of unnecessary file probes and tiny disk reads. Waveform analysis also uses buffered reads; its existing cache is reused on later opens.

The toolbar starts with **Import**, **Save**, the **Undo** and **Redo** arrow buttons, and **Autosave**. History buttons remain visible and are disabled when their action is unavailable; their shortcuts are Ctrl/Cmd+Z and Ctrl/Cmd+Shift+Z. The right-hand group contains **Project**, **Settings**, **Visualizer**, **Device**, and the **More** ellipsis. The current project path appears underneath, with its full path available on hover.

Banks and sample slots occupy two adjacent vertical strips with matching tile sizes. The selected bank shares its background with the sample strip. Sample tiles show 01–16 and a filename when recorded; hover to read a long filename in full. Slots use 0–15 in hardware filenames. When a project loads without a selected sample, the first occupied slot in its first nonempty bank is selected automatically. Switching banks restores the last selected sample in each bank while that project is open. If there is no remembered sample, the first occupied slot is selected automatically; empty banks stay unselected. This also works when clicking the initially active bank. **Project → Reveal folder** opens the project folder; **Project → Recent projects** reopens a previous folder. Missing recent folders are disabled. **Project → Duplicate project…** copies the complete portable project into a new empty folder after saving finishes. Existing unused files are copied with the project.

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

Keep `.core-manager` when moving or copying an editable project. Its manifest uses relative paths and stable sample IDs. Source files are addressed by content hash and never stretched in place. Existing cards are adopted without rewriting their audio; unpadded `.0.wav` audio becomes a clearly identified recovered source when no original exists. Recording resolution and pre-normalized recordings cannot be recovered from a card. Damaged or unsupported entries remain protected from editing. Select one and use **Remove…** to clear only that slot after confirmation; its exact files are retained in recovery and can be restored with Undo, including after saving and reopening. **More → Reveal recovery files** opens the backups. Clear bank continues to retain protected slots. Unrelated files, savefiles, unknown settings, and unmanaged legacy companions are preserved.

Copying whole bank folders preserves sample names automatically, even without `.core-manager`. Each numbered sample has a UTF-8 sidecar such as `bank1/0.name.json`, shared by its primary WAV and normal-speed alternative variants. When copying an individual sample, include its sidecar along with its numbered WAV and `.info` files. If you change its slot number, change the sidecar's slot number too. The website includes these sidecars in full sample packs; settings-only packs are unchanged.

The sidecar records an editable display name, the imported basename (including the extension, or the XRNI archive filename), and the SHA-256 of the complete primary `.0.wav`. Renaming changes only the display name. Newly merged samples and older projects with no recorded original filename use an empty original filename. Older cards without recorded names retain generic labels. Existing native projects keep their names and automatically save missing sidecars through the normal transaction.

Recovery uses only the sidecar in the same slot whose audio hash matches. Missing or unusable metadata never prevents audio from opening. Malformed, unsupported, oversized (over 64 KiB), and mismatched sidecars are preserved with a warning that filename persistence was skipped; repair those files outside the app, then reconcile to resume filename persistence. Explicitly removing a slot with unusable filename metadata asks for confirmation and archives its numbered sidecar along with its audio; Undo restores the exact bytes. An intact native project remains authoritative for names while its primary audio is unchanged. Replaced audio gets its matching sidecar names or a generic label. The sidecars do not change numbered WAV filenames, binary `.info` formats, or firmware requirements.

## Save while editing

**Autosave** starts enabled and waits one second after your last completed edit. Turn it off to collect changes and save them together with **Save** or **Ctrl/Cmd+S**. Both the toolbar and shortcut submit pending text fields before saving. Autosave is remembered across launches and projects.

The footer distinguishes **Unsaved changes**, **Saving…**, **Saved**, and **Save failed**. Saving runs independently of editing, so you can keep moving markers while card writes are in progress. A completed save advances the saved revision without replacing newer edits. Turning autosave off lets an active save finish and leaves subsequent edits pending.

Local crash recovery continues in manual mode. It checkpoints edits and undo/redo history after a short pause and schedules a checkpoint at least once a second during continuous editing. Imported originals can still be copied into the hidden project storage, but manual edits do not update bank outputs until Save. Preview uses the last completed rendering; changed audio settings display **pending save** until rendering is requested.

Closing or switching projects with pending edits offers **Save**, **Discard**, and **Cancel**. Discard returns to the saved project, keeps its saved undo history, and clears pending recovery so those edits cannot reappear. Duplication and cleanup require saved state and offer Save or Cancel. If saving fails, your edits remain available and automatic retries pause; fix the problem and use Save or **More → Retry pending save**.

Saves verify complete hashes for affected slots and settings, including existing audio in a slot whose metadata changes. Moves check both locations. Opening and **Reload / reconcile** check the complete card. Changes elsewhere on the card are preserved and detected during that complete check. Marker-only saves retain WAV bytes and reuse waveform/spectrum analysis.

## Edit and preview

Imported samples display a source waveform before primary rendering and saving finish. Once the primary audio is saved, playback and the device visualizer become available and the editor switches to the completed waveform. Source peaks are kept in memory and shared for identical imported audio, and pending imports regain their waveform when reopened.

During batch imports, the footer shows the current filename and file count, followed by waveform preparation, primary audio processing, and saving. Up to four files render concurrently, with a completed-file count in the footer. Identical samples share their render cache safely. The whole batch becomes **Saved** after saving; its input order, bank slots, and single undo step are retained. The activity bar is animated while import, processing, and saving are in progress.

Below the waveform, interaction tips come first, followed by editing tools, source/channel/playback settings, playback options and sample actions. **Advanced** starts collapsed and remembers its expansion across sample and bank selections for the current session. It contains tempo processing (Render BPM and Preserve pitch), variable splice timing, auto-slice tuning and online analysis. Collapsing it keeps all settings and lets the waveform grow. At narrow widths, related controls wrap together; when expanded controls need more height, they scroll below the waveform.

Click within a slice to audition it. **Play / Stop** previews the whole completed rendering. Right-click adds a slice boundary; drag moves it; double-click removes it. Manual slice-boundary edits enable **Variable splice timing**, which stays on through subsequent edits until you turn it off or choose **Even slices**. Editing transient markers alone leaves that setting unchanged. The mouse wheel zooms, and Shift-wheel or middle-drag pans. Select Kick, Snare, or Other in the marker selector to add, move, or remove transient markers. **Snap to slices** starts enabled and aligns new or dragged transients with a slice start within 10 display pixels, at any zoom level. Hold **Option/Alt** to bypass it temporarily. The current snap target is highlighted. Snapping is remembered across launches and projects, and moving a slice later leaves existing transients at their stored positions. A completed drag and its timing setting form one undo operation.

**Even slices** divides the sample into the requested count, recalculates the fixed splice interval from Source BPM and sample length using the website's 24-tick rounding, and turns off **Variable splice timing**. **Auto slice**, beside it, uses the same count as a target and turns on **Variable splice timing**. Auto slicing uses the local onset detector, starting with HFC and a 15 ms refinement window, and selects the strongest detected attacks. The first slice starts at zero, so a target of 16 needs 15 internal cuts. Sparse audio or minimum-spacing limits can yield fewer slices. Detector method and minimum spacing remain in the advanced controls. Slice markers, their interval and timing toggle change together, including through undo/redo. Local slicing never contacts a service. Imports preserve supported WAV cue/loop markers, XRNI slices, OP-1 AIFF markers, and the website's custom AIFF/Ogg slice metadata.

**Source BPM** is initialized from an explicit filename label such as `135 bpm`, `bpm135`, or `123.5BPM`, then embedded tempo metadata. For unlabeled loops, the app first checks whether duration fits one plausible integer tempo with 2/4/8/16/... beats. Ambiguous lengths use the vendored SoundTouch BPM detector and even-beat loop alignment. If SoundTouch cannot resolve a tempo, the app compares repeating attacks in the audio, including half- and double-beat intervals. Each analysis pass reads at most the first 45 seconds in bounded blocks and can be cancelled. Half/double-time ambiguity is inherent; check the result for unusual meters or very slow/fast loops. Silence, very short audio, or failed detection retain 120 as a fallback; `[TEMPO]` logs identify the source of the estimate. Existing project tempos are retained.

Leave **Render BPM** empty to retain the source tempo. Setting a render BPM changes duration by `source BPM / render BPM`; **Preserve pitch** is initially off, so this changes speed and pitch. Enabling it uses offline Rubber Band's finer engine with stereo channels together. Renders always start from the immutable original. Slice and transient positions follow the rendered timeline, and preview uses the same completed rendering as the output. **Tempo matching** separately controls subsequent behavior on the instrument.

The subtitle always shows **Original** and **Rendered** durations, even when they match, followed by the output sample rate. **Rendered** is the actual unpadded audio length heard in preview. While the selected sample's audio changes, the subtitle shows **Target** and **rendering…** with autosave, or **pending save** in manual mode; Play continues to use its last completed render, as explained in the tooltip. A failed update shows **Last rendered** and **Update failed**. Changes to another sample do not change a completed primary's status. Hover over the duration to see how the saved hardware WAV's extra padding differs from preview. Longer subtitles wrap beside the visualizer dock.

Channel mode, playback mode, one-shot behavior, and variable timing retain their card-format meaning. Splice ticks remain stored in the card format but are hidden in the editor, matching the website. New imports calculate that interval using 192 ticks per beat and the rounded loop beat count from Source BPM and the unpadded source duration; one-shot imports use one beat, matching the website's import calculation. Existing saved or adopted intervals are retained; choose **Even slices** again to recalculate an older sample's interval. Newly rendered audio is normalized to −6 dB peak, written as canonical 16-bit PCM at 44.1 kHz, and circularly padded by half a second at both ends. Adopted 88.2 kHz settings are retained. Every playback mode uses the primary WAV. Firmware performs live granular stretching from 1× to 16×; the triggered Time Stretch effect uses 8×. Pitch-preserving tempo edits in the manager still use offline Rubber Band processing.

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

**Startup playback**, available for Ezeptocore and Ectocore in both managers,
defaults to **Play immediately (default)**. Choose **Wait for clock or button**
to power up with the sample loaded, audio silent, and clock/trigger outputs idle.
The normal start gesture or first incoming clock pulse starts playback. Once
started, the existing incoming-clock stop/continue setting controls clock loss.
Changing samples, banks, or presets does not reapply the startup setting.
Restart the device with the updated card to apply it; supporting firmware is
required, and older firmware ignores it. Zeptocore keeps its existing behavior.

Both managers write `settings/startup_playback` containing `play` or `wait`,
followed by a newline. Full and settings-only web exports include the file,
including when switching back to Play immediately. A root-level file is also
accepted; the settings-directory file takes precedence. Missing or invalid
values use immediate playback. The desktop manager retains the choice through
undo/redo, reopening, duplication, and presentation changes.

**Start tempo** appears in all three presentations and in the web managers. Leave it
at **Default (no override)** to keep the existing startup behavior, including a
saved preset's tempo. Choose **Fixed BPM** and enter a whole number from 30 to 300
to start at that tempo instead; the first suggested value is 130. Press Enter or
leave the BPM field to save. Copy web exports onto the card and restart the device
to apply the setting. Supporting firmware is required; older firmware ignores it.

The fixed value applies only at startup. Tap tempo, Zeptocore tempo controls,
external clock/MIDI, runtime preset recall, and the EZEPTOCORE/Ectocore combination
to restore the current sample's tempo keep working normally. Changing samples or
banks does not reapply it. Sample BPM metadata and tempo matching are unchanged.

Both managers write `settings/start_tempo` containing `default` or a decimal BPM,
followed by a newline. Full and settings-only web exports include the file, so
switching back to Default replaces a previous override. A root-level
`start_tempo` file is also accepted, with the settings-directory file taking
precedence. Missing or invalid values disable the override; the native manager
reports invalid values. VCV Rack reads the same file.

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

**Device** opens on the **Firmware** tab by default, with model, version, and build selectors, UF2 downloading and installation, and a four-step guide. **Device → Connection** opens shared MIDI input/output selection, logging, and version query. A unique matching product port pair connects automatically; Ectocore may identify as ezeptocore. Visualization needs the existing opt-in telemetry firmware. Normal firmware can supply legacy status but cannot provide full slice tracking. See the unchanged [firmware opt-in notes](../visualizer/AGENTS.md#firmware-opt-in); firmware build/upload targets remain separate from this app.

In **Device → Firmware**, select your physical **Hardware model**, **Firmware version**, and **Firmware build**, then click **Download UF2**. Opening Device loads available stable releases from GitHub in the background. The newest version with a published UF2 for your hardware is selected by default; older versions remain available in the version menu. Only builds actually published for that model and version are shown, including historical ultra-low-latency options. Normal is the default build when available; choose Visualizer for full device visualization.

Changing the app presentation to Ectocore, Zeptocore, or Ezeptocore also selects that hardware and its latest firmware version, including when Device is hidden. You can still choose hardware manually. During a download, version lookup, installation, or confirmation, the latest presentation choice takes effect when the operation finishes. Ectocore and Ezeptocore use different firmware files even when their MIDI names match.

**Refresh versions** checks for newly published releases while preserving an explicitly selected version if it is still available. If GitHub is unavailable or rate-limits requests, the existing list remains usable; on the first lookup, the app falls back to the version bundled from the root README and labels it **bundled**. There are no release checks at app startup or scheduled background checks.

The app downloads only after you click **Download UF2**, reports progress, and supports **Cancel**. Files are saved to Downloads using the exact published asset URLs and filenames; existing filenames receive a numbered suffix. Only complete, validated UF2 files become available for installation. **Show in folder** reveals the completed download. Closing Device cancels an active download or version lookup. Errors leave previously completed downloads intact.

Validation recognizes both MIDI builds and the USB serial builds used by normal Ectocore/Ezeptocore firmware. Diagnostic runs (`make sample-manager-run` from the repository root) log version lookups and each download's requested build, HTTP status, received byte count, validation result, and saved path under `[FIRMWARE]`.

Follow the illustrated guide: choose hardware, version, and build; click **Download UF2**; connect USB and use **Reset to bootloader** when MIDI is available, or follow the selected model’s **bootloader guide**. A single detected `RPI-RP2` drive is selected automatically; use **Refresh drives** or choose the correct drive if several are connected. Click **Install firmware**, confirm the downloaded filename and destination, and wait for the device to restart.

The installer accepts only the completed download that matches the selected hardware, version, and build. Changing a selector requires a matching download before installation. Files are checked again before copying, including a checksum comparison with the validated download. There is no local UF2 picker. Downloading never automatically resets or installs firmware. Closing Device dismisses pending confirmation; an installation already in progress continues while the window is hidden.

Usable MIDI, motion, and reference-folder preferences from the former native visualizer are imported only when new app preferences do not yet exist.

The display estimates source position and spectrum; it does not measure the instrument's processed audio or model output latency. Existing browser parity fixtures and virtual-MIDI tests have moved into this project. The former standalone/AU/VST3 source and release entrypoints are retired.

## Recovery and external edits

Edits save automatically after a short debounce. Metadata edits leave audio unchanged; slot moves reuse completed audio. Audio and metadata are saved in recoverable transactions. Pending changes and undo history are stored separately from active hardware files.

Existing companion WAVs and their metadata are ignored and left on disk. Missing, damaged, or externally changed companions do not block primary playback or editing. Old pending companion jobs are discarded. Entries protected only by a companion format mismatch are recovered after validating their primary audio; the primary WAV and sample settings are preserved.

Saving, moving, replacing, and removing samples does not rewrite or delete unused companions. **More → Retry pending save** retries failed primary saves. Existing cards work with updated firmware without audio regeneration; this manager does not provide companion exports for older firmware.

Before saving, full file verification uses up to four workers and reuses those verified hashes while planning the same save. Hash reads cancel between buffered chunks when a new edit arrives, allowing queued actions such as Auto slice to proceed. Unchanged filename sidecars are reused. Actual replacements still undergo transaction conflict checks and verification of staged bytes.

Output replacements use staged files, backups, and a recoverable journal on the destination filesystem. The manifest commits after the output set. Multiple renames are not an atomic filesystem operation: reopening rolls back an interrupted transaction before resuming pending work. Only one manager instance can write a project.

Disk-full, permission, and disconnected-volume errors retain completed output and pending state, including a local recovery copy. After fixing the cause, choose **More → Retry pending save**. Externally changed owned files block replacement. **More → Reload / reconcile completed card** adopts current card contents while preserving edits and originals in recovery; the reload can be undone. **Clean unused originals and recovery history** explicitly clears unused data and undo/redo after confirmation.

On macOS, preferences, local recovery, and the last completed visualizer cache live under `~/Library/com.infinitedigits.coresamplemanager/` (the platform's user application-data directory elsewhere). Portable project data remains inside the selected folder.

## Explicit online analysis

**Advanced → Analyze drums online** sends the chosen source as mono 44.1 kHz Ogg to `https://tool.getectocore.com/drumextract`. Like firmware version lookup and downloading, it initiates network requests only after an explicit user action. Collapsing Advanced leaves analysis running and shows an activity indicator beside its label. Reopen it and click **Cancel online analysis** to cancel. Timeouts, invalid replies, or a sample revision change leave current markers usable. The response supplies kick/snare/other markers; downloadable separated stems are outside this API contract.

See [validation evidence and remaining host/device checks](VALIDATION.md) and the [implementation map](IMPLEMENTATION.md).

Save/editing regression scenarios, measured before/after timings, and the optional SD-card benchmark command are recorded in [Editing workflow validation](Tests/EditingWorkflowValidation.md).
