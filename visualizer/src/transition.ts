import type { Playback } from './types';
import { isFresh } from './protocol';

// Keep speculative display state separate from the actual device telemetry.
export class SampleTransition {
  private previous?: Playback;
  private pending?: { state: Playback; at: number };

  update(state: Playback, at: number) {
    const changed = this.previous && (this.previous.bank !== state.bank || this.previous.sample !== state.sample);
    this.previous = state;
    if (state.valid) {
      this.pending = undefined;
      return { state, at };
    }
    if (changed) {
      this.pending = { state: { ...state, slice: 0, valid: true, estimated: true, effects: undefined }, at };
    }
    if (this.pending && isFresh(this.pending.at, at)) {
      // Heartbeats must not restart the estimate or extend its deadline.
      this.pending.state = { ...this.pending.state, bpm: state.bpm, stopped: state.stopped, muted: state.muted };
      return this.pending;
    }
    return { state, at };
  }
}
