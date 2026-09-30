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
assert.equal(first.app.settingsMidiChannel, '1');
for (let n = 1; n <= 16; n++) {
    first.app.settingsMidiChannel = '16';
    first.app.settingsMidiChannel = String(n);
    const saved = JSON.parse(first.messages.at(-1).state);
    assert.equal(saved.settingsMidiChannel, String(n));
    const reopened = workspace();
    reopened.context.receiveState({ data: JSON.stringify({ action: 'getstate', state: JSON.stringify(saved) }) });
    assert.equal(reopened.app.settingsMidiChannel, String(n));
    for (const settingsOnly of [false, true]) {
        reopened.app.submitForm(settingsOnly);
        assert.equal(reopened.downloads.at(-1).body.settingsMidiChannel, String(n));
    }
}
for (const invalid of [undefined, '', '0', '17', '01', 'invalid']) {
    const next = workspace();
    next.context.receiveState({data: JSON.stringify({action:'getstate', state:JSON.stringify({settingsMidiChannel:invalid})})});
    assert.equal(next.app.settingsMidiChannel, '1');
}
console.log('Web MIDI channel defaults, persistence, restoration and downloads passed');
