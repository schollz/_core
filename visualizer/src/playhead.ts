import type { Playback, Waveform } from './types';

// This estimates source time, not device audio position. Retriggers, including
// the same slice, replace the anchor; heartbeats never restart the cursor.
export class Playhead {
  private state?: Playback;
  private wave?: Waveform;
  private at = 0;
  private position = 0;

  update(state: Playback, wave: Waveform, at: number) {
    const region = wave.slices[state.slice];
    if (!region || wave.bank !== state.bank || wave.sample !== state.sample) return;
    const old = this.state;
    const retrigger = !old || old.trigger !== state.trigger || old.bank !== state.bank ||
      old.sample !== state.sample || old.slice !== state.slice || old.forward !== state.forward || this.wave !== wave;
    this.position = retrigger ? (state.forward ? region.start : region.stop) : this.value(at) ?? region.start;
    this.state = state; this.wave = wave; this.at = at;
  }

  value(now: number): number | null {
    const s = this.state, w = this.wave;
    if (!s || !w || !s.valid) return null;
    const region = w.slices[s.slice];
    if (!region) return null;
    const speed = w.tempoMatch && w.bpm > 0 ? s.bpm / w.bpm : 1;
    const elapsed = s.stopped ? 0 : Math.max(0, now - this.at) / 1000;
    return Math.max(region.start, Math.min(region.stop, this.position + elapsed * speed * (s.forward ? 1 : -1)));
  }
}
