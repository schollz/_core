import { describe, expect, test } from 'vitest';
import { makeWaveform, parseInfo, parseWav } from '../build/waveform';
import { buildLibrary } from '../build/library';
import { fileURLToPath } from 'node:url';
import { mkdtemp, mkdir, writeFile, rm } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';

function fixture(channels = 2, rate = 44100) {
  const frames = rate * 2, stride = channels * 2, wav = Buffer.alloc(44 + frames * stride);
  wav.write('RIFF'); wav.writeUInt32LE(wav.length - 8, 4); wav.write('WAVEfmt ', 8); wav.writeUInt32LE(16, 16);
  wav.writeUInt16LE(1, 20); wav.writeUInt16LE(channels, 22); wav.writeUInt32LE(rate, 24);
  wav.writeUInt32LE(rate * stride, 28); wav.writeUInt16LE(stride, 32); wav.writeUInt16LE(16, 34);
  wav.write('data', 36); wav.writeUInt32LE(frames * stride, 40);
  for (let f = 0; f < frames; f++) for (let ch = 0; ch < channels; ch++) {
    const value = f < rate / 2 || f >= rate * 1.5 ? 30000 : ch ? -1000 : 2000;
    wav.writeInt16LE(value, 44 + f * stride + ch * 2);
  }
  const info = Buffer.alloc(11 + 2 * 9), size = rate * stride;
  info.writeUInt32LE(size); info.writeUInt32LE(120 | (1 << 13) | ((rate / 44100 - 1) << 14) | ((channels - 1) << 15) | (1 << 16), 4); info[10] = 2;
  info.writeInt32LE(0, 11); info.writeInt32LE(size / 4, 15); info.writeInt32LE(size / 4, 19); info.writeInt32LE(size, 23);
  return { wav, info };
}
describe('reference audio', () => {
  test.each([[1, 44100], [2, 44100], [2, 88200]])('removes padding, retains polarity: %i channels at %i Hz', (channels, rate) => {
    const { wav, info } = fixture(channels, rate);
    const result = makeWaveform(wav, info, 0, 0, 20);
    expect(result.duration).toBe(1); expect(result.peaks[0]).toEqual(Array(40).fill(2000));
    if (channels === 2) expect(result.peaks[1]).toEqual(Array(40).fill(-1000));
    expect(result.slices).toEqual([{ start: 0, stop: 0.25 }, { start: 0.25, stop: 1 }]);
  });
  test('rejects corrupt metadata, inconsistent padding and truncated WAV chunks', () => {
    const { wav, info } = fixture();
    expect(() => parseInfo(info.subarray(0, 12))).toThrow();
    expect(() => parseWav(wav.subarray(0, wav.length - 1))).toThrow();
    info.writeInt32LE(9999999, 19); expect(() => parseInfo(info)).toThrow('bounds');
    const other = fixture(); other.info.writeUInt32LE(other.info.readUInt32LE(0) + 4);
    expect(() => makeWaveform(other.wav, other.info, 0, 0)).toThrow('padding');
  });
  test('discovers additions/removals and reports broken samples, without changing originals', async () => {
    const root = await mkdtemp(join(tmpdir(), 'zepto-library-'));
    try {
      await mkdir(join(root, 'bank2'));
      const { wav, info } = fixture();
      await writeFile(join(root, 'bank2/3.0.wav'), wav);
      expect((await buildLibrary(root)).manifest.samples[0].error).toBeTruthy();
      await writeFile(join(root, 'bank2/3.0.wav.info'), info);
      await writeFile(join(root, 'bank2/3.1.wav'), wav);
      const result = await buildLibrary(root);
      expect(result.manifest.samples).toHaveLength(1);
      expect(result.manifest.samples[0]).toMatchObject({ bank: 1, sample: 3, path: 'bank2/3.0.wav' });
      expect(result.manifest.samples[0].error).toBeUndefined();
      await rm(join(root, 'bank2/3.0.wav'));
      expect((await buildLibrary(root)).manifest.samples).toHaveLength(0);
    } finally { await rm(root, { recursive: true, force: true }); }
  });
  test('parses the available reference library, including its 16/32/64-slice samples', async () => {
    const result = await buildLibrary(fileURLToPath(new URL('../reference', import.meta.url)));
    // The reference directory is deliberately replaceable and may be absent in CI.
    for (const sample of result.manifest.samples) expect(sample.error, sample.path).toBeUndefined();
    if (result.manifest.samples.length === 96) {
      const counts = new Set(result.manifest.samples.map(s => JSON.parse(result.assets.get(s.url!)!).slices.length));
      expect(counts).toEqual(new Set([16, 32, 64]));
    }
  });
});
