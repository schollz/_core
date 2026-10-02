# Tempo fixtures

These three fixtures derive from the user's Clean, Comp and Vintage versions of “Medium DnB 3 - 135 bpm”, supplied from their local Star Power Drums collection. The user explicitly authorized vendoring them for tempo regression checks on 2026-09-29. This records that authorization, not a claim that the recordings are public domain or covered by the application license.

Each fixture contains the entire original loop, converted from 48 kHz stereo to 24 kHz mono PCM16 with `scipy.signal.resample_poly(samples.mean(axis=1), 1, 2)`. Duration and rhythmic content are retained; filename/embedded BPM information is omitted. `sources.json` records the original basenames, source and fixture SHA-256 hashes, and expected tempo. Original audio files are untouched.

The native tempo tests use these files without tempo hints, exercise filename/embedded metadata precedence, and append a short silent tail to require audio analysis. These syncopated loops expose a SoundTouch detection failure, so the tests verify that onset correlation recovers 135 BPM within 2 BPM. They also cover quiet opposite-phase stereo at 44.1 and 48 kHz, cancellation during analysis, and source-file preservation. Run them with `--self-test tempo` or the `tempo_detection` CTest target.
