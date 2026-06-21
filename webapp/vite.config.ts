import { defineConfig, type Plugin } from 'vite';

// Content-Security-Policy for the production build. Injected as a <meta> tag only
// on `vite build` (dev mode needs inline/eval for HMR, which this policy forbids).
// The same policy is also sent as an HTTP header by Render (see render.yaml);
// the header is authoritative, the meta tag is a fallback for any host.
const CSP = [
  "default-src 'self'",
  "script-src 'self'",
  // Vite inlines small styles; 'unsafe-inline' for styles only is low-risk.
  "style-src 'self' 'unsafe-inline'",
  // img-src includes api.mapbox.com for the static route-preview image.
  "img-src 'self' data: https://api.mapbox.com",
  "font-src 'self'",
  "manifest-src 'self'",
  "worker-src 'self'",
  // fetch targets: GitHub API (release list) + asset-download proxy + Mapbox.
  // Asset binaries are fetched via the proxy (GitHub's CDN has no CORS); keep
  // this in sync with render.yaml's connect-src.
  "connect-src 'self' https://api.github.com https://delsolapi.markgarrison.io https://api.mapbox.com",
  "base-uri 'self'",
  "form-action 'self'",
  "frame-ancestors 'none'",
  'upgrade-insecure-requests',
].join('; ');

function cspMetaPlugin(): Plugin {
  return {
    name: 'inject-csp-meta',
    apply: 'build',
    transformIndexHtml(html) {
      return html.replace(
        '</title>',
        `</title>\n    <meta http-equiv="Content-Security-Policy" content="${CSP}" />`,
      );
    },
  };
}

// Served from the root of a custom domain (delsol.markgarrison.io) on Render,
// so the base path is '/'.
export default defineConfig({
  plugins: [cspMetaPlugin()],
  base: '/',
  server: {
    host: true, // listen on all interfaces so a tunnel / phone can reach it
    port: 3000,
  },
  preview: {
    host: true,
    port: 3000,
  },
  build: {
    target: 'es2021',
    outDir: 'dist',
  },
});
