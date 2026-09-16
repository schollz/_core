import { defineConfig, type Plugin } from 'vite';
import react from '@vitejs/plugin-react';
import { fileURLToPath } from 'node:url';
import { buildLibrary } from './build/library';

function referenceLibrary(): Plugin {
  const root = fileURLToPath(new URL('./reference', import.meta.url));
  const cache = new Map<string, { stamp: string; text: string }>();
  let library: Awaited<ReturnType<typeof buildLibrary>>;
  return {
    name: 'zeptocore-reference',
    async buildStart() { library = await buildLibrary(root, cache); },
    generateBundle() {
      for (const [fileName, source] of library.assets) this.emitFile({ type: 'asset', fileName, source });
    },
    configureServer(server) {
      let timer: ReturnType<typeof setTimeout>;
      let refreshing = Promise.resolve();
      const onChange = (_event: string, path: string) => {
        if (path !== root && !path.startsWith(`${root}/`)) return;
        clearTimeout(timer);
        timer = setTimeout(() => {
          refreshing = refreshing.then(async () => {
            library = await buildLibrary(root, cache);
            server.ws.send({ type: 'custom', event: 'reference-changed', data: {} });
          }).catch(error => server.config.logger.error(String(error)));
        }, 250);
      };
      server.watcher.add(root);
      server.watcher.on('all', onChange);
      server.httpServer?.once('close', () => { clearTimeout(timer); server.watcher.off('all', onChange); });
      server.middlewares.use((req, res, next) => {
        const path = (req.url ?? '').split('?')[0].replace(/^\//, '');
        if (!path.startsWith('data/')) return next();
        const asset = library?.assets.get(path);
        res.setHeader('Content-Type', 'application/json');
        res.setHeader('Cache-Control', 'no-store');
        res.statusCode = asset === undefined ? 404 : 200;
        res.end(asset ?? JSON.stringify({ error: 'Waveform unavailable; refresh the library.' }));
      });
    },
  };
}

export default defineConfig({ plugins: [react(), referenceLibrary()], base: './' });
