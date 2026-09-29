# Core Sample Manager 0.1.0 validation

## Splice interval parity: coverage prepared, not executed

Read-only comparison of the supplied Icarus exports found JUCE retaining the default 96 ticks for both loops. The website stored 384 ticks for `beat_bpm174.wav` (32 beats / 16 slices) and 192 for `amen_bpm170_freak.wav` (16 beats / 16 slices). The native importer and Even slices action had never recalculated `spliceTrigger`.

Imports now calculate the interval from the rounded source beat count and slice count, following `core/src/zeptocore/zeptocore.go` (192 ticks per beat, quarter-tick rounding followed by integer conversion; one-shot imports use one beat). Even slices follows `createRegionsEvenly` in `core/src/server/static/app.js`, using unrounded beats and 24-tick divisions. Results are bounded to the firmware's valid 2–32767 ticks. Render BPM and the padded/eight-times output durations do not change the loop's beat count. Saved/adopted intervals are preserved until an explicit Even slices action, which updates markers, timing mode and interval in one undoable operation. Device-setting defaults, drum markers, project schema and audio processing are unchanged.

Added regression coverage includes synthetic imports with the supplied loops' exact source frame counts/rates and tempo labels; primary/companion metadata; differing import/even rounding; short/dense and long-loop bounds; one-shot import; render-tempo independence; stored interval preservation; and manager saves with unchanged WAV hashes, undo and redo. Source review and whitespace checks passed. No builds, tests or native UI sessions were executed by the agent.

Follow-up read-only inspection of the user's regenerated JUCE export confirmed 384 ticks for Beat and 192 for Amen in both primary and stretched `.info` files, matching the web export. Both retained 16 slices with variable timing off, and the native project reported matching completed revisions. The regenerated JUCE bank has the opposite sample order, so comparisons were matched by sample identity. Reopen and undo/redo acceptance remain unexecuted.

## Filename sidecars: coverage prepared, not executed

The filename persistence changes add coverage to the existing native contract suite and `core/src/pack` tests. No builds, test execution, firmware edits, flashing, or hardware checks were performed for this change. The execution evidence below predates these changes.

- Website full export, ZIP extraction, duplicate filenames and settings-only exclusion use completed audio fixtures without invoking SoX. The extracted sidecars must match `Tests/fixtures/website-name.json`, also consumed by native card adoption with the same independently generated WAV bytes.
- Native checks cover Unicode, imported basenames including XRNI, schema-1 projects without the optional field, display/original/generic fallback order, bank-only copies, duplication, reordering, moves, deletion, undo/redo, rerendered hashes, and transactional backfill without rewriting WAV or `.info` files.
- Recovery checks cover unchanged primary audio retaining native names, replacement audio with matching or stale metadata, corrupt JSON, field types, unsupported schemas, the 64 KiB read limit, path rejection, wrong-slot metadata, protected entries, and preservation of unusable sidecars during saves and deletion.
- Transaction fixtures interrupt after audio, sidecar and manifest mutations, plus an abrupt child-process exit after audio and sidecar replacement, and check rollback of both files.

After build/test execution is authorized, run the existing native self-test and `go test ./src/pack` from `core/`. For an additional end-to-end check, export a full website pack, extract it, open the folder in the native app, then rename a sample and copy only its bank folder into a fresh folder. Check both recovered names and verify WAV/`.info` hashes remain unchanged after the rename. Existing-firmware recognition with sidecars present remains a hardware acceptance check; no firmware build or version bump is required by the format.

## Folder-opening and bank-selection follow-up

Folder-opening follow-up: the existing Linux logs for a 104-sample folder show 20,039–20,115 ms in storage open, 406–457 ms in cached waveform preparation, and roughly 21 seconds overall. Source inspection traced full-file SHA-256 to JUCE's 64-byte reads on an unbuffered file stream. Hashing now uses 256 KiB buffering and up to four workers for opening/adoption, with bank enumeration replacing probes for every possible missing variant. Waveform file reads are buffered too. Added checks compare buffered SHA-256 against memory hashes at block/buffer boundaries and cover parallel progress/error propagation. These changes have not been built, executed, or benchmarked; no new timing claim is made. Bank selection now explicitly repaints the fixed 16-row sample list and clears the visualizer selection; click/keyboard bank-switch acceptance remains to be run.

## Auto slice, save errors and bank memory: coverage prepared, not executed

