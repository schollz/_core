import { parseArgs } from 'node:util';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createVisualizerServer } from './server';

async function main() {
  const { values, positionals } = parseArgs({ options: {
    port: { type: 'string', default: '4173' }, help: { type: 'boolean', short: 'h' },
  }, allowPositionals: true });
  if (values.help) {
    console.log('Usage: node dist/server.mjs [reference-folder] [--port 4173]\n\nThe folder must contain bank1, bank2, ... with WAV and .info files.\nDefaults to the reference folder beside dist. Restart after changing reference files.');
    return;
  }
  if (positionals.length > 1) throw new Error('Expected one reference folder. Use --help for usage.');
  if (!/^\d+$/.test(values.port!) || Number(values.port) > 65535) throw new Error('--port must be an integer from 0 to 65535.');
  const distRoot = fileURLToPath(new URL('.', import.meta.url));
  const referenceRoot = positionals[0] ? resolve(positionals[0]) : fileURLToPath(new URL('../reference/', import.meta.url));
  console.log(`Loading reference data from ${referenceRoot} …`);
  const { server, manifest, reused, prepared } = await createVisualizerServer(referenceRoot, distRoot);
  console.log(`${reused} samples reused from cache; ${prepared} newly prepared.`);
  const failed = manifest.samples.filter(s => s.error);
  for (const sample of failed) console.warn(`${sample.path}: ${sample.error}`);
  await new Promise<void>((ready, reject) => {
    server.once('error', reject);
    server.listen(Number(values.port), '127.0.0.1', ready);
  });
  const address = server.address();
  console.log(`Visualizer: http://127.0.0.1:${typeof address === 'object' && address ? address.port : values.port}`);
  console.log(`${manifest.samples.length - failed.length} samples ready${failed.length ? `; ${failed.length} unavailable` : ''}. Press Ctrl+C to stop.`);
  const shutdown = () => { server.close(); server.closeAllConnections(); };
  process.once('SIGINT', shutdown); process.once('SIGTERM', shutdown);
}

main().catch(error => {
  console.error(`Visualizer server: ${error instanceof Error ? error.message : String(error)}`);
  process.exitCode = 1;
});
