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
    const retrigger = !old || old.estimated !== state.estimated || old.trigger !== state.trigger || old.bank !== state.bank ||
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
    const position = this.position + elapsed * speed * (s.forward ? 1 : -1);
    const clamp = (start: number, stop: number) => Math.max(start, Math.min(stop, position));
    const wrap = (start: number, stop: number) => {
      if (position >= start && position <= stop) return position;
      const length = stop - start;
      return length > 0 ? start + ((position - start) % length + length) % length : start;
    };
    // Slice boundaries schedule triggers, but only slice-stop/loop playback
    // constrains the audio to that region. Normal audio can run beyond it.
    switch (w.playMode) {
      case 1: return clamp(region.start, region.stop); // PLAY_SPLICE_STOP
      case 2: return wrap(region.start, region.stop); // PLAY_SPLICE_LOOP
      case 3: return clamp(0, w.duration); // PLAY_SAMPLE_STOP
      case 4: return s.forward ? wrap(region.start, w.duration) : wrap(0, region.stop);
      default: return wrap(0, w.duration); // PLAY_NORMAL
    }
  }
}
