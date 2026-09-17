# Zeptocore knob controls

These are the mappings in the current firmware source. Knobs **1, 2, 3** are
labelled **X, Y, Z**, respectively. Hold the modifier button while turning a knob.

| Combination | Function | Behavior |
| --- | --- | --- |
| **A + 1 (X)** | Output volume | Turn up to increase volume. |
| **A + 2 (Y)** | Low-pass filter cutoff | Turn up to open the filter. |
| **A + 3 (Z)** | Realtime time stretching | About 1×–10×, with finer adjustment at the low end; bypassed below roughly 1.1×. |
| **B + 1 (X)** | Random slice sequence | Low end disables it; middle range selects sequence length; upper end regenerates an 8-step sequence and enables phrase-end retriggering. |
| **B + 2 (Y)** | Pitch / playback rate | Lower to higher, with a neutral zone around the center. |
| **B + 3 (Z)** | Tempo | Approximately half to 1½ times the source BPM, limited to 30–300 BPM. |

The A/B remap moves volume from A+3 to A+1, filter from B+2 to A+2,
realtime stretch from B+3 to A+3, playback rate from A+2 to B+2, and tempo
from A+1 to B+3. B+1 is unchanged. The remap applies with or without the
optional visualizer firmware flag.

C/D combinations, unmodified knob behavior, and Mash-mode held-effect parameter
editing are unchanged. The standard Zeptocore build uses `DJ_FILTER=0`, so A+2
is low-pass only; custom `DJ_FILTER=1` builds retain the existing DJ filter sweep
on that same new combination.

## MIDI and feedback

The outgoing MIDI CC moves with the function. CC numbers, incoming CC behavior,
parameter ranges, and LED feedback patterns are unchanged:

| Combination | MIDI CC | Function |
| --- | --- | --- |
| A+1 | 7 | Volume |
| A+2 | 19 | Filter |
| A+3 | 20 | Realtime stretch |
| B+1 | 18 | Random sequence |
| B+2 | 16 | Pitch / playback rate |
| B+3 | 15 | Tempo |

Implementation: [`lib/zeptocore.h`](../lib/zeptocore.h).
Build with `make zeptocore`, or build without flashing with
`make zeptocore ZEPTOCORE_VISUALIZER=ON` for visualizer telemetry.
Older installed firmware retains its old mappings until updated.
