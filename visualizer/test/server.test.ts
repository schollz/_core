import { expect, test } from 'vitest';
import { mkdtemp, mkdir, writeFile, rm, readFile, readdir } from 'node:fs/promises';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import type { PreparationProgress } from '../build/library';
import { createVisualizerServer } from '../server/server';

async function fixture() {
  const root = await mkdtemp(join(tmpdir(), 'visualizer-server-'));
  const dist = join(root, 'dist'), reference = join(root, 'reference with spaces');
  await mkdir(join(dist, 'assets'), { recursive: true });
  await mkdir(join(reference, 'bank2'), { recursive: true });
  await writeFile(join(dist, 'index.html'), '<html>visualizer</html>');
  await writeFile(join(dist, 'assets/app.js'), 'console.log("app")');
  await writeFile(join(dist, 'server.mjs'), 'private server bundle');
  await mkdir(join(dist, 'data'));
  await writeFile(join(dist, 'data/library.json'), '{"stale":true}');
  const wav = Buffer.alloc(44 + 44100 * 4);
  wav.write('RIFF'); wav.writeUInt32LE(wav.length - 8, 4); wav.write('WAVEfmt ', 8);
  wav.writeUInt32LE(16, 16); wav.writeUInt16LE(1, 20); wav.writeUInt16LE(1, 22);
  wav.writeUInt32LE(44100, 24); wav.writeUInt32LE(88200, 28);
  wav.writeUInt16LE(2, 32); wav.writeUInt16LE(16, 34);
  wav.write('data', 36); wav.writeUInt32LE(wav.length - 44, 40);
  const info = Buffer.alloc(20);
  info.writeUInt32LE(88200); info.writeUInt32LE(130 | (1 << 13) | (1 << 16), 4);
  info[10] = 1; info.writeInt32LE(88200, 15);
  await writeFile(join(reference, 'bank2/3.0.wav'), wav);
  await writeFile(join(reference, 'bank2/3.0.wav.info'), info);
  return { root, dist, reference };
}

test('serves the built UI and generated waveform/spectrum from the requested folder', async () => {
  const { root, dist, reference } = await fixture();
  const { server } = await createVisualizerServer(reference, dist, join(root, 'cache'));
  try {
    await new Promise<void>(ready => server.listen(0, '127.0.0.1', ready));
    const address = server.address() as { port: number };
    const base = `http://127.0.0.1:${address.port}`;
    expect(await (await fetch(base)).text()).toContain('visualizer');
    const script = await fetch(`${base}/assets/app.js`);
    expect(script.headers.get('content-type')).toContain('text/javascript');
    const manifest = await (await fetch(`${base}/data/library.json`)).json();
    expect(manifest.samples).toHaveLength(1);
    expect(manifest.samples[0]).toMatchObject({ bank: 1, sample: 3 });
    const wave = await (await fetch(`${base}/${manifest.samples[0].url}`)).json();
    expect(wave).toMatchObject({ bank: 1, sample: 3, duration: 1, bpm: 130 });
    expect(wave.spectrum.bands).toBe(32);
    const head = await fetch(`${base}/assets/app.js`, { method: 'HEAD' });
    expect(head.status).toBe(200); expect(await head.text()).toBe('');
    expect(Number(head.headers.get('content-length'))).toBeGreaterThan(0);
    for (const path of ['/server.mjs', '/bank2/3.0.wav', '/assets/%2e%2e%2fserver.mjs', '/data/missing.json'])
      expect((await fetch(base + path)).status).toBe(404);
    expect((await fetch(base + '/%ZZ')).status).toBe(400);
    expect((await fetch(base, { method: 'POST' })).status).toBe(405);
  } finally {
    await new Promise<void>((done, reject) => server.close(error => error ? reject(error) : done()));
    await rm(root, { recursive: true, force: true });
  }
});

test('rejects a missing reference path and supports an empty reference folder', async () => {
  const { root, dist } = await fixture();
  try {
    await expect(createVisualizerServer(join(root, 'missing'), dist)).rejects.toThrow();
    await expect(createVisualizerServer(join(dist, 'index.html'), dist)).rejects.toThrow('not a directory');
    const empty = join(root, 'empty'); await mkdir(empty);
    const { manifest } = await createVisualizerServer(empty, dist, join(root, 'cache'));
    expect(manifest.samples).toEqual([]);
  } finally { await rm(root, { recursive: true, force: true }); }
});


test('persists prepared samples across server instances and invalidates changed or damaged data', async () => {
  const { root, dist, reference } = await fixture();
  const cacheRoot = join(root, 'cache');
  const start = () => createVisualizerServer(reference, dist, cacheRoot);
  try {
    expect(await start()).toMatchObject({ prepared: 1, reused: 0 });
    expect(await start()).toMatchObject({ prepared: 0, reused: 1 });
    const infoPath = join(reference, 'bank2/3.0.wav.info');
    const info = await readFile(infoPath);
    info.writeUInt32LE(140 | (1 << 13) | (1 << 16), 4);
    await writeFile(infoPath, info);
    expect(await start()).toMatchObject({ prepared: 1, reused: 0 });
    expect(await start()).toMatchObject({ prepared: 0, reused: 1 });
    const audioPath = join(reference, 'bank2/3.0.wav');
    const audio = await readFile(audioPath); audio.writeInt16LE(1234, 88244);
    await writeFile(audioPath, audio);
    expect(await start()).toMatchObject({ prepared: 1, reused: 0 });
    const cacheFile = join(cacheRoot, (await readdir(cacheRoot))[0]);
    await writeFile(cacheFile, 'broken JSON');
    expect(await start()).toMatchObject({ prepared: 1, reused: 0 });
    const saved = JSON.parse(await readFile(cacheFile, 'utf8'));
    saved.version = -1; await writeFile(cacheFile, JSON.stringify(saved));
    expect(await start()).toMatchObject({ prepared: 1, reused: 0 });
    await rm(audioPath);
    const removed = await start();
    expect(removed.manifest.samples).toEqual([]);
    expect(removed).toMatchObject({ prepared: 0, reused: 0 });
  } finally { await rm(root, { recursive: true, force: true }); }
});


test('reports preparation progress, cached samples and failures with totals', async () => {
  const { root, dist, reference } = await fixture();
  const events: PreparationProgress[] = [];
  const start = () => createVisualizerServer(reference, dist, join(root, 'cache'), event => events.push(event));
  try {
    await start();
    expect(events).toEqual([
      { completed: 0, total: 1, path: 'bank2/3.0.wav', status: 'preparing' },
      { completed: 1, total: 1, path: 'bank2/3.0.wav', status: 'prepared' },
    ]);
    events.length = 0;
    await start();
    expect(events).toEqual([{ completed: 1, total: 1, path: 'bank2/3.0.wav', status: 'cached' }]);
    events.length = 0;
    await rm(join(reference, 'bank2/3.0.wav.info'));
    await start();
    expect(events).toEqual([{ completed: 1, total: 1, path: 'bank2/3.0.wav', status: 'failed' }]);
  } finally { await rm(root, { recursive: true, force: true }); }
});
