import type { Playback } from './types';

export interface LegacyInfo { bank: number; sample: number; bpm: number; stopped: boolean; muted: boolean; }
export type Message = { kind: 'view'; state: Playback } | { kind: 'info'; state: LegacyInfo } | { kind: 'button'; button: number };

export function decodeMessage(bytes: ArrayLike<number>): Message | null {
  // Physical performance pads: notes 0–15 on channels 1–3 (jump/mash/bass).
  // Sequencer-generated notes use other channels and must not create callouts.
  if (bytes.length === 3 && bytes[0] >= 0x90 && bytes[0] <= 0x92 &&
      bytes[1] >= 0 && bytes[1] < 16 && bytes[2] > 0 && bytes[2] <= 127)
    return { kind: 'button', button: bytes[1] + 1 };
  if (bytes.length < 3 || bytes.length > 128 || bytes[0] !== 0xf0 || bytes[bytes.length - 1] !== 0xf7) return null;
  const body = Array.from(bytes).slice(1, -1);
  if (body.some(b => b < 32 || b > 126)) return null;
  const text = String.fromCharCode(...body);
  const split = text.indexOf('=');
  const name = text.slice(0, split), fields = text.slice(split + 1).split(',');
  if (!fields.every(f => /^\d+$/.test(f))) return null;
  const v = fields.map(Number);
  const bit = (n: number) => n === 0 || n === 1;
  if (name === 'view' && ((v.length === 10 && v[0] === 1) || (v.length === 11 && v[0] === 2))) {
    const [, bank, sample, slice, trigger, bpm, forward, stopped, muted, valid, effects] = v;
    if (bank > 15 || sample > 15 || slice > 254 || trigger > 0xffffffff || bpm > 511 ||
        ![forward, stopped, muted, valid].every(bit) || (effects !== undefined && effects > 65535)) return null;
    return { kind: 'view', state: { bank, sample, slice, trigger, bpm,
      forward: !!forward, stopped: !!stopped, muted: !!muted, valid: !!valid,
      ...(effects !== undefined ? { effects } : {}) } };
  }
  if (name === 'info' && v.length === 7 && v[0] < 16 && v[1] < 16 && v[2] <= 511 && bit(v[5]) && bit(v[6]))
    return { kind: 'info', state: { bank: v[0], sample: v[1], bpm: v[2], stopped: !!v[5], muted: !!v[6] } };
  return null;
}

export const TELEMETRY_STALE_MS = 1500;
export const isFresh = (receivedAt: number | undefined, now: number) =>
  receivedAt !== undefined && now - receivedAt < TELEMETRY_STALE_MS;
