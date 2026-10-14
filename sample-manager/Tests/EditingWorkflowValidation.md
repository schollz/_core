# Editing workflow validation

Validated on 2026-10-14 on macOS, Apple Silicon, using the Release build and internal storage.

## Automated and visual checks

All 13 CTest targets passed. The separate real-ENOSPC acceptance harness also passed on a disposable 16 MB HFS+ image: completed audio and metadata remained intact, local pending recovery completed independently, and Retry saved the retained edit after space was released. The image was detached and removed afterward. The editing workflow suite includes a 30-second blocked save: slice/transient edits and undo/redo must be accepted within two seconds while the disk job remains blocked, the local recovery checkpoint must advance independently, and the eventual older save must leave newer edits pending.

Other checks cover the production one-second autosave debounce; manual Save and local recovery; Discard and reopening without resurrecting edits; preservation of saved undo history; repeated Save while an older save is running; rechecking newly accepted edits before an asynchronous project transition; Save/Discard/Cancel continuations and focused text fields; failure/retry; affected-slot external-change protection; missing WAV/metadata, unsupported metadata, corruption discovered while a project is already open, and malformed/oversized filename sidecars; exact-byte Undo/Redo across save and reopening; slot reuse; backup failure; raw-sidecar transaction rollback; committed byte identities in the waveform metadata cache; all transient lanes, zoom/pan, tie-breaking, bypass, duplicate handling and draft acknowledgements.

Rendered previews cover widths of 1120 and 1320 pixels in all three presentations, plus damaged-slot removal controls. They are under `sample-manager/build/editing-previews/`. Normal and damaged views were inspected for clipping and control overlap.

## Save benchmark

The before implementation was compiled from `9cd162d8af87` in a temporary source snapshot, using the same pinned JUCE, vendor dependencies, Release flags, and compiled JUCE modules as the updated app. No branch or worktree was created.

The fixture contains 48 slots in three banks, each holding 20 seconds of stereo audio at 44.1 kHz. Five consecutive Even slices changes affect only the first slot. The table reports medians from the diagnostic scopes. The updated benchmark uses a 20 ms test debounce to measure pipeline cost; the production default remains one second. Before uses its original 250 ms debounce.

| Measurement | Before | After |
| --- | ---: | ---: |
| Save work | 532.2 ms | 198.7 ms |
| Affected-file verification | 261.3 ms | 19.0 ms |
| Transaction | 16.6 ms | 16.4 ms |
| Visualizer/cache refresh | 233.7 ms | 124.3 ms |
| Recovery checkpoint | 28.8 ms | 31.9 ms |
| Edit to idle, including test debounce | 838.0 ms | 246.2 ms |

Median save work decreased by 62.7%. Each before save reanalysed one waveform and reused 47; each after save performed zero audio analyses and reused all 48. Transaction safety remains in place. Recovery after the change runs on its own writer and coalesces checkpoints, so its samples do not correspond one-for-one with edits and its cost does not block editing.

Raw logs are retained in the ignored `sample-manager/logs/save-benchmark-before.log` and `save-benchmark-after.log` files. Absolute temporary paths in those logs identify generated fixtures, not user projects.

## Repeat on internal storage or an SD card

No SD card was mounted during this validation, so representative SD-card timing remains unmeasured. The delayed-save regression establishes edit responsiveness independently of disk completion; it does not establish a card's write throughput.

Run the benchmark on internal temporary storage:

```sh
CORE_MANAGER_DEBUG=1 'sample-manager/build/macos-arm64/CoreSampleManager_artefacts/Release/_core sample manager.app/Contents/MacOS/_core sample manager' --self-test save-benchmark
```

To use a mounted card, add `CORE_SAVE_BENCHMARK_DIR=/absolute/path/to/the/card` to that command. The location must already exist. The location override and automatic fixture cleanup were also verified on a dedicated temporary directory. The benchmark creates a uniquely named temporary fixture under it, uses only that fixture, and removes it afterward. Allow roughly 250 MB of free space. Existing banks and project files are not used.
