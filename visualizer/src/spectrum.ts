import type { Spectrum } from './types';

export function decodeSpectrum(spectrum?: Spectrum): Uint8Array {
  if (!spectrum) return new Uint8Array();
  return Uint8Array.from(atob(spectrum.levels), c => c.charCodeAt(0));
}

// Interpolate at source time, shared with the waveform's estimated playhead.
export function spectrumAt(spectrum: Spectrum | undefined, levels: Uint8Array, position: number | null): number[] {
  if (!spectrum || position === null || !Number.isFinite(position) || levels.length !== spectrum.frames * spectrum.bands)
    return Array(32).fill(0);
  const index = Math.max(0, Math.min(spectrum.frames - 1, position * spectrum.frameRate));
  const first = Math.floor(index), next = Math.min(first + 1, spectrum.frames - 1), mix = index - first;
  return Array.from({ length: spectrum.bands }, (_, band) =>
    (levels[first * spectrum.bands + band] * (1 - mix) + levels[next * spectrum.bands + band] * mix) / 255);
}