Read-only inspection of the user's subsequent Linux log shows the 104-sample folder opening in 3,960 ms. Auto slice detection took 71.1–71.9 ms, while saves took 10,550–12,156 ms. One click at 11:59:16.773 waited until 11:59:26.069 for the preceding save to finish. Saving performed serial, repeated full-file hashes without cancellation during the initial verification pass. The new save path verifies files with up to four workers, reuses hashes within that save, interrupts hash reads between buffered chunks for queued edits, and reuses unchanged sidecars. Replacement destinations and staged content remain verified by the transaction. These timing observations predate the save changes; no new benchmark has been run.

The reported transient range errors came from counted zero positions in existing card metadata. The firmware's loop-start logic accepts zero, while the app incorrectly rejected all positions below 16 frames. Validation now accepts that first block without removing any markers, retains upper-bound checks, clears obsolete persisted range warnings on load, and identifies the bank/sample in future encoding failures. No firmware or user-card files were modified during this investigation.

Prepared regression cases cover zero/first-block/maximum transient encoding, true overflow rejection, resuming a pending slice save with zero markers and old warnings, preserving WAV bytes, cancelling an in-progress buffered hash, rejecting staged content changed after save planning, and retaining the modification time of an unchanged sidecar. Bank selection now restores the last selected sample by ID for each bank in the open project. Manual acceptance should switch away and back with mouse and keyboard, including empty banks, renamed/reordered/deleted samples, and opening a different project. Builds, tests and UI execution remain unperformed for these changes per the task constraint.

## Compact bank and sample strips: source review only

The two navigation strips now share a 72-pixel width, 39-pixel row height and vertical alignment. The selected bank and sample strip use the same background in each presentation. Sample tiles contain only a slot number and an available filename, with the full name on hover; generated recovery labels, empty-slot text and metadata subtitles are omitted. Selection memory and multiple selection still use the existing list models. The optional docked visualizer occupies a resizable column in the workspace instead of reducing the sample strip. Source and whitespace checks were performed; builds, test execution and visual acceptance were not run. Manual acceptance remains for all three presentations, long/Unicode filenames, empty/recovered slots, bank switching, multi-selection and the dock at minimum window size.

## Import waveform latency and defaults: coverage prepared, not executed

The user's Linux log at 13:26 shows `beat_bpm174.wav` importing in 62.3 ms, but no completed waveform until 13:26:25.520, about 12.4 seconds later. Pending-state persistence took 1,376.6 ms, full-file checks 2,684.0 ms, rendering 4,620.1 ms (about 3.8 seconds after beginning the eight-times companion), transaction commit 2,741.2 ms, and waveform preparation/cache persistence 476.1 ms. The visualizer dock was off. The editor's dependence on the completed card waveform made all of this work block the initial display.

The editor now receives bounded source peaks on the existing background worker before pending-state writes and the hardware save. It uses sequential decoded blocks, with no resampling, stretch, padding or spectrum analysis. Completed output takes precedence once available. New `[WAVEFORM]` timing/readiness/display logs make the first visible waveform measurable on the next run. This changes display readiness, not the audio rendering or transaction guarantees. A new accepted-import event selects the first sample in each batch once, including a previously selected empty slot; later save notifications do not steal selection. Unmarked imports start with 16 even slices, capped only for very short audio; embedded markers and source render anchors remain intact. Manual slice-boundary edits enable variable timing, and unrelated editor controls no longer overwrite that flag.

Prepared checks cover stereo polarity, decoded block boundaries, final frames, very short and silent audio, cancellation, all import formats including XRNI, source-waveform reuse/identity, pending import recovery despite a blocked save, completed-waveform handoff, removal/undo, first-in-batch selection, failed-batch selection suppression, default slices, and variable timing persistence through edits and undo. Source/whitespace review only: no builds, tests, UI run or new latency benchmark was performed. Manual acceptance remains for first-waveform latency, multi-file automatic selection, clicking elsewhere during the later save, and the visible variable-timing checkbox after a marker drag.

## Subsequent import log and follow-ups: source review, coverage not executed

The user's 13:44 run provides subsequent evidence for early waveform display: the first import took 692.5 ms, source peaks took 22.5 ms, and the UI displayed those peaks at 13:44:28.230 before a 16,664.7 ms hardware save. The following three-file batch took 619.6, 616.0 and 632.9 ms per import; its first UI status did not arrive until 1.914 seconds after the batch began. Import now publishes filename/count before each file, waveform preparation and rendering report their counts, and an animated footer bar indicates active work. These progress changes have not been run by the agent.

