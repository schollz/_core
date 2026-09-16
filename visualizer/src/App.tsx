import { useEffect, useState, useSyncExternalStore } from 'react';
import { MidiConnection } from './midi';
import { isFresh } from './protocol';
import { Waveform } from './Waveform';
import { EffectIcons } from './EffectIcons';
import type { Library, Waveform as WaveformData } from './types';

const midi = new MidiConnection();
const number = (n?: number) => n === undefined ? '—' : String(n + 1).padStart(2, '0');
const json = async <T,>(url: string, signal?: AbortSignal): Promise<T> => {
  const response = await fetch(url, { signal, cache: 'no-store' });
  if (!response.ok) throw new Error(`Could not load ${url} (${response.status})`);
  return response.json();
};

export default function App() {
  const device = useSyncExternalStore(midi.subscribe, midi.snapshot);
  const [library, setLibrary] = useState<Library>({ samples: [] });
  const [libraryError, setLibraryError] = useState('');
  const [waves, setWaves] = useState<Record<string, WaveformData>>({});
  const [error, setError] = useState('');
  const [setup, setSetup] = useState(false);
  const [now, setNow] = useState(performance.now());
  const supported = typeof navigator.requestMIDIAccess === 'function' && window.isSecureContext;
  useEffect(() => {
    const timer = setInterval(() => setNow(performance.now()), 200);
    void midi.connectIfPermitted();
    const reconnectTimer = setInterval(() => midi.reconnect(), 2000);
    const resume = () => { if (document.visibilityState === 'visible') void midi.connectIfPermitted(); };
    document.addEventListener('visibilitychange', resume);
    let active = true;
    const refresh = () => { void json<Library>('./data/library.json').then(data => { if (active) { setLibrary(data); setLibraryError(''); } }).catch(e => { if (active) setLibraryError(String(e)); }); };
    refresh();
    import.meta.hot?.on('reference-changed', refresh);
    return () => { active = false; clearInterval(timer); clearInterval(reconnectTimer); document.removeEventListener('visibilitychange', resume); import.meta.hot?.off('reference-changed', refresh); midi.dispose(); };
  }, []);
  const live = device.connection === 'connected';
  const fresh = live && isFresh(device.receivedAt, now);
  const selected = fresh ? device.playback : device.legacy &&
    (!device.playback || (device.legacyAt ?? 0) > (device.receivedAt ?? 0)) ? device.legacy : device.playback;
  const bank = selected?.bank ?? library.samples[0]?.bank;
  const sample = selected?.sample ?? library.samples[0]?.sample;
  const source = library.samples.find(s => s.bank === bank && s.sample === sample);
  const wave = source?.url ? waves[source.url] : undefined;
  useEffect(() => {
    const controller = new AbortController();
    setError('');
    if (source?.error) setError(source.error);
    else if (source?.url && !wave) {
      const url = source.url;
      void json<WaveformData>(`./${url}`, controller.signal)
        .then(data => setWaves(cache => ({ ...cache, [url]: data })))
        .catch(e => { if (!controller.signal.aborted) setError(String(e)); });
    }
    return () => controller.abort();
  }, [source?.url, source?.error, wave]);
  // Preload with bounded concurrency so sample changes normally need no fetch.
  useEffect(() => {
    const controller = new AbortController();
    const queue = library.samples.filter(s => s.url && !s.error).map(s => s.url!);
    const worker = async () => {
      while (queue.length && !controller.signal.aborted) {
        const url = queue.shift()!;
        try {
          const data = await json<WaveformData>(`./${url}`, controller.signal);
          if (!controller.signal.aborted) setWaves(cache => ({ ...cache, [url]: cache[url] ?? data }));
        } catch { /* A selected sample retries and reports its own load error. */ }
      }
    };
    void Promise.all([worker(), worker(), worker()]);
    return () => controller.abort();
  }, [library]);
  const display = device.displayPlayback;
  const displayFresh = live && isFresh(device.displayAt, now);
  const activeSlice = displayFresh && display?.valid && wave?.slices[display.slice] ? display.slice : undefined;
  const showSetup = setup || device.connection === 'choose';
  return <main>
    <header className="topbar" aria-label="Current sample">
      <div className="selection"><span>BANK <b>{number(bank)}</b></span><span>SAMPLE <b>{number(sample)}</b></span></div>
      <button className="midi-button" disabled={!supported || device.connection === 'connecting'} onClick={() => live ? setSetup(!setup) : void midi.connect()}>{live ? 'MIDI' : device.connection === 'connecting' ? 'WAIT' : 'CONNECT'}</button>
    </header>
    {!supported && <p className="notice">Open this page in Chrome or Edge on localhost to use Web MIDI.</p>}
    {device.error && <p className="notice" role="alert">{device.error} Allow MIDI and SysEx access in your browser, then reconnect.</p>}
    {showSetup && <div className="ports">
      <label>MIDI input<select value={device.inputId} onChange={e => void midi.select(e.target.value, device.outputId)}><option value="">Select input</option>{device.inputs.map(p => <option key={p.id} value={p.id}>{p.name}</option>)}</select></label>
      <label>MIDI output<select value={device.outputId} onChange={e => void midi.select(device.inputId, e.target.value)}><option value="">Select output</option>{device.outputs.map(p => <option key={p.id} value={p.id}>{p.name}</option>)}</select></label>
    </div>}
    <section className="scope" aria-label="Sample waveform">
      <EffectIcons mask={fresh && device.playback?.valid && wave && device.playback.bank === wave.bank && device.playback.sample === wave.sample ? device.playback.effects : undefined} />
      {wave ? <Waveform wave={wave} playback={display} buttonPress={device.buttonPress} receivedAt={device.displayAt} live={live} /> : <div className="empty" role="status"><strong>{error || libraryError ? 'Waveform unavailable' : source ? 'Reading waveform' : 'No matching sample'}</strong><p>{error || libraryError || (library.samples.length ? `Add the matching WAV and .info files for bank ${number(bank)}, sample ${number(sample)} to reference.` : 'Place your bank folders in visualizer/reference.')}</p></div>}
    </section>
    <footer>
      <div className="transport"><span>SLICE <b>{number(activeSlice)}</b><span className="slice-count">/{wave?.slices.length ?? '—'}</span></span><span><b>{selected?.bpm ?? wave?.bpm ?? '—'}</b> BPM</span></div>
    </footer>
  </main>;
}
