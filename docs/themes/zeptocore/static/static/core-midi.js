// Shared browser management protocol. Keep the documentation-site copy identical.
// The caller owns the timer and calls tick() at least every 500 ms.
// SysEx text follows the manufacturer ID 7D. Firmware that predates it reads
// text straight after F0, so the hello retry goes without it and later
// commands use the form the device answered in.
globalThis.CoreMidiManagement = class CoreMidiManagement {
    constructor(send, ready = () => {}, error = () => {}, now = () => Date.now()) {
        this.send = send; this.ready = ready; this.error = error; this.now = now;
        this.reset();
    }
    reset() { this.mode = null; this.started = null; this.stage = 0; this.id = true; }
    static frame(operation, id = true) {
        return [0xf0, ...(id ? [0x7d] : []), ...Array.from('core_cmd=1,' + operation, c => c.charCodeAt(0)), 0xf7];
    }
    // The text of a SysEx message, with or without the ID, or null.
    static text(data) {
        if (data[0] !== 0xf0 || data[data.length - 1] !== 0xf7) return null;
        return String.fromCharCode(...Array.from(data).slice(data[1] === 0x7d ? 2 : 1, -1));
    }
    start() {
        this.reset(); this.started = this.now();
        this.send(CoreMidiManagement.frame('hello'));
    }
    tick() {
        if (this.mode || this.started === null) return;
        const elapsed = this.now() - this.started;
        if (this.stage === 0 && elapsed >= 500) {
            this.stage = 1; this.send(CoreMidiManagement.frame('hello', false));
        }
        if (this.stage === 1 && elapsed >= 1000) {
            this.stage = 2; this.send([0xb0, 1, 0]);
        }
        if (this.stage === 2 && elapsed >= 2000) {
            this.stage = 3; this.error('Device management is unavailable: no protocol response. Reconnect to retry.');
        }
    }
    receive(data) {
        const text = this.started === null ? null : CoreMidiManagement.text(data);
        if (text === null) return false;
        let mode = null;
        if (text === 'core_caps=1') { mode = 'sysex'; this.id = data[1] === 0x7d; }
        else if (this.stage >= 2 && /^version=.+$/.test(text) && this.mode !== 'sysex') mode = 'legacy';
        if (mode && mode !== this.mode) { this.mode = mode; this.ready(mode); }
        return text === 'core_caps=1';
    }
    command(operation, argument) {
        const commands = { version: [0xb0, 1, 0], bootloader: [0xb0, 0, 0],
            info: [0x89, 4, 0], slices: [0x89, 3, 0], view: [0x89, 5, 0] };
        const key = operation === 'key' && Number.isInteger(argument) && argument >= 0 && argument <= 127;
        if (!key && !Object.prototype.hasOwnProperty.call(commands, operation)) throw new Error('Unsupported management command');
        if (!this.mode) throw new Error('Device management is not ready. Wait for protocol detection or reconnect.');
        this.send(this.mode === 'sysex' ? CoreMidiManagement.frame(operation + (key ? ',' + argument : ''), this.id)
            : key ? [0x89, argument, 1] : commands[operation]);
    }
};