New-folder initialization now commits the complete default settings and first manifest together. Bank 1 enables only Time Stretch, banks 2–7 use the website effect presets, and general settings retain the existing native defaults. Prepared checks cover marker presence/exclusivity, reopening and preservation of existing/custom settings. Preserve Pitch defaults off for newly created samples; saved project values remain authoritative.

Source inspection found no prior import tempo estimation: every sample kept the model's 120 BPM default. Filename labels and embedded tags now take precedence, with a loop-duration fast path and a statically vendored SoundTouch 2.4.1 detector for ambiguous lengths. Checks are prepared for the user's three 135 BPM loops (compact audio fixtures authorized by the user), unlabeled imports, tail-extended loops that force SoundTouch, metadata precedence, fractional/invalid/ambiguous names, cancellation and silence. These tests and the native app have not been built or executed for the changes. Fixture source/output hashes and upstream SoundTouch archive/per-file hashes are recorded; no new accuracy or speed benchmark is claimed.

Manual UI acceptance remains for all three presentations: header runes on Ezeptocore/Zeptocore, duration/rate-only subtitle, hidden Splice ticks, activity-bar contrast and file-count updates during multi-file imports. Ectocore retains its logo.

## Waveform controls and project creation: source review only

The editor now orders interaction tips, editing tools, sample settings, playback options and sample actions above a collapsed Advanced disclosure. Advanced holds tempo processing, slice timing, auto-slice tuning and online analysis. Its toggle belongs to the view and is absent from project data and preferences, so it survives sample/bank selections during the session and starts collapsed on a new launch. Showing or hiding the group does not invoke sample edits or cancel analysis. The timer keeps the hidden-analysis indicator and the online button's cancellation label current.

Source review covered all three presentation branches, the 1120 × 840 minimum window and the dock's 510-pixel editor minimum. Layout measures the tips, wraps whole control groups and reserves vertical scrollbar width only when needed. Collapsed controls return height to the waveform; expanded controls scroll while retaining the existing 145-pixel waveform minimum. The dock divider calls the same layout path. Native visual acceptance has not been run.

Zeptocore hides One-shot and excludes it from settings writes, including queued edits. It also hides the transient lanes, their 56-pixel reserve and the Kick/Snare/Other selector options. Presentation changes do not alter saved one-shot values or transient arrays. Other presentations draw the separator above Kick using the waveform outline colour. Marker edits still use `Manager::editMarkers`, which enables variable timing for changed slices; the hidden timing checkbox still refreshes from sample state and ordinary settings edits do not overwrite it. Online cancellation still uses the existing busy-first button callback after reopening Advanced.

Create project collects a new folder name and location with a save chooser. Cancellation returns before any creation or manager call. Existing files, directories and symbolic links are rejected; a single non-recursive `create_directory` must report a newly created directory before the existing `Manager::open` initialization is invoked. Creation errors use the existing UI error display. Project schemas and audio processing are unchanged.

Changed-source review and `git diff --check` passed. No builds, tests, native UI sessions or online requests were executed for this change, per the task constraint. Remaining host acceptance: switch among all three presentations with both one-shot values and existing transient markers; resize and drag the visualizer divider with Advanced open/closed; change samples/banks and relaunch to check disclosure lifetime; edit slice markers while collapsed, then inspect variable timing and undo; run analysis, collapse/reopen and cancel; create a project, cancel the chooser, and try existing file/directory destinations.

## Tooltip coverage, control colours and initial bank selection: source review only

Added explanatory tooltips to toolbar actions, editor fields and labels, waveform gestures, bank/sample rows, the dock divider, hardware settings/effects, Device tools and visualizer controls. The waveform help follows the selected marker lane; Advanced/online help follows analysis state; effect help names the selected effect bank and its enable/disable action. Each view owns a tooltip window restricted to that view and its look-and-feel, avoiding duplicate tooltips from the dock and manager. Tooltips inherit their view's theme; dropdowns, popup menus and text editors now share the background colour, with explicit text, highlight and focus colours. Theme switches also refresh an open Device window. The visualizer retains its dark palette.

Bank selection keeps a valid remembered sample; otherwise it chooses the lowest occupied slot and uses the normal sample selection callback to update the editor and selection memory. Empty banks remain unselected. An explicit bank-click callback covers the initially active bank, which does not emit a selection-change event when clicked again. Source review covers gapped slots, stale selection memory, empty banks, keyboard bank changes and the existing imported-sample selection path.

