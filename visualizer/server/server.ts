import { createServer } from 'node:http';
import { readFile, readdir, stat } from 'node:fs/promises';
import { extname, join } from 'node:path';
import { loadReferenceLibrary } from './cache';

const mime: Record<string, string> = {
  '.html': 'text/html; charset=utf-8', '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8', '.json': 'application/json; charset=utf-8',
  '.woff2': 'font/woff2', '.svg': 'image/svg+xml', '.png': 'image/png', '.ico': 'image/x-icon',
};

export async function createVisualizerServer(referenceRoot: string, distRoot: string, cacheRoot?: string) {
  if (!(await stat(referenceRoot)).isDirectory()) throw new Error(`Reference path is not a directory: ${referenceRoot}`);
  const files = new Map<string, { body: Buffer; type: string }>();
  const addFile = async (url: string, path: string) => {
    files.set(url, { body: await readFile(path), type: mime[extname(path)] ?? 'application/octet-stream' });
  };
  await addFile('/index.html', join(distRoot, 'index.html'));
  files.set('/', files.get('/index.html')!);
  // Only public client assets are served. The server bundle and reference WAVs
  // are never reachable through HTTP, even via encoded traversal paths.
  const addAssets = async (directory: string, prefix: string) => {
    for (const entry of await readdir(directory, { withFileTypes: true })) {
      if (entry.isDirectory()) await addAssets(join(directory, entry.name), `${prefix}/${entry.name}`);
      else if (entry.isFile()) await addFile(`${prefix}/${entry.name}`, join(directory, entry.name));
    }
  };
  await addAssets(join(distRoot, 'assets'), '/assets');
  const library = await loadReferenceLibrary(referenceRoot, cacheRoot);
  for (const [url, body] of library.assets) files.set(`/${url}`, { body: Buffer.from(body), type: mime['.json'] });
  const server = createServer((req, res) => {
    res.setHeader('Cache-Control', 'no-store');
    res.setHeader('X-Content-Type-Options', 'nosniff');
    if (req.method !== 'GET' && req.method !== 'HEAD') {
      res.writeHead(405, { Allow: 'GET, HEAD' }); res.end('Method not allowed'); return;
    }
    let pathname: string;
    try { pathname = decodeURIComponent(new URL(req.url ?? '/', 'http://localhost').pathname); }
    catch { res.writeHead(400); res.end('Invalid URL'); return; }
    const file = files.get(pathname);
    if (!file) { res.writeHead(404); res.end(req.method === 'HEAD' ? undefined : 'Not found'); return; }
    res.writeHead(200, { 'Content-Type': file.type, 'Content-Length': file.body.length });
    res.end(req.method === 'HEAD' ? undefined : file.body);
  });
  return { server, manifest: library.manifest, reused: library.reused, prepared: library.prepared };
}
