# Deploying the Del Sol web app to Render

The app is a static Vite build hosted on Render's free tier at
**https://delsol.markgarrison.io**. HTTPS is required for Web Bluetooth; Render
provisions and renews the TLS certificate automatically.

## One-time setup

### 1. Create the static site

Either use the Blueprint (recommended) or configure manually.

**Blueprint (uses `render.yaml` at the repo root):**

1. Render Dashboard → **New** → **Blueprint**.
2. Connect the `DelSolClock` GitHub repo. Render reads `render.yaml` and creates
   the `delsol-clock` static site.
3. Approve and deploy.

**Manual (if you prefer no Blueprint):**

| Field | Value |
| --- | --- |
| Type | Static Site |
| Repository | `Marcus10110/DelSolClock` |
| Branch | `master` |
| Root Directory | `webapp` |
| Build Command | `npm install && npm run build` |
| Publish Directory | `dist` |

Then add the headers and the SPA rewrite (`/* → /index.html`) from `render.yaml`
in the dashboard manually.

### 2. Add the custom domain

1. On the static site → **Settings** → **Custom Domains** → add
   `delsol.markgarrison.io`.
2. Render shows a DNS target. For a subdomain, add a **CNAME** at your DNS
   provider for `markgarrison.io`:

   ```
   Type:  CNAME
   Name:  delsol
   Value: <the *.onrender.com hostname Render shows>
   ```

3. Wait for DNS to propagate; Render verifies the domain and issues the TLS cert
   automatically. HTTP is auto-redirected to HTTPS.

## Routine deploys

Push to `master` → Render auto-builds and deploys (`autoDeployTrigger: commit`).

## Verifying after deploy

- Visit `https://delsol.markgarrison.io` — the app loads (not a blank screen).
  If blank, check the browser console for asset 404s (a base-path issue).
- **Demo mode** works in any browser (no hardware, no GitHub calls).
- **Check for updates** lists releases (confirms `connect-src` allows the
  GitHub API past the CSP).
- On iPhone, open in **Bluefy** to use real Bluetooth; desktop Chrome/Edge also
  work. Safari/Firefox have no Web Bluetooth (the app shows a banner).

## Notes

- The CSP lives in two places that must stay in sync: the HTTP header in
  `render.yaml` (authoritative) and the `<meta>` tag injected at build time by
  `vite.config.ts`. If a future feature fetches a new host, add it to
  `connect-src` in both.
- GitHub's unauthenticated API allows 60 requests/hour per IP. "Check for
  updates" uses one request; it fails gracefully if rate-limited.
