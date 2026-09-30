const assert = require('node:assert/strict');
const fs = require('node:fs');
const vm = require('node:vm');
const path = require('node:path');
const root = path.join(__dirname, '../..');
const source = fs.readFileSync(path.join(root, 'core/src/server/static/core-midi.js'), 'utf8');
assert.equal(fs.readFileSync(path.join(root, 'docs/themes/zeptocore/static/static/core-midi.js'), 'utf8'), source);
vm.runInThisContext(source);
const frame = text => [0xf0, ...Buffer.from(text), 0xf7];
function fixture() {
    let now = 0;
    const sent = [], ready = [], errors = [];
    const link = new CoreMidiManagement(data => sent.push(data), mode => ready.push(mode), error => errors.push(error), () => now);
    return {link, sent, ready, errors, at(n) { now = n; link.tick(); }};
}
const modern = fixture(); modern.link.start();
assert.deepEqual(modern.sent, [CoreMidiManagement.frame('hello')]);
assert.throws(() => modern.link.command('bootloader'), /not ready/);
modern.link.receive(frame('version=v8.0.3')); assert.equal(modern.link.mode, null);
modern.link.receive(frame('core_caps=1')); assert.equal(modern.link.mode, 'sysex');
for (const op of ['version', 'bootloader', 'info', 'slices', 'view']) {
    modern.link.command(op); assert.deepEqual(modern.sent.at(-1), CoreMidiManagement.frame(op));
}
modern.link.command('key', 127); assert.deepEqual(modern.sent.at(-1), CoreMidiManagement.frame('key,127'));
for (const key of [-1, 128, NaN, undefined]) assert.throws(() => modern.link.command('key', key));
assert.throws(() => modern.link.command('constructor'));
modern.at(3000); assert.equal(modern.errors.length, 0);
modern.link.reset(); assert.throws(() => modern.link.command('version'));
modern.link.receive(frame('core_caps=1')); assert.equal(modern.link.mode, null);
const old = fixture(); old.link.start(); old.at(499); assert.equal(old.sent.length, 1);
old.at(500); assert.equal(old.sent.length, 2); old.at(1000);
assert.deepEqual(old.sent.at(-1), [0xb0, 1, 0]);
assert.throws(() => old.link.command('view'));
old.link.receive(frame('version=v7')); old.link.command('view'); assert.deepEqual(old.sent.at(-1), [0x89, 5, 0]);
old.link.command('bootloader'); assert.deepEqual(old.sent.at(-1), [0xb0, 0, 0]);
old.link.receive(frame('core_caps=1')); old.link.command('view');
assert.deepEqual(old.sent.at(-1), CoreMidiManagement.frame('view'));
old.link.receive(frame('version=v7')); assert.equal(old.link.mode, 'sysex');
const silent = fixture(); silent.link.start(); silent.at(500); silent.at(1000); silent.at(2000); silent.at(3000);
assert.equal(silent.errors.length, 1); assert.throws(() => silent.link.command('bootloader'));
assert.ok(!silent.sent.some(data => data[0] === 0xb0 && data[1] === 0));
console.log('Browser management negotiation, safe fallback, timeout and reconnect passed');