Verification is limited to changed-source review and whitespace checks. No builds, tests or native UI sessions were run under the existing constraint. Remaining manual acceptance: hover controls in each presentation and auxiliary window; switch themes with Device open; check popup backgrounds, selected text and keyboard focus; check docked/detached tooltips for clipping or duplicates; and visit populated, empty and previously selected banks.

## Prior validation

Validated on 2026-09-29 on Apple Silicon, macOS 26.5.2 (25F84), Apple Clang 17.0.0. Both macOS binaries target macOS 11.0. That target is not evidence of runtime acceptance on an older OS or physical Intel Mac.

Integrated Mac acceptance passed before the old standalone/AU/VST3 entrypoints were removed. No commits, pushes, releases, deployments, firmware flashes or physical-device resets were performed. Windows/Linux runtime qualification and deliberate hardware acceptance remain separate steps.

## Builds and execution

| Check | Result |
| --- | --- |
| Native ARM64 Release build | Passed with pinned JUCE 9.0.3 and static vendored Rubber Band |
| Native CTest | 2/2 passed: `manager_contract` and `native_midi`, 13.89 seconds in the final checkout run |
| Independent source copy | Built under `/tmp/CoreSampleManagerSource-20260929` using only copied application sources and the pinned archive; CTest 2/2 passed in 12.34 seconds |
| Intel Release build | Cross-compiled on Apple Silicon; x86_64 architecture/linkage verified; physical Intel execution not qualified |
| Final package outside checkout | Extracted under `/tmp/CoreSampleManagerDelivery-20260929`; embedded `--self-test` passed with all network access denied |
| Dependencies | Both executables link only Apple system libraries/frameworks; no sibling checkout or external audio-processing runtime |
| Bundle audit | Version 0.1.0, exact requested architecture, Mach-O minimum macOS 11.0 agrees with packaged Launch Services metadata |
| Integrity | Strict ad-hoc codesign verification, every payload hash and both archive SHA-256 values passed |

The final offline self-test reported passing audio, import, manager, recovery, protocol, playback, analysis and library groups, plus 57 base contract checks. These are grouped tests with internal assertions, not 57 registered CTest cases.

## Automated evidence

**Format compatibility.** Independent Python fixtures encode the website/firmware layout without calling the application's writer. They cover mono/stereo, 44.1/88.2 kHz, flags, padding/header layout, slices/types, transients, settings and numeric boundaries. Further checks cover companions, the one-shot/no-tempo-match exception, full banks, protected entries and unknown files.

**Storage.** Tests cover adoption, immutable sources, reopen, duplication, ordering/removal/undo, exclusive settings, stable IDs, writer locking, external conflicts, reconciliation/undo, metadata-only saves without changing WAV modification time, stale work and late online results. Closing/reopening with pending work retains edits. An actual child process exits during a partially applied transaction; reopening rolls back to the completed state.

**Failure recovery.** A read-only destination reports failure. Renaming the project away simulates unavailable storage and checks cached visualization, retained edits, prevention of accidental root recreation and retry after restoration. This does not qualify physical SD-card removal. A separate disposable 16 MiB mounted image was filled to actual `ENOSPC`: completed WAV/metadata hashes and revision stayed unchanged, local recovery retained pending state, and freeing space then Retry reached Ready without rerendering unchanged audio. The image was unmounted and removed.

**Imports and audio.** Embedded fixtures cover WAV, AIFF/AIF, FLAC, MP3, Ogg, XRNI, WAV cues/loops, OP-1 and custom AIFF/Ogg metadata. Corrupt files are rejected; source hashes and tampered-cache repair are checked. Audio tests verify:

- Resampling duration and anti-aliasing: a 30 kHz tone downsampled from 96 kHz to 44.1 kHz has residual RMS below 0.001.
- A two-second 440 Hz stereo tone stretched to three seconds has 132,300 frames within two frames, pitch within 2 Hz and aligned channels. Speed conversion produces approximately 660 Hz.
- Canonical 44-byte PCM headers, −6 dB peaks, circular padding, a 13-frame input and exact silence.
- Stretched attacks within 10 ms of mapped anchors; merging current render settings with retained ordered boundaries/types/transients and original sources.
- Cancellation, companion generation and range validation without numeric wrapping.

