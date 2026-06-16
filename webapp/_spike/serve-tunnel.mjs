// Single-process: serve webapp/ over HTTP on :3000 AND open a localtunnel.
// Run with sandbox disabled so the tunnel can reach the local server.
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import localtunnel from 'localtunnel';

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const PORT = 3000;

const TYPES = {
  '.html': 'text/html; charset=utf-8',
  '.js': 'text/javascript; charset=utf-8',
  '.css': 'text/css; charset=utf-8',
  '.json': 'application/json; charset=utf-8',
  '.ico': 'image/x-icon',
};

const server = http.createServer((req, res) => {
  let urlPath = decodeURIComponent((req.url || '/').split('?')[0]);
  if (urlPath === '/') urlPath = '/spike.html';
  const filePath = path.join(__dirname, urlPath);
  // prevent path traversal
  if (!filePath.startsWith(__dirname)) {
    res.writeHead(403).end('Forbidden');
    return;
  }
  fs.readFile(filePath, (err, data) => {
    if (err) {
      res.writeHead(404, { 'content-type': 'text/plain' }).end('Not found: ' + urlPath);
      console.log('404', urlPath);
      return;
    }
    const ext = path.extname(filePath).toLowerCase();
    res.writeHead(200, { 'content-type': TYPES[ext] || 'application/octet-stream' });
    res.end(data);
    console.log('200', urlPath, data.length + 'b');
  });
});

server.listen(PORT, async () => {
  console.log(`local server on http://localhost:${PORT}`);
  try {
    const tunnel = await localtunnel({ port: PORT });
    console.log('TUNNEL URL: ' + tunnel.url);
    console.log('TUNNEL URL (spike): ' + tunnel.url + '/spike.html');
    tunnel.on('close', () => console.log('tunnel closed'));
    tunnel.on('error', (e) => console.log('tunnel error: ' + e.message));
  } catch (e) {
    console.log('tunnel failed to open: ' + e.message);
  }
});
