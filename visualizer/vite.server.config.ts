import { defineConfig } from 'vite';

// Bundle the reference analyser into a standalone Node entry point. Runtime
// dependencies are Node built-ins; the completed dist folder needs no npm install.
export default defineConfig({
  build: {
    ssr: 'server/index.ts', outDir: 'dist', emptyOutDir: false,
    rollupOptions: { output: { entryFileNames: 'server.mjs' } },
  },
});
