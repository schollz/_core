import type { Spectrum } from '../src/types';

// Hann-windowed FFTs, precomputed from unpadded PCM. Stereo channels contribute
// power independently so out-of-phase audio does not disappear from the display.
export function makeSpectrum(data: Buffer, sampleRate: number, channels: number): Spectrum {
  const size = 4096, bands = 32, frameRate = 20, minHz = 50, maxHz = 16000;
  const stride = channels * 2, samples = data.length / stride;
  const frames = Math.max(1, Math.ceil(samples / sampleRate * frameRate));
  const levels = Buffer.alloc(frames * bands);
  const real = new Float64Array(size), imag = new Float64Array(size);
  const power = new Float64Array(size / 2 + 1);
  const window = Float64Array.from({ length: size }, (_, i) => 0.5 - 0.5 * Math.cos(2 * Math.PI * i / (size - 1)));
  const reverse = new Uint32Array(size);
  const cos = new Float64Array(size / 2), sin = new Float64Array(size / 2);
  for (let i = 0; i < size; i++) {
    let value = i, reversed = 0;
    for (let bit = 1; bit < size; bit *= 2) { reversed = reversed * 2 + (value & 1); value >>>= 1; }
    reverse[i] = reversed;
    if (i < size / 2) { cos[i] = Math.cos(-2 * Math.PI * i / size); sin[i] = Math.sin(-2 * Math.PI * i / size); }
  }
  const ranges = Array.from({ length: bands }, (_, band) => {
    const lo = minHz * (maxHz / minHz) ** (band / bands);
    const hi = minHz * (maxHz / minHz) ** ((band + 1) / bands);
    const first = Math.max(1, Math.ceil(lo * size / sampleRate));
    return [Math.min(size / 2, first), Math.min(size / 2, Math.max(first, Math.ceil(hi * size / sampleRate) - 1))];
  });
  const scale = (2 / window.reduce((sum, v) => sum + v, 0)) ** 2 / channels;
  for (let frame = 0; frame < frames; frame++) {
    power.fill(0);
    const start = Math.round(frame * sampleRate / frameRate) - size / 2;
    for (let ch = 0; ch < channels; ch++) {
      imag.fill(0);
      for (let i = 0; i < size; i++) {
        const sample = start + i;
        real[reverse[i]] = sample >= 0 && sample < samples ? data.readInt16LE(sample * stride + ch * 2) / 32768 * window[i] : 0;
      }
      for (let length = 2; length <= size; length *= 2) {
        const half = length / 2, step = size / length;
        for (let offset = 0; offset < size; offset += length) {
          for (let j = 0; j < half; j++) {
            const a = offset + j, b = a + half, k = j * step;
            const r = real[b] * cos[k] - imag[b] * sin[k];
            const im = real[b] * sin[k] + imag[b] * cos[k];
            real[b] = real[a] - r; imag[b] = imag[a] - im;
            real[a] += r; imag[a] += im;
          }
        }
      }
      for (let i = 1; i < power.length; i++) power[i] += (real[i] ** 2 + imag[i] ** 2) * scale;
    }
    for (let band = 0; band < bands; band++) {
      let peak = 0;
      for (let bin = ranges[band][0]; bin <= ranges[band][1]; bin++) peak = Math.max(peak, power[bin]);
      const db = 10 * Math.log10(Math.max(1e-12, peak));
      levels[frame * bands + band] = Math.round(255 * Math.max(0, Math.min(1, (db + 72) / 72)));
    }
  }
  return { bands, frameRate, minHz, maxHz, frames, levels: levels.toString('base64') };
}
