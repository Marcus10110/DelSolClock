// Static server for the built app (webapp/dist) on :3000, for tunnel testing.
// Usage: npm run build && npm run serve, then in another terminal:
//   ssh -R 80:localhost:3000 nokey@localhost.run
// (localhost.run gave a working HTTPS tunnel; localtunnel returned 502s.)
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(__dirname, '..', 'dist');
const PORT = 3000;
const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.webmanifest': 'application/manifest+json; charset=utf-8',
  '.png': 'image/png',
  '.svg': 'image/svg+xml',
  '.ico': 'image/x-icon',
};

if (!fs.existsSync(ROOT)) {
  console.error('dist/ not found — run `npm run build` first.');
  process.exit(1);
}

http
  .createServer((req, res) => {
    let urlPath = decodeURIComponent((req.url || '/').split('?')[0]);
    if (urlPath === '/') urlPath = '/index.html';
    let filePath = path.join(ROOT, urlPath);
    if (!filePath.startsWith(ROOT)) {
      res.writeHead(403).end('Forbidden');
      return;
    }
    fs.readFile(filePath, (err, data) => {
      if (err) {
        // SPA fallback to index.html
        fs.readFile(path.join(ROOT, 'index.html'), (e2, idx) => {
          if (e2) {
            res.writeHead(404, { 'content-type': 'text/plain' }).end('Not found');
            console.log('404', urlPath);
          } else {
            res.writeHead(200, { 'content-type': TYPES['.html'] });
            res.end(idx);
            console.log('200 (spa)', urlPath);
          }
        });
        return;
      }
      const ext = path.extname(filePath).toLowerCase();
      res.writeHead(200, { 'content-type': TYPES[ext] || 'application/octet-stream' });
      res.end(data);
      console.log('200', urlPath, data.length + 'b');
    });
  })
  .listen(PORT, () => console.log(`serving dist/ on http://localhost:${PORT}`));
