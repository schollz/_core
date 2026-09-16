import { decodeMessage, isFresh, type LegacyInfo } from './protocol';
import type { Playback } from './types';

type Port = { id: string; name: string };
export interface ButtonPress { button: number; at: number; bank: number; sample: number; slice: number; }
export interface MidiState {
  connection: 'idle' | 'connecting' | 'connected' | 'disconnected' | 'choose' | 'error' | 'unsupported';
  inputs: Port[];
  outputs: Port[];
  inputId: string;
  outputId: string;
  playback?: Playback;
  buttonPress?: ButtonPress;
  legacy?: LegacyInfo;
  receivedAt?: number;
  legacyAt?: number;
  connectedAt?: number;
  error?: string;
}
const initial = (): MidiState => ({ connection: 'idle', inputs: [], outputs: [], inputId: '', outputId: '' });
const matches = (name?: string | null) => /zeptocore/i.test(name ?? '');

export class MidiConnection {
  state = initial();
  private access?: MIDIAccess;
  private input?: MIDIInput;
  private output?: MIDIOutput;
  private timer?: ReturnType<typeof setInterval>;
  private listeners = new Set<() => void>();
  private generation = 0;
  constructor(private request = () => navigator.requestMIDIAccess({ sysex: true }), private now = () => performance.now()) {}
  subscribe = (listener: () => void) => { this.listeners.add(listener); return () => { this.listeners.delete(listener); }; };
  snapshot = () => this.state;
  private update(patch: Partial<MidiState>) { this.state = { ...this.state, ...patch }; this.listeners.forEach(f => f()); }

  async connectIfPermitted() {
    if (this.state.connection === 'connected' || this.state.connection === 'connecting' || this.state.connection === 'choose') return;
    if (this.access) { this.refreshPorts(); return; }
    if (!navigator.requestMIDIAccess || !navigator.permissions || !window.isSecureContext) return;
    const token = this.generation;
    try {
      const permission = await navigator.permissions.query({ name: 'midi', sysex: true } as unknown as PermissionDescriptor);
      if (permission.state === 'granted' && token === this.generation) await this.connect();
    } catch {
      // Browsers without MIDI permission queries retain the manual Connect button.
    }
  }

  reconnect() {
    // Retry existing permission/access after USB errors or missed port events.
    if (this.access && (this.state.connection === 'disconnected' || this.state.connection === 'error')) this.refreshPorts();
  }

  async connect() {
    if (this.access) this.access.onstatechange = null;
    this.detach();
    const token = this.generation;
    this.update({ connection: 'connecting', error: undefined });
    try {
      const access = await this.request();
      if (token !== this.generation) return;
      this.access = access;
      if (!this.access.sysexEnabled) throw new Error('Allow MIDI system exclusive (SysEx) access to read the zeptocore.');
      this.access.onstatechange = () => this.refreshPorts();
      this.refreshPorts();
    } catch (error) {
      if (token !== this.generation) return;
      this.update({ connection: 'error', error: error instanceof Error ? error.message : 'MIDI access was denied.' });
    }
  }

  private refreshPorts() {
    if (!this.access) return;
    const inputs = Array.from(this.access.inputs.values()).filter(p => p.state === 'connected');
    const outputs = Array.from(this.access.outputs.values()).filter(p => p.state === 'connected');
    this.update({ inputs: inputs.map(p => ({ id: p.id, name: p.name ?? 'MIDI input' })),
      outputs: outputs.map(p => ({ id: p.id, name: p.name ?? 'MIDI output' })) });
    if (this.input?.state === 'connected' && this.output?.state === 'connected') return;
    this.detach();
    const namedInputs = inputs.filter(p => matches(p.name)), namedOutputs = outputs.filter(p => matches(p.name));
    const input = inputs.find(p => p.id === this.state.inputId) ?? (namedInputs.length === 1 ? namedInputs[0] : undefined);
    const output = outputs.find(p => p.id === this.state.outputId) ?? (namedOutputs.length === 1 ? namedOutputs[0] : undefined);
    if (input && output) void this.select(input.id, output.id);
    else this.update({ connection: inputs.length && outputs.length ? 'choose' : 'disconnected' });
  }

  async select(inputId: string, outputId: string) {
    this.detach();
    const token = this.generation;
    const input = this.access?.inputs.get(inputId), output = this.access?.outputs.get(outputId);
    this.update({ inputId, outputId, playback: undefined, buttonPress: undefined, legacy: undefined, receivedAt: undefined, legacyAt: undefined });
    if (!input || !output) { this.update({ connection: 'choose' }); return; }
    // Set before opening: opening a port itself produces statechange events.
    this.input = input; this.output = output;
    try {
      await Promise.all([input.open(), output.open()]);
      if (token !== this.generation) return;
      input.onmidimessage = event => {
        if (token !== this.generation) return;
        const message = event.data ? decodeMessage(event.data) : null;
        if (!message) return;
        const at = event.timeStamp || this.now();
        if (message.kind === 'button') {
          const state = this.state.playback;
          if (state?.valid && isFresh(this.state.receivedAt, at))
            this.update({ buttonPress: { button: message.button, at, bank: state.bank, sample: state.sample, slice: state.slice } });
        }
        else if (message.kind === 'view') {
          const press = this.state.buttonPress;
          // The first snapshot after the press supplies its resulting slice.
          // Later automatic steps must not move the callout to another slice.
          const pending = press && press.at >= (this.state.receivedAt ?? 0);
          const buttonPress = pending ? message.state.valid && message.state.bank === press.bank && message.state.sample === press.sample
            ? { ...press, slice: message.state.slice } : undefined : press;
          this.update({ playback: message.state, receivedAt: at, buttonPress });
        }
        else this.update({ legacy: message.state, legacyAt: at });
      };
      this.update({ connection: 'connected', connectedAt: this.now(), error: undefined });
      const poll = () => {
        try {
          output.send([0x89, 5, 0]);
          if (!isFresh(this.state.receivedAt, this.now())) output.send([0x89, 4, 0]);
        } catch (error) {
          this.detach(); this.update({ connection: 'disconnected', error: String(error) });
        }
      };
      poll();
      if (token === this.generation) this.timer = setInterval(poll, 500);
    } catch (error) {
      if (token !== this.generation) return;
      this.detach(); this.update({ connection: 'error', error: String(error) });
    }
  }

  private detach() {
    this.generation++;
    if (this.timer) clearInterval(this.timer);
    this.timer = undefined;
    if (this.input) this.input.onmidimessage = null;
    this.input = undefined; this.output = undefined;
  }
  dispose() {
    const input = this.input, output = this.output;
    if (this.access) this.access.onstatechange = null;
    this.detach();
    this.access = undefined;
    void input?.close(); void output?.close();
  }
}
