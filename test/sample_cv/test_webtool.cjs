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
assert.equal(first.app.settingsSampleCVMapping, 'bank');
first.app.settingsSampleCVMapping = '1voct';
assert.equal(first.messages.at(-1).action, 'updatestate');
first.app.settingsOverrideWithReset = 'sample';
assert.equal(first.app.settingsSampleCVMapping, '1voct');
const saved = JSON.parse(first.messages.at(-1).state);
assert.equal(saved.settingsSampleCVMapping, '1voct');

const second = workspace();
second.context.receiveState({ data: JSON.stringify({ action: 'getstate', state: JSON.stringify(saved) }) });
assert.equal(second.app.settingsSampleCVMapping, '1voct');
assert.equal(second.app.settingsOverrideWithReset, 'sample');
second.app.settingsOverrideWithReset = 'none';
assert.equal(second.app.settingsSampleCVMapping, '1voct');
for (const settingsOnly of [false, true]) {
    second.app.submitForm(settingsOnly);
    const download = second.downloads.at(-1);
    assert.ok(download.url.endsWith('settingsOnly=' + settingsOnly));
    assert.equal(download.body.settingsSampleCVMapping, '1voct');
}
second.app.settingsSampleCVMapping = 'bank';
assert.equal(JSON.parse(second.messages.at(-1).state).settingsSampleCVMapping, 'bank');
second.app.submitForm(true);
assert.equal(second.downloads.at(-1).body.settingsSampleCVMapping, 'bank');

for (const legacy of [{}, { settingsSampleCVMapping: 'invalid' }]) {
    second.app.settingsSampleCVMapping = '1voct';
    second.context.receiveState({ data: JSON.stringify({ action: 'getstate', state: JSON.stringify(legacy) }) });
    assert.equal(second.app.settingsSampleCVMapping, 'bank');
}
console.log('Webtool defaults, watched persistence, restoration, and download payloads passed');
