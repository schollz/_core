import { expect, test } from 'vitest';
import { makeSpectrum } from '../build/spectrum';
import { decodeSpectrum, spectrumAt } from '../src/spectrum';

function tone(hz: number, rate = 44100, stereo = false) {
  const channels = stereo ? 2 : 1, pcm = Buffer.alloc(rate * channels * 2);
  for (let i = 0; i < rate; i++) {
    const value = Math.round(16000 * Math.sin(2 * Math.PI * hz * i / rate));
    pcm.writeInt16LE(value, i * channels * 2);
    if (stereo) pcm.writeInt16LE(-value, i * channels * 2 + 2);
  }
  return makeSpectrum(pcm, rate, channels);
}

test.each([44100, 88200])('locates a 1 kHz tone in logarithmic bands at %i Hz', rate => {
  const spectrum = tone(1000, rate);
  const values = spectrumAt(spectrum, decodeSpectrum(spectrum), 0.5);
  const peak = values.indexOf(Math.max(...values));
  const lower = spectrum.minHz * (spectrum.maxHz / spectrum.minHz) ** (peak / spectrum.bands);
  const upper = spectrum.minHz * (spectrum.maxHz / spectrum.minHz) ** ((peak + 1) / spectrum.bands);
  expect(lower).toBeLessThan(1000); expect(upper).toBeGreaterThan(1000);
  expect(values[peak]).toBeGreaterThan(0.8);
});

test('opposite-polarity stereo preserves spectral energy and silence stays silent', () => {
  const mono = tone(1000), stereo = tone(1000, 44100, true);
  expect(decodeSpectrum(stereo)).toEqual(decodeSpectrum(mono));
  const silence = makeSpectrum(Buffer.alloc(44100 * 2), 44100, 1);
  expect(decodeSpectrum(silence).every(v => v === 0)).toBe(true);
});

test('source position selects and interpolates frames, including backwards movement and inactive playback', () => {
  const spectrum = { bands: 32, frames: 2, frameRate: 20, minHz: 50, maxHz: 16000,
    levels: Buffer.from([...Array(32).fill(0), ...Array(32).fill(255)]).toString('base64') };
  const data = decodeSpectrum(spectrum);
  expect(spectrumAt(spectrum, data, 0.05)).toEqual(Array(32).fill(1));
  expect(spectrumAt(spectrum, data, 0.025)).toEqual(Array(32).fill(0.5));
  expect(spectrumAt(spectrum, data, -1)).toEqual(Array(32).fill(0));
  expect(spectrumAt(spectrum, data, 100)).toEqual(Array(32).fill(1));
  expect(spectrumAt(spectrum, data, null)).toEqual(Array(32).fill(0));
  expect(spectrumAt(undefined, data, 0)).toEqual(Array(32).fill(0));
});
