# _core sample manager: native sample preparation and integrated visualizer

## 1. Product and architecture

Build **_core sample manager**, a standalone JUCE application that opens a local folder, manages its banks and samples, and continuously writes hardware-compatible files into that folder. The first implementation targets macOS, with a portable build and packaging structure for Windows and Linux.

- Create an independent `sample-manager/` CMake project in this repository, using **JUCE 9.0.3**, pinned and verified by archive checksum. Use `juce_add_gui_app`; no browser, localhost server, Go executable, Python runtime, or external audio-processing tools are required to run it. [JUCE 9.0.3 release](https://github.com/juce-framework/JUCE/releases/tag/9.0.3)
- Keep one app identity and three switchable presentations:

  | Presentation | Appearance and terminology |
  |---|---|
  | Ezeptocore, initial default | Current gray website styling; “Effect banks:” and I–VII |
  | Zeptocore | Current purple website styling and Zeptocore terminology |
  | Ectocore | Historical blue palette, restored logo, “Grimoire of Breaks:” and seven runes |

- Changing presentation updates colors, branding, terminology, and the website’s product-specific settings visibility. It does not convert audio, reset settings, change effect indices, or reconnect hardware. Remember the last selection.
- Open directly into the manager. On first launch, offer a folder chooser within the empty application shell; thereafter reopen the last project when available.
- Use the selected **persistent editor** layout: bank sidebar, sample list, and waveform/settings editor. Include a folder toolbar, device/settings controls, visualizer toggle, and a bottom processing/save-status strip.
- Keep the website’s fonts, symbols, effect ordering, and general visual character. Bundle native font files and artwork; add keyboard navigation, resizable panels, and undo/redo.
- Separate the implementation into project/document state, card-format reading/writing, background audio processing, preview playback, shared device connection, visualizer, and native UI. UI actions operate on stable sample IDs rather than filenames or changing list indices.

Vendor the relevant onset detector, waveform-editing code, exporter primitives, and Rubber Band sources from `amenbreakvst`; use `tape`’s dependency and packaging patterns. Record source revisions and preserve notices. Neither reference checkout becomes a build dependency.

## 2. Portable projects and continuous folder writes

The selected folder is both the editable project and the hardware output root:

```text
Selected folder/
  bank1/
    0.0.wav
    0.0.wav.info
    0.1.wav
    0.1.wav.info
    ...
  bank2/
  ...
  settings/
  .core-manager/
    project.json
    sources/
    cache/
    transactions/
    recovery/
```

**Project storage**

- Store immutable imported originals under `.core-manager/sources/`, addressed by content hash. Keep user-facing names and stable IDs in a versioned project manifest, using relative paths so the folder can move between computers.
- Record bank/slot assignments, sample settings, source and render BPM, slices, transient lanes, device settings, and the last completed output revision.
- Support 16 banks with 16 slots each. Display slots as 1–16 and retain hardware filenames numbered 0–15. A full bank reports overflow; it never wraps around and overwrites slot 0.
- Provide Open Folder, recent folders, Reveal Folder, and Duplicate Project. Duplication creates another complete portable folder, replacing the website’s workspace-copy operation.

**Existing-card support**

- Open existing website-generated folders by reading their WAV/`.info` pairs and settings marker files.
- Recover editable source audio from the unpadded `.0.wav` when no original exists. Identify it as recovered card audio; the app cannot recover the original filename, resolution, or pre-normalized recording.
- Preserve existing hardware files until an edit requires updating them. Preserve unrelated files, firmware savefiles, and unrecognized settings.
- Report damaged or unsupported entries individually. Preserve their files and prevent destructive rewrites of entries the app cannot interpret.
- Use one card-format implementation for importing, exporting, and supplying metadata to the visualizer.

**Saving and recovery**

- Every accepted edit saves automatically. Debounce rapid parameter changes and waveform drags; finalize a drag as one undo operation.
- Metadata-only changes rewrite metadata. Audio changes regenerate only the affected sample’s required outputs. Reordering changes slot assignments without reprocessing audio.
- Use background jobs tagged with project, sample, and revision IDs. Cancel superseded work and prevent stale results from replacing newer edits.
- Stage replacement files on the destination filesystem. Maintain a recoverable transaction journal and backups for changes involving multiple WAVs, metadata files, slot renames, or settings markers. Commit the manifest after the output set is complete.
- On reopening an interrupted project, roll back incomplete transactions before resuming queued rendering. Do not describe multiple file renames as an atomic filesystem operation.
- Permit one writer per project. Detect externally changed files before replacing them and offer reload/reconciliation instead of silently overwriting them.
- Show **Processing**, **Saving**, **Ready**, or an actionable error. Disk-full, permission, and disconnected-volume failures retain the last completed output and pending project changes. “Ready” means all required writes have finished.
- Retain undo/recovery data separately from active hardware files; expose an explicit cleanup operation for unused originals and recovery history.

## 3. Sample editing, audio processing, and compatibility

**Website workflow parity**

Implement drag/drop and file-picker import, multiselection, ordering, removal, bank clearing, merging, whole-sample preview, slice audition, waveform zoom/pan, manual slice boundaries, even slicing, automatic slicing, and kick/snare/other transient editing.

Preserve the current per-sample controls: channel mode, source BPM, tempo matching, one-shot behavior, playback mode, and splice timing settings. Preserve the full device-settings model, including brightness, clock behavior, CV polarity/behavior, reset override, knob/MASH options, and the seven-by-sixteen effect selection matrix.

- Support WAV, AIFF/AIF, FLAC, MP3, and Ogg offline.
- Preserve existing special imports: WAV cue/loop markers, Renoise XRNI sample/slice data, OP-1 AIFF markers, and the website’s custom slice metadata. Parse metadata structurally rather than copying fragile fixed-offset assumptions.
- Merge selected samples in list order, retaining their slice boundaries in the combined source and leaving the original entries available until explicitly removed.
- Reuse the local onset implementation from `amenbreakvst`. Start with HFC and the website’s 15 ms refinement window; put detector method and spacing options in an advanced section. Automatic slicing must not make a network request.
- Use background, block-based decoding, resampling, stretching, waveform generation, and file writing. Do not load an entire bank—or an entire eight-times-longer companion—into RAM.
- Use band-limited resampling, replacing the existing C++ exporter’s linear-resampling shortcut. Keep preview file reads buffered off the audio callback; no analysis, rendering, network requests, or disk writes run in the callback.

**New pitch-preserving BPM conversion**

Add an optional **Render BPM** control beside **Source BPM**, with **Preserve pitch** enabled by default.

- An unset render BPM leaves the sample’s tempo unchanged.
- With preservation enabled, use offline Rubber Band processing with pitch ratio `1.0` and duration ratio `source BPM / render BPM`.
- Use the finer engine with stereo channels processed together. Complete the study/process passes on a worker and drain the output correctly. [Rubber Band integration guidance](https://breakfastquay.com/rubberband/integration.html)
- Preserve the original source permanently and rerender from it when settings change, avoiding accumulated stretching degradation.
- Map slice boundaries and transient positions into the rendered timeline, using key-frame constraints where appropriate. Preview and output must use the same completed rendering.
- Write the rendered BPM into the hardware metadata. Keep this conversion distinct from the existing **Tempo matching** option, which controls subsequent behavior on the instrument.
- With Preserve pitch disabled, perform an explicit speed-and-pitch conversion using the same source/render BPM controls.

**Hardware output**

Keep the existing card format and firmware compatibility for the first version; no v9 firmware change is needed for this workflow.

- Write canonical 16-bit little-endian PCM WAVs with the expected 44-byte header, normal 44.1 kHz output, and half-second circular padding at both ends. Preserve supported 88.2 kHz settings when importing existing cards.
- Preserve the website’s −6 dB peak-normalization behavior for newly rendered output. Handle silence and samples shorter than half a second correctly.
- Generate the required eight-times-longer `.1.wav` companion using the adapted exporter’s Rubber Band path. Retain the current one-shot/no-tempo-match exception.
- Encode `.info` explicitly in little-endian form, including flags, aligned slice offsets, slice types, and transient groups. Do not serialize C++ struct memory.
- Respect firmware limits for slot counts, file sizes, slices, and transient encoding. Keep full-resolution editing data in the project; display a clear compatibility warning for markers that cannot be represented on the device instead of silently wrapping or truncating them.
- Replace mutually exclusive settings marker files together so stale `-on`/`-off` files cannot contradict one another.

**Network access requires an explicit action**

Provide **Analyze drums online** as a user-triggered action. Reuse the existing API’s mono 44.1 kHz request and kick/snare/other marker response. Include cancellation, timeout/error handling, and revision checks so a late response cannot overwrite newer edits. Failed analysis leaves local slicing and existing markers usable. Downloadable separated stem audio is outside the current API contract.

## 4. Integrated visualizer and local device tools

Move the functionality from `visualizer-juce/` into the new application, then retire its separate standalone, AU, and VST3 targets.

- Extract its renderer from the plugin-editor wrapper into a reusable JUCE component.
- Add a **Visualizer** toolbar toggle, initially off. Show a docked panel with an optional separate/fullscreen window and an explicit **Device / Preview** source selector.
- **Device** preserves the existing telemetry parsing, estimated playhead, reverse/retrigger behavior, effect indicators, physical-pad callouts, spectrum, stale-state handling, and reduced-motion support.
- **Preview** follows the app’s actual preview transport and current rendered sample. It does not invent hardware effect state or physical-pad events.
- Keep live device selection independent of the sample currently being edited.
- Share committed project audio, metadata, waveform preparation, and spectrum caches. Refresh affected visualizer data after a completed edit; remove the old separate folder picker and manual rescan requirement.
- Maintain a local cache of the last completed project’s visualizer data and bank mapping. Device visualization remains available after the SD card is removed from the computer. Editing and uncached audio preview pause while the project folder is unavailable.
- Use one shared MIDI connection service for visualization, version queries, reset commands, and device logging. Handle all three product names, including Ectocore hardware presenting as “ezeptocore.”
- Stop telemetry lease renewal and animation work when the visualizer is disabled. Opening the visualizer alone must not open an audio input or request microphone access.
- Keep the existing opt-in visualizer firmware and legacy fallback. Show clear guidance when normal firmware lacks MIDI telemetry.
- Include flashing of a downloaded or user-selected local UF2 to a detected bootloader volume, with device/file checks, progress, and explicit confirmation. Device → Firmware offers user-initiated downloads for Zeptocore, Ectocore, and Ezeptocore using the root README’s version-specific URLs bundled at build time, plus an illustrated installation guide. Downloads save to Downloads and become selected only after validation. No automatic flashing or online firmware checks.
- Import usable preferences from the former visualizer on first launch. Move its relevant protocol/parity fixtures and tests into the new project before removing the old source/build/release entrypoints. Keep the browser/kiosk visualizer and firmware telemetry targets.

## 5. Delivery, validation, and implementation order

**Build and packaging**

- Use C++17, CMake presets, pinned JUCE, and statically built vendored Rubber Band. Embed fonts, icons, and required notices.
- Provide build/run/package commands that work without sibling repositories or globally installed JUCE, SoX, Rubber Band, Node, or Python at application runtime.
- Follow `tape`’s macOS and Windows packaging patterns, with publication disabled by default. Keep app versioning independent of firmware versioning.
- Deliver and validate the native macOS app first. Prepare separate Apple Silicon and Intel builds, Windows x64 with the static MSVC runtime, and Linux x86_64 packaging with required non-system runtime libraries included.
- Adapt the old visualizer packaging and documentation to _core sample manager. Windows/Linux runtime qualification remains a later native-host step; macOS compilation does not count as cross-platform verification.
- Keep the current website available. Make implementation changes in the current checkout, without commits, pushes, releases, deployments, or hardware flashing.

**Ordered implementation**

1. Establish the JUCE app, themes, document model, shared card-format reader/writer, and compatibility fixtures.
2. Implement portable-folder adoption, source storage, background jobs, transactional output, and recovery.
3. Complete bank management, sample editing, preview, local analysis, device settings, and Rubber Band BPM conversion.
4. Integrate Device/Preview visualization, shared MIDI tools, local UF2 handling, and old-preference migration.
5. Finish macOS packaging and acceptance checks, then remove the superseded JUCE visualizer project and update cross-platform build/release entrypoints.

**Validation**

Targeted automated tests are authorized for this new application, replacing the earlier no-tests constraint for this work.

- **Format compatibility:** independently generated website/firmware fixtures; mono/stereo, both supported sample rates, padding, settings flags, slice offsets, one-shot cases, companion files, and numeric boundaries.
- **Storage correctness:** existing-card adoption, reopen, duplicate/reorder/remove/undo, interrupted transactions, disk-full/read-only failures, unplug/reconnect, external edits, and stale background jobs.
- **Audio correctness:** supported imports and embedded markers, short/silent/corrupt files, resampling, normalization, stretch duration, pitch retention, stereo alignment, and marker alignment.
- **Visualizer:** retain existing protocol/parity coverage; add preview-source behavior, committed-revision refresh, missing-folder cache operation, source switching, and disable/re-enable lifecycle.
- **Offline operation:** import, slicing, editing, conversion, saving, reopening, and preview with networking unavailable. Only explicit firmware downloads and online drum analysis may initiate application network requests.
- **macOS acceptance:** launch the packaged app outside the checkout; exercise all three themes, real drag/drop, preview and editing, settings, visualizer modes, and clean shutdown. Leave actual SD-card playback and connected-hardware/UF2 acceptance to a deliberate hardware check.

Completion means the selected folder stays device-ready as edits finish, reopens as a portable editable project, and supplies the integrated visualizer without a separate export or reference-folder workflow.
