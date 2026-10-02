// Exercise the real webtool script without a browser, network, or downloads.
const assert = require('node:assert/strict');
const fs = require('node:fs');
const path = require('node:path');
const vm = require('node:vm');

const source = fs.readFileSync(path.join(__dirname, '../../core/src/server/static/app.js'), 'utf8');
function workspace() {
    const messages = [];
    const downloads = [];
    const context = vm.createContext({
        console: { log() {}, error() {} },
        window: {
            location: { pathname: '/sample-cv-test' },
            addEventListener() {}, clearTimeout() {}, setTimeout() { return 0; },
        },
        document: { cookie: '', getElementById() { return { style: {} }; } },
        fetch(url, options) {
            downloads.push({ url, body: JSON.parse(options.body) });
            // Leave the response pending; no browser download is performed.
            return new Promise(() => {});
        },
        Vue: function (options) {
            const instance = Object.assign({}, options.data, options.methods);
            // Run the registered property watchers when a setting changes.
            return new Proxy(instance, {
                set(target, key, value) {
                    const previous = target[key];
                    target[key] = value;
                    const watcher = options.watch[key];
                    if (value !== previous && typeof watcher === 'string') {
                        target[watcher].call(target);
                    }
                    return true;
                },
            });
        },
    });
    vm.runInContext(source + '\nglobalThis.receiveState = socketMessageListener;', context);
    context.socket = { send(message) { messages.push(JSON.parse(message)); } };
    return { context, app: context.app, messages, downloads };
}

const first = workspace();
assert.equal(first.app.settingsStartTempo, 'default');
first.app.setStartTempoMode('fixed');
assert.equal(first.app.settingsStartTempo, '130');
const saved = JSON.parse(first.messages.at(-1).state);
assert.equal(saved.settingsStartTempo, '130');
const second = workspace();
second.context.receiveState({ data: JSON.stringify({ action: 'getstate', state: JSON.stringify(saved) }) });
assert.equal(second.app.settingsStartTempo, '130');
assert.equal(second.app.startTempoBpm, '130');
for (const value of ['30', '145', '300']) {
    second.app.startTempoBpm = value;
    assert.equal(second.app.commitStartTempo(), true);
    for (const settingsOnly of [false, true]) {
        second.app.submitForm(settingsOnly);
        assert.equal(second.downloads.at(-1).body.settingsStartTempo, value);
    }
}
for (const value of ['', '0', '29', '301', '130.5', '0130', 'garbage']) {
    second.app.startTempoBpm = value;
    assert.equal(second.app.commitStartTempo(), false);
    assert.equal(second.app.settingsStartTempo, '300');
    const before = second.downloads.length;
    second.app.submitForm(true);
    assert.equal(second.downloads.length, before);
}
second.app.setStartTempoMode('default');
second.app.submitForm(true);
assert.equal(second.downloads.at(-1).body.settingsStartTempo, 'default');
for (const legacy of [{}, {settingsStartTempo:'invalid'}]) {
    second.context.receiveState({ data: JSON.stringify({ action: 'getstate', state: JSON.stringify(legacy) }) });
    assert.equal(second.app.settingsStartTempo, 'default');
    assert.equal(second.app.startTempoBpm, '130');
}
console.log('Web start tempo: persistence, validation and both download payloads passed');
