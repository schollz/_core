import { createHash, randomUUID } from 'node:crypto';
import { mkdir, readFile, realpath, rename, rm, writeFile } from 'node:fs/promises';
import { homedir } from 'node:os';
import { join } from 'node:path';
import { buildLibrary } from '../build/library';

// Bump when waveform/spectrum generation or its output format changes.
const CACHE_VERSION = 1;

export async function loadReferenceLibrary(referenceRoot: string, cacheRoot = join(
  process.env.XDG_CACHE_HOME || join(homedir(), '.cache'), 'zeptocore-visualizer',
)) {
  const root = await realpath(referenceRoot);
  const key = createHash('sha256').update(root).digest('hex');
  const cacheFile = join(cacheRoot, `${key}.json`);
  const cache = new Map<string, { stamp: string; text: string }>();
  const digest = (text: string) => createHash('sha256').update(text).digest('hex');
  try {
    const saved = JSON.parse(await readFile(cacheFile, 'utf8'));
    if (saved.version === CACHE_VERSION && saved.root === root && Array.isArray(saved.entries)) {
      for (const entry of saved.entries) {
        if (typeof entry?.path === 'string' && typeof entry.stamp === 'string' &&
            typeof entry.text === 'string' && entry.hash === digest(entry.text)) {
          try { JSON.parse(entry.text); cache.set(entry.path, { stamp: entry.stamp, text: entry.text }); }
          catch { /* Regenerate damaged entries. */ }
        }
      }
    }
  } catch { /* Missing, outdated or unreadable caches are safe to regenerate. */ }
  const library = await buildLibrary(root, cache);
  const entries = library.manifest.samples.filter(s => !s.error).map(sample => {
    const entry = cache.get(sample.path)!;
    return { path: sample.path, ...entry, hash: digest(entry.text) };
  });
  const temporary = `${cacheFile}.${randomUUID()}.tmp`;
  try {
    await mkdir(cacheRoot, { recursive: true });
    await writeFile(temporary, JSON.stringify({ version: CACHE_VERSION, root, entries }));
    await rename(temporary, cacheFile);
  } catch (error) {
    console.warn(`Cannot save reference cache: ${error instanceof Error ? error.message : String(error)}`);
  } finally { await rm(temporary, { force: true }).catch(() => {}); }
  return library;
}
