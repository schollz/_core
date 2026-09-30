# _core sample manager implementation map

The application implements the accepted [specification](SPECIFICATION.md) as an independent JUCE 9.0.3 project. Version 0.1.0 has native Apple Silicon acceptance and separate Apple Silicon/Intel packages. [VALIDATION.md](VALIDATION.md) records the evidence and remaining host/device checks.

## Ownership and data flow

| Area | Primary sources | Responsibility |
| --- | --- | --- |
| Project | `Source/Project.*` | Versioned manifest, stable IDs, assignments, sample/device settings, full-resolution markers and completed revision |
| Card format | `Source/CardFormat.*` | Canonical WAVs, explicit little-endian `.info`, limits and settings marker interpretation |
| Storage | `Source/Storage.*`, `Common.h` | Hashed originals, adoption, writer lock, external fingerprints, staged replacements, journal recovery, duplication and cleanup |
| Worker | `Source/Manager.*` | Debounce, undo/redo, revision checks, cancellation, pending recovery, committed snapshots and status |
| Audio | `Source/AudioProcessing.*` | Block decoding, structural import metadata, sinc resampling, offline Rubber Band, normalization/padding, merging and onset detection |
| Preview | `Source/Preview.*` | Buffered completed-audio playback, slice audition, output-only audio and actual transport position |
| Native UI | `Source/AppView.*`, `WaveformEditor.*`, `SettingsView.*`, `Theme.*` | Bank/list/editor layout, three presentations, markers, settings, navigation and preference migration |
| Device tools | `Source/Device.*`, `DeviceView.*`, `Firmware.*`, `FirmwareView.*`, `Uf2.*`, `Visualizer/Midi.*` | Shared MIDI, commands/logs, README-pinned firmware downloads, illustrated guide and explicit local UF2 validation/copy |
| Visualizer | `Source/Visualizer/{Core,Library,Session,VisualizerView}.*` | Migrated protocol/playhead/rendering, committed caches, Device/Preview selection and lifecycle |
| Online analysis | `Source/OnlineAnalysis.*` | Explicit mono 44.1 kHz Ogg request, cancellation/timeouts, bounded parsing and stale-revision rejection |

UI commands address stable sample IDs. The worker owns storage mutations and rendering. Pending state is saved before work; a completed transaction advances the committed snapshot used by playback and visualization. The audio callback reads a buffered transport and performs no analysis, network request or write.

Primary rendering and saving complete before eight-times companion work. The same worker processes one interruptible companion at a time only when no user commands or primary saves are pending. Companion work never sets the editor's `busy` flag; `backgroundBusy`, `pendingCompanions`, and `companionError` report its separate progress. The per-sample `companionPending` manifest field survives reopening and duplication. A companion transaction updates only that sample's `.1.wav`, `.1.wav.info`, ownership/fingerprints and manifest; it neither advances user-edit revisions nor rebuilds primary waveforms. Commands cancel audio and hash work between blocks before changing project state, so old results cannot land after edits, moves, removal, or a project switch.

## Storage and processing invariants

- Originals are content-addressed under `.core-manager/sources/`; manifest paths are relative. Rendering never modifies an original. Adoption identifies recovered card audio and its reduced provenance.
- One writer owns a project. Unsupported entries and unrelated files are retained. External fingerprints are checked before staging and again before replacement. Conflicts stop saving; reconciliation preserves pending/completed state in recovery and remains undoable.
- Staging and backups use the destination filesystem. A durable journal records replacements; outputs commit before the manifest. Reopening rolls back interrupted replacements. Multiple renames are not claimed to be atomic.
- Local recovery retains pending edits when project storage is full or unavailable. Failed saves do not advance the completed revision. Directory creation refuses to recreate a disappeared selected root. Retry resumes after the cause is resolved.
- Undo/recovery are separate from hardware files. Explicit cleanup requires confirmation. Slot changes reuse completed audio.
- Audio decoding, resampling, waveform generation, Rubber Band and writing use bounded blocks. Preserve-pitch conversion uses the finer engine, channels together, pitch ratio 1.0 and complete offline study/process/drain passes.
- Render cache identity covers audible processing and immutable anchors captured at import/adoption/merge. Editable markers do not unnecessarily invalidate audio. Hashes are verified before reuse. Metadata-only edits preserve WAV bytes and modification times; one-shot/tempo-match changes separately control companion requirements.
- Primary cache completion never certifies partial companion files left by cancellation. Companion failures retain usable primary output and report separately from failed primary saves; Retry resumes them.
- Merging uses current audio settings in list order, maps markers and retains original entries. Unrepresentable hardware markers remain in the project while the last compatible output stays intact.

## Visualization and delivery

The reusable renderer has no plugin-editor dependency. One shared library prepares completed waveform/spectrum data and persists the last completed bank mapping locally. Device selection is independent of editor selection; Preview follows the actual transport and clears hardware-only effect/pad state. Disabling visualization stops animation and lease renewal. Opening it never initializes an audio input.

Firmware downloads and online analysis create network requests only after explicit user actions. The build generates all 13 model/build URLs from the root README through `cmake/FirmwareCatalog.cmake`; firmware versions are independent of the application version and there are no automatic release checks. The download worker follows redirects, enforces time/size limits, validates the UF2, and saves it to Downloads without replacing existing files. Failed or cancelled transfers remove their temporary file and preserve the selected firmware. Local onset detection works offline. Local UF2 copying requires file/product/family/block checks, a detected bootloader volume and an explicit flash action. No real upload, device reset or flash was performed during validation.

`CMakeLists.txt` verifies the JUCE archive checksum/version; `cmake/RubberBand.cmake` builds the vendored implementation statically. [Vendor/PROVENANCE.md](Vendor/PROVENANCE.md) records amenbreakvst and tape revisions. Notices accompany resources and packages. Protocol/parity fixtures and virtual-MIDI tests have moved into `Tests/`.

Presets cover macOS ARM/Intel, Windows x64 and Linux x86_64. `Release/` provides local packaging and dependency audits. Windows selects static MSVC; Linux bundles non-glibc dependencies with installed notices. The root helper and manual Windows workflow do not publish releases.

The old `visualizer-juce/` tracked source and standalone/AU/VST3 entrypoints were retired after integrated Mac acceptance. Website, browser/kiosk visualizer and firmware telemetry targets remain. No commit, push, branch, worktree, publication, deployment or hardware flashing was performed.
