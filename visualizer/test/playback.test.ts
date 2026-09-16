import { expect, test, vi, afterEach } from 'vitest';
import { decodeMessage, isFresh } from '../src/protocol';
import { Playhead } from '../src/playhead';
import { MidiConnection } from '../src/midi';
import type { Playback, Waveform } from '../src/types';

const sysex = (text: string) => Uint8Array.from([0xf0, ...Array.from(text).map(c => c.charCodeAt(0)), 0xf7]);
const state: Playback = { bank: 0, sample: 0, slice: 0, trigger: 1, bpm: 120, forward: true, stopped: false, muted: false, valid: true };
const wave: Waveform = { bank: 0, sample: 0, bpm: 120, tempoMatch: true, sampleRate: 44100, channels: 1, duration: 2, slices: [{ start: 0, stop: 1 }, { start: 1, stop: 2 }], peaks: [] };

test('strictly decodes versioned snapshots and legacy status, ignores clock and malformed frames', () => {
  expect(decodeMessage(sysex('view=1,0,0,0,1,120,1,0,0,1'))).toEqual({ kind: 'view', state });
  expect(decodeMessage(sysex('info=1,2,170,180,0,1,0'))).toMatchObject({ kind: 'info', state: { bank: 1, sample: 2, stopped: true } });
  for (const text of ['view=2,0,0,0,1,120,1,0,0,1', 'view=1,16,0,0,1,120,1,0,0,1', 'view=1,0,0,-1,1,120,1,0,0,1', 'view=1,0,0,0,1,120,3,0,0,1', 'view=1,0', 'view=1,0,0,0,1.5,120,1,0,0,1']) expect(decodeMessage(sysex(text))).toBeNull();
  expect(decodeMessage([0xf8])).toBeNull(); expect(decodeMessage([0xf0, 0xff, 0xf7])).toBeNull();
  expect(isFresh(0, 1499)).toBe(true); expect(isFresh(0, 1500)).toBe(false); expect(isFresh(undefined, 0)).toBe(false);
});
test('cursor resynchronizes on repeated triggers but not heartbeats; clamps to the active slice', () => {
  const clock = new Playhead(); clock.update(state, wave, 1000);
  expect(clock.value(1400)).toBeCloseTo(0.4);
  clock.update({ ...state }, wave, 1500); expect(clock.value(1700)).toBeCloseTo(0.7);
  clock.update({ ...state, trigger: 2 }, wave, 1800); expect(clock.value(1900)).toBeCloseTo(0.1);
  expect(clock.value(5000)).toBe(1);
  clock.update({ ...state, trigger: 3, slice: 1 }, wave, 6000); expect(clock.value(6100)).toBeCloseTo(1.1);
});
test('cursor handles tempo changes, reverse, stop, invalidity and disabled tempo match', () => {
  const clock = new Playhead(); clock.update(state, wave, 0);
  clock.update({ ...state, bpm: 240 }, wave, 200); expect(clock.value(400)).toBeCloseTo(0.6);
  clock.update({ ...state, forward: false }, wave, 500); expect(clock.value(700)).toBeCloseTo(0.8);
  clock.update({ ...state, forward: false, stopped: true }, wave, 800); expect(clock.value(2000)).toBeCloseTo(0.7);
  clock.update({ ...state, valid: false }, wave, 2100); expect(clock.value(2200)).toBeNull();
  clock.update({ ...state, bpm: 240, trigger: 2 }, { ...wave, tempoMatch: false }, 3000); expect(clock.value(3500)).toBeCloseTo(0.5);
});

function fakeAccess(count = 1) {
  const inputs = new Map(), outputs = new Map();
  for (let i = 0; i < count; i++) {
    inputs.set(`in${i}`, { id: `in${i}`, name: 'zeptocore', state: 'connected', open: vi.fn(async () => {}), close: vi.fn(async () => {}), onmidimessage: null });
    outputs.set(`out${i}`, { id: `out${i}`, name: 'zeptocore', state: 'connected', open: vi.fn(async () => {}), close: vi.fn(async () => {}), send: vi.fn() });
  }
  return { inputs, outputs, sysexEnabled: true, onstatechange: null as null | (() => void) };
}
afterEach(() => vi.useRealTimers());
test('connects, renews telemetry, falls back for old firmware and reconnects without duplicate timers', async () => {
  vi.useFakeTimers(); let now = 100;
  const access = fakeAccess(), input = access.inputs.get('in0'), output = access.outputs.get('out0');
  const connection = new MidiConnection(async () => access as unknown as MIDIAccess, () => now);
  await connection.connect(); await Promise.resolve(); await Promise.resolve();
  expect(connection.state.connection).toBe('connected');
  expect(output.send.mock.calls.map((c: unknown[]) => c[0])).toEqual([[0x89, 5, 0], [0x89, 4, 0]]);
  input.onmidimessage({ data: sysex('info=0,0,120,180,0,0,0'), timeStamp: now });
  expect(connection.state.playback).toBeUndefined(); expect(connection.state.legacy?.bpm).toBe(120);
  input.onmidimessage({ data: sysex('view=1,0,0,0,1,120,1,0,0,1'), timeStamp: now });
  output.send.mockClear(); now += 500; await vi.advanceTimersByTimeAsync(500);
  expect(output.send.mock.calls).toHaveLength(1);
  input.state = output.state = 'disconnected'; access.onstatechange!();
  expect(connection.state.connection).toBe('disconnected'); expect(input.onmidimessage).toBeNull();
  input.state = output.state = 'connected'; access.onstatechange!(); await Promise.resolve(); await Promise.resolve();
  expect(connection.state.connection).toBe('connected');
  output.send.mockClear(); now += 500; await vi.advanceTimersByTimeAsync(500);
  expect(output.send.mock.calls).toHaveLength(2); // one renewal + one legacy query until a new snapshot
  connection.dispose(); output.send.mockClear(); await vi.advanceTimersByTimeAsync(1000);
  expect(output.send).not.toHaveBeenCalled();
});
test('requires explicit selection for multiple devices and exposes permission failures', async () => {
  const multiple = new MidiConnection(async () => fakeAccess(2) as unknown as MIDIAccess);
  await multiple.connect(); expect(multiple.state.connection).toBe('choose'); multiple.dispose();
  const denied = new MidiConnection(async () => { throw new Error('Permission denied'); });
  await denied.connect(); expect(denied.state.connection).toBe('error'); expect(denied.state.error).toBe('Permission denied');
});
test('a failed initial output write does not leave a renewal timer running', async () => {
  vi.useFakeTimers();
  const access = fakeAccess(), output = access.outputs.get('out0');
  output.send.mockImplementation(() => { throw new Error('Port disconnected'); });
  const connection = new MidiConnection(async () => access as unknown as MIDIAccess);
  await connection.connect(); await Promise.resolve(); await Promise.resolve();
  expect(connection.state.connection).toBe('disconnected');
  await vi.advanceTimersByTimeAsync(2000); expect(output.send).toHaveBeenCalledTimes(1);
  connection.dispose();
});
