# Vendored sources

- amenbreakvst: `f3c296089e9fdef2ecb2386f1ce0225a55fa10fc`, local reference `/Users/zns/Documents/amenbreakvst`, https://github.com/schollz/amenbreakvst. `amenbreak/Onsets`, `amenbreak/Core`, and `amenbreak/WaveformDisplay.*` are retained reference sources. The onset implementation is compiled directly; exporter and waveform primitives are adapted into the application's block-based processing and editor. The reference checkout is not a build dependency. That checkout contains no root license file; source notices are preserved verbatim.
- Rubber Band: copied from the same amenbreakvst revision, including `COPYING` and dependency notices. Static build uses builtin FFT and BQ resampler. See `rubberband/CHANGELOG` for upstream version.
- JUCE: 9.0.3, tag archive SHA-256 `a81e5508b8a0efa483917794ebeaff56aed3075730c405a947734413f40c1aba`. Fetched and verified by CMake; see the fetched `LICENSE.md`.
- Packaging and dependency conventions adapted from tape `94ddf5fec79feef3ba166bf891a750f9f924fb1c` (https://github.com/schollz/tape). No reference checkout required to configure, build, package, or run.
- Fonts initially copied from this repository's native visualizer; individual license notices accompany them. Website fonts/artwork are tracked with the app resources as migrated.

Application code is licensed under this repository's GPLv3 license. Upstream source files retain their original notices.
