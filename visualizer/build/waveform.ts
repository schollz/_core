import type { Waveform } from '../src/types';

export function parseInfo(bytes: Buffer) {
  if (bytes.length < 11) throw new Error('Truncated sample metadata');
  const size = bytes.readUInt32LE(0), flags = bytes.readUInt32LE(4);
  const count = bytes[10];
  if (!size || !count || bytes.length < 11 + count * 9) throw new Error('Invalid slice metadata');
  if (((flags >>> 16) & 127) > 1) throw new Error('Unsupported sample metadata version');
  const slices = Array.from({ length: count }, (_, i) => {
    const start = bytes.readInt32LE(11 + i * 4);
    const stop = bytes.readInt32LE(11 + count * 4 + i * 4);
    if (start < 0 || stop <= start || stop > size) throw new Error('Slice outside sample bounds');
    return { start, stop };
  });
  return { size, slices, bpm: flags & 511, tempoMatch: !!(flags & (1 << 13)),
    sampleRate: 44100 * (((flags >>> 14) & 1) + 1), channels: ((flags >>> 15) & 1) + 1 };
}

export function parseWav(bytes: Buffer) {
  if (bytes.length < 12 || bytes.toString('ascii', 0, 4) !== 'RIFF' ||
      bytes.toString('ascii', 8, 12) !== 'WAVE') throw new Error('Expected a RIFF WAV file');
  let format: { channels: number; sampleRate: number; blockAlign: number } | undefined;
  let data: Buffer | undefined;
  const end = bytes.readUInt32LE(4) + 8;
  if (end > bytes.length) throw new Error('Truncated WAV file');
  for (let p = 12; p + 8 <= end;) {
    const tag = bytes.toString('ascii', p, p + 4), length = bytes.readUInt32LE(p + 4);
    const start = p + 8;
    if (start + length > end) throw new Error('Truncated WAV chunk');
    if (tag === 'fmt ') {
      if (length < 16 || bytes.readUInt16LE(start) !== 1 || bytes.readUInt16LE(start + 14) !== 16)
        throw new Error('Expected 16-bit PCM audio');
      const channels = bytes.readUInt16LE(start + 2), sampleRate = bytes.readUInt32LE(start + 4);
      const blockAlign = bytes.readUInt16LE(start + 12);
      if (![1, 2].includes(channels) || ![44100, 88200].includes(sampleRate) || blockAlign !== channels * 2)
        throw new Error('Unsupported WAV format');
      format = { channels, sampleRate, blockAlign };
    } else if (tag === 'data') data = bytes.subarray(start, start + length);
    p = start + length + (length & 1);
  }
  if (!format || !data || data.length % format.blockAlign) throw new Error('Missing or invalid WAV data');
  return { ...format, data };
}

export function makeWaveform(wav: Buffer, metadata: Buffer, bank: number, sample: number, resolution = 4096): Waveform {
  const info = parseInfo(metadata), audio = parseWav(wav);
  if (audio.sampleRate !== info.sampleRate || audio.channels !== info.channels ||
      info.size !== audio.data.length - audio.sampleRate * audio.blockAlign)
    throw new Error('WAV and metadata disagree about format or padding');
  if (info.size % audio.blockAlign || info.slices.some(s => s.start % audio.blockAlign || s.stop % audio.blockAlign))
    throw new Error('Unaligned sample boundaries');
  const frames = info.size / audio.blockAlign;
  const padding = audio.sampleRate / 2 * audio.blockAlign;
  const bins = Math.min(resolution, frames);
  const peaks = Array.from({ length: audio.channels }, () => [] as number[]);
  for (let i = 0; i < bins; i++) {
    const first = Math.floor(i * frames / bins), last = Math.floor((i + 1) * frames / bins);
    for (let ch = 0; ch < audio.channels; ch++) {
      let lo = 32767, hi = -32768;
      for (let f = first; f < last; f++) {
        const value = audio.data.readInt16LE(padding + f * audio.blockAlign + ch * 2);
        lo = Math.min(lo, value); hi = Math.max(hi, value);
      }
      peaks[ch].push(lo, hi);
    }
  }
  const bytesPerSecond = audio.sampleRate * audio.blockAlign;
  return { bank, sample, sampleRate: audio.sampleRate, channels: audio.channels,
    duration: info.size / bytesPerSecond, bpm: info.bpm, tempoMatch: info.tempoMatch,
    slices: info.slices.map(s => ({ start: s.start / bytesPerSecond, stop: s.stop / bytesPerSecond })), peaks };
}
