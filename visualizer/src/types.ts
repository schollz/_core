export interface SampleRef {
  bank: number;
  sample: number;
  path: string;
  url?: string;
  error?: string;
}
export interface Library { samples: SampleRef[]; }
export interface Spectrum {
  bands: number;
  frameRate: number;
  minHz: number;
  maxHz: number;
  frames: number;
  // Frame-major unsigned 8-bit dB levels, encoded compactly for the reference cache.
  levels: string;
}
export interface Waveform {
  bank: number;
  sample: number;
  sampleRate: number;
  channels: number;
  duration: number;
  bpm: number;
  tempoMatch: boolean;
  playMode: number;
  slices: { start: number; stop: number }[];
  // Interleaved minimum/maximum signed 16-bit values, one array per channel.
  peaks: number[][];
  spectrum?: Spectrum;
}
export interface Playback {
  bank: number;
  sample: number;
  slice: number;
  trigger: number;
  bpm: number;
  forward: boolean;
  stopped: boolean;
  muted: boolean;
  valid: boolean;
  effects?: number;
  estimated?: boolean;
}
