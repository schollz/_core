import { readdir, readFile, stat } from 'node:fs/promises';
import { join } from 'node:path';
import { createHash } from 'node:crypto';
import { makeWaveform } from './waveform';
import type { Library } from '../src/types';

export type PreparationProgress = {
  completed: number; total: number; path: string; status: 'preparing' | 'prepared' | 'cached' | 'failed';
};
export type ProgressListener = (progress: PreparationProgress) => void;

export async function buildLibrary(root: string, cache = new Map<string, { stamp: string; text: string }>(), onProgress?: ProgressListener) {
  let reused = 0, prepared = 0;
  const manifest: Library = { samples: [] };
  const assets = new Map<string, string>();
  const dirs = await readdir(root, { withFileTypes: true }).catch((error: NodeJS.ErrnoException) => {
    if (error.code === 'ENOENT') return [];
    throw error;
  });
  const sources: { bank: number; sample: number; path: string }[] = [];
  for (const dir of dirs.filter(d => d.isDirectory() && /^bank([1-9]|1[0-6])$/.test(d.name))) {
    const bank = Number(dir.name.slice(4)) - 1;
    const files = await readdir(join(root, dir.name), { withFileTypes: true });
    for (const file of files.filter(f => f.isFile() && /^(\d+)\.0\.wav$/.test(f.name))) {
      const sample = Number(file.name.split('.')[0]);
      if (sample > 15) continue;
      sources.push({ bank, sample, path: `${dir.name}/${file.name}` });
    }
  }
  sources.sort((a, b) => a.bank - b.bank || a.sample - b.sample);
  for (const { bank, sample, path } of sources) {
    const absolute = join(root, path);
    let status: PreparationProgress['status'] = 'cached';
    const report = (state: PreparationProgress['status']) => onProgress?.({
      completed: manifest.samples.length, total: sources.length, path, status: state,
    });
    try {
      const [audioStat, infoStat] = await Promise.all([stat(absolute), stat(`${absolute}.info`)]);
      const stamp = `${audioStat.mtimeMs}:${audioStat.ctimeMs}:${audioStat.size}:${infoStat.mtimeMs}:${infoStat.ctimeMs}:${infoStat.size}`;
      let entry = cache.get(path);
      if (entry?.stamp !== stamp) {
        report('preparing');
        status = 'prepared';
        const [audio, info] = await Promise.all([readFile(absolute), readFile(`${absolute}.info`)]);
        entry = { stamp, text: JSON.stringify(makeWaveform(audio, info, bank, sample)) };
        cache.set(path, entry);
        prepared++;
      } else reused++;
      const hash = createHash('sha256').update(entry.text).digest('hex').slice(0, 16);
      const url = `data/${bank}-${sample}-${hash}.json`;
      assets.set(url, entry.text);
      manifest.samples.push({ bank, sample, path, url });
    } catch (error) {
      status = 'failed';
      manifest.samples.push({ bank, sample, path, error: error instanceof Error ? error.message : String(error) });
    }
    report(status);
  }
  manifest.samples.sort((a, b) => a.bank - b.bank || a.sample - b.sample);
  assets.set('data/library.json', JSON.stringify(manifest));
  return { manifest, assets, reused, prepared };
}
