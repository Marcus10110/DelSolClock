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
  "img-src 'self' data:",
  "font-src 'self'",
  "manifest-src 'self'",
  "worker-src 'self'",
  // fetch targets: GitHub API + release asset downloads (and their redirect host)
  "connect-src 'self' https://api.github.com https://github.com https://release-assets.githubusercontent.com https://objects.githubusercontent.com",
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