**Visualization and device tools.** Migrated protocol, estimated-playback, analysis, library and browser parity fixtures pass. New checks cover Preview transport, absence of invented effects/pads, committed-revision refresh, missing-folder cache use, source switching and disable/re-enable. Actual macOS virtual-MIDI endpoints test the shared service, SysEx/pads, leases, connection loss/reconnection and teardown. Synthetic UF2 tests check family/product/block handling without flashing. Online parsing and stale-revision rejection run locally; no real service request was sent.

**Preferences/UI construction.** Migration imports usable old MIDI/motion/reference-folder preferences, preserves the old file and respects existing new preferences. All three settings views construct. No-op edits preserve revisions and undo history.

## Native UI acceptance

Packaged apps were launched outside the checkout. Native UI checks covered:

- Empty-shell folder chooser, opening a portable folder, file-picker import, reopening and clean Command-Q shutdown.
- Ezeptocore gray, Zeptocore purple, Ectocore blue/logo/runes, product-specific settings, brightness and the seven-by-sixteen effect matrix.
- Whole preview and slice audition, BPM editing/conversion, even/automatic slicing, added/dragged boundaries, zoom and saving to Ready.
- Docked, detached and fullscreen visualization, Device/Preview switching, preview playhead/spectrum, disable/re-enable and no microphone prompt.
- Minimum 1120 × 840 layout, settings and detached/fullscreen controls.
- Shared Device window and disabled flashing until a valid local file/bootloader selection. No device command or flash was executed.

Real macOS file-URL drag/drop used the optional `--drag-drop-acceptance` fixture. Its chip starts an OS external drag of a synthetic Ogg; dropping it enters JUCE's native platform drop handler and the normal import queue. It never calls `filesDropped()` directly. Finder-origin dragging was not separately qualified. The fixture is absent in normal launch.

A native UI session ran with all networking denied: drag import, local slicing, Render BPM conversion, saving to Ready, preview and Preview visualization worked, followed by clean shutdown. The final packaged self-test was repeated under the same network restriction.

## Reproduce

From the repository root on Apple Silicon:

```sh
cmake --preset macos-arm64 -S sample-manager
cmake --build sample-manager/build/macos-arm64 --parallel 6
ctest --test-dir sample-manager/build/macos-arm64 --output-on-failure
python3 sample-manager/Release/package.py --platform macos-arm64 --skip-build
```

After extracting the package, substitute its executable path:

```sh
APP='/path/to/Core Sample Manager.app/Contents/MacOS/Core Sample Manager'
"$APP" --self-test
"$APP" --midi-test
/usr/bin/sandbox-exec -p '(version 1)(allow default)(deny network*)' "$APP" --self-test
python3 sample-manager/Tests/macos-disk-full.py "$APP"
"$APP" --drag-drop-acceptance
```

The disk-full runner creates a bounded disposable image; its test command refuses ordinary folders. Self-tests use temporary isolated state and embedded fixtures. MIDI tests use temporary virtual endpoints. Fixture generators are test utilities, not runtime dependencies.

Ignored local logs in `.cache/`: `ctest-final.log`, `independent-build.log`, `offline-self-test-final.log`, `disk-full.log`, and the final ARM/Intel build and package logs.

## Local packages

These are ad-hoc-signed local builds, not notarized public releases. Each `dist/` directory includes `manifest.json`, `SHA256SUMS.txt` and `complete.json`.

| Platform | Archive under `dist/` | Bytes | SHA-256 |
| --- | --- | ---: | --- |
| Apple Silicon | `macos-arm64-0.1.0-20260929T164634Z/Core-Sample-Manager-0.1.0-macos-arm64.zip` | 6,113,995 | `84897b6f2ae61fc69da4078110811049fc2af9cc19c2800546b0a979ed710d0a` |
| Intel | `macos-x86_64-0.1.0-20260929T164634Z/Core-Sample-Manager-0.1.0-macos-x86_64.zip` | 6,760,354 | `7aab20bed0ac5bb1734866577ac4689e1e587b40613bab0b91ce13528dc1b789` |

## Deferred qualification

- Physical Intel execution and runtime acceptance on macOS 11.
- Native Windows/Linux builds and runtime, including static-CRT and bundled-library acceptance. Presets, packaging and a manual Windows artifact workflow are prepared; Mac compilation is not cross-platform verification.
- Actual SD-card playback/removal timing, physical MIDI hardware and deliberate local UF2 copying.
- An authorized live drum-analysis request, service availability and cancellation under real network conditions.
- Developer ID/notarization, public distribution and large-library performance qualification.
