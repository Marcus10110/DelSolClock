// Navigation panel: pick a destination (favorites or autocomplete search),
// compute route alternatives, preview, and send to the clock over BLE.
// Search + preview work while disconnected; Send requires a connection.

import type { IConnection } from '../ble/iconnection';
import {
  newSessionToken,
  suggest,
  retrieve,
  forward,
  type Suggestion,
} from '../navigation/search';
import { fetchRoute } from '../navigation/mapbox';
import { buildRouteSummary } from '../navigation/routePrep';
import { staticRouteImageUrl } from '../navigation/staticMap';
import { addFavorite, listFavorites } from '../navigation/favorites';
import type { LatLng, RouteSummary } from '../navigation/types';

const TOKEN = import.meta.env.VITE_MAPBOX_TOKEN as string | undefined;

interface Place {
  name: string;
  coords: LatLng;
}

interface RouteOption {
  index: number;
  distanceMeters: number;
  durationSeconds: number;
  summary: RouteSummary;
}

export class NavigationPanel {
  readonly el: HTMLElement;
  private conn: IConnection | null = null;

  private origin: LatLng | null = null;     // null => use current location
  private destination: Place | null = null;
  private session = newSessionToken();
  private suggestTimer: ReturnType<typeof setTimeout> | null = null;

  private options: RouteOption[] = [];
  private selected = 0;
  private busy = false;

  constructor() {
    this.el = document.createElement('div');
    this.el.className = 'card';
    this.render();
  }

  setConnection(conn: IConnection | null): void {
    this.conn = conn;
    this.updateSendEnabled();
  }

  // --- rendering ---

  private render(): void {
    const favs = listFavorites();
    this.el.innerHTML = `
      <h2>Navigation</h2>
      ${TOKEN ? '' : '<p class="nav-error">Set VITE_MAPBOX_TOKEN in webapp/.env to enable search.</p>'}
      <div class="nav-favs" id="nav-favs">
        ${favs
          .map(
            (f) =>
              `<button class="nav-chip" data-fav="${f.id}" title="${escapeHtml(f.name)}">${escapeHtml(f.label)}</button>`,
          )
          .join('')}
      </div>
      <div class="nav-search">
        <input type="text" id="nav-dest" class="nav-input" autocomplete="off"
               placeholder="Search destination or paste an address" />
        <div class="nav-suggest" id="nav-suggest"></div>
      </div>
      <div class="nav-origin muted-text" id="nav-origin">Start: Current location</div>
      <div id="nav-options"></div>
      <div id="nav-preview"></div>
      <div class="nav-actions">
        <button id="nav-send">Send Route to Clock</button>
        <button id="nav-savefav" class="secondary">★ Save as favorite</button>
      </div>
      <div id="nav-status"></div>
    `;
    this.wire();
    this.updateSendEnabled();
  }

  private wire(): void {
    const input = this.q<HTMLInputElement>('#nav-dest');
    input.addEventListener('input', () => this.onSearchInput(input.value));
    input.addEventListener('keydown', (e) => {
      if (e.key === 'Enter') {
        e.preventDefault();
        void this.onSubmitTyped(input.value);
      }
    });

    for (const chip of this.el.querySelectorAll<HTMLElement>('[data-fav]')) {
      chip.addEventListener('click', () =>
        void this.onFavorite(chip.dataset.fav as string),
      );
    }
    this.q<HTMLButtonElement>('#nav-send').addEventListener('click', () =>
      void this.onSend(),
    );
    this.q<HTMLButtonElement>('#nav-savefav').addEventListener('click', () =>
      this.onSaveFavorite(),
    );
  }

  private q<T extends HTMLElement>(sel: string): T {
    return this.el.querySelector(sel) as T;
  }

  // --- search / suggestions ---

  private onSearchInput(value: string): void {
    if (this.suggestTimer) clearTimeout(this.suggestTimer);
    if (!TOKEN || value.trim().length < 3) {
      this.q('#nav-suggest').innerHTML = '';
      return;
    }
    this.suggestTimer = setTimeout(() => void this.runSuggest(value.trim()), 250);
  }

  private async runSuggest(query: string): Promise<void> {
    try {
      const results = await suggest(query, this.session, TOKEN!, this.origin ?? undefined);
      this.renderSuggestions(results);
    } catch {
      this.q('#nav-suggest').innerHTML = '';
    }
  }

  private renderSuggestions(results: Suggestion[]): void {
    const box = this.q('#nav-suggest');
    if (results.length === 0) {
      box.innerHTML = '';
      return;
    }
    box.innerHTML = results
      .map(
        (s) =>
          `<div class="nav-suggest-row" data-id="${escapeHtml(s.mapboxId)}">
             <div class="nav-suggest-name">${escapeHtml(s.name)}</div>
             <div class="nav-suggest-addr muted-text">${escapeHtml(s.address)}</div>
           </div>`,
      )
      .join('');
    for (const row of box.querySelectorAll<HTMLElement>('[data-id]')) {
      row.addEventListener('click', () => void this.onPickSuggestion(row.dataset.id as string));
    }
  }

  private async onPickSuggestion(mapboxId: string): Promise<void> {
    this.q('#nav-suggest').innerHTML = '';
    try {
      const place = await retrieve(mapboxId, this.session, TOKEN!);
      this.session = newSessionToken(); // end the session after a retrieve
      if (!place) {
        this.status('Could not resolve that place.', 'error');
        return;
      }
      this.setDestination(place);
      await this.computeRoute();
    } catch (err) {
      this.status(errMsg(err), 'error');
    }
  }

  private async onSubmitTyped(value: string): Promise<void> {
    const q = value.trim();
    if (!TOKEN || q.length === 0) return;
    this.q('#nav-suggest').innerHTML = '';
    try {
      const place = await forward(q, TOKEN, this.origin ?? undefined);
      if (!place) {
        this.status('No match for that address.', 'error');
        return;
      }
      this.setDestination(place);
      await this.computeRoute();
    } catch (err) {
      this.status(errMsg(err), 'error');
    }
  }

  private setDestination(place: Place): void {
    this.destination = place;
    this.q<HTMLInputElement>('#nav-dest').value = place.name;
  }

  // --- favorites ---

  private async onFavorite(id: string): Promise<void> {
    const fav = listFavorites().find((f) => f.id === id);
    if (!fav) return;
    this.setDestination({ name: fav.name, coords: fav.coords });
    await this.computeRoute();
  }

  private onSaveFavorite(): void {
    if (!this.destination) {
      this.status('Pick a destination first.', 'error');
      return;
    }
    const label = prompt('Label for this favorite (e.g. Home, Work):');
    if (!label) return;
    addFavorite({ label, name: this.destination.name, coords: this.destination.coords });
    this.render(); // refresh chips
  }

  // --- routing ---

  private async resolveOrigin(): Promise<LatLng> {
    if (this.origin) return this.origin;
    return new Promise<LatLng>((resolve, reject) => {
      if (!('geolocation' in navigator)) {
        reject(new Error('Geolocation unavailable; set a start address.'));
        return;
      }
      navigator.geolocation.getCurrentPosition(
        (pos) => resolve({ lat: pos.coords.latitude, lng: pos.coords.longitude }),
        () => reject(new Error('Location denied; set a start address.')),
        { enableHighAccuracy: true, timeout: 10_000 },
      );
    });
  }

  private async computeRoute(): Promise<void> {
    if (!this.destination || !TOKEN) return;
    this.status('Computing route…', 'info');
    try {
      const start = await this.resolveOrigin();
      const raw = await fetchRoute(start, this.destination.coords, TOKEN, true);
      if (raw.code !== 'Ok' || raw.routes.length === 0) {
        this.status('No route found.', 'error');
        return;
      }
      this.options = raw.routes.map((r, i) => ({
        index: i,
        distanceMeters: r.distance,
        durationSeconds: r.duration,
        summary: buildRouteSummary(raw, { routeIndex: i }),
      }));
      this.selected = 0;
      this.renderOptions();
      this.renderPreview();
      this.status('', 'info');
      this.updateSendEnabled();
    } catch (err) {
      this.status(errMsg(err), 'error');
    }
  }

  private renderOptions(): void {
    const box = this.q('#nav-options');
    if (this.options.length <= 1) {
      box.innerHTML = '';
      return;
    }
    box.innerHTML =
      '<div class="nav-opts-title muted-text">Route options</div>' +
      this.options
        .map(
          (o, i) =>
            `<button class="nav-opt ${i === this.selected ? 'nav-opt-sel' : ''}" data-opt="${i}">
               ${fmtMiles(o.distanceMeters)} · ${fmtDuration(o.durationSeconds)}
             </button>`,
        )
        .join('');
    for (const b of box.querySelectorAll<HTMLElement>('[data-opt]')) {
      b.addEventListener('click', () => {
        this.selected = Number(b.dataset.opt);
        this.renderOptions();
        this.renderPreview();
      });
    }
  }

  private renderPreview(): void {
    const box = this.q('#nav-preview');
    const opt = this.options[this.selected];
    if (!opt || !TOKEN) {
      box.innerHTML = '';
      return;
    }
    const url = staticRouteImageUrl(opt.summary, TOKEN, { width: 560, height: 300 });
    box.innerHTML = `<img class="nav-preview-img" alt="Route preview" src="${escapeHtml(url)}" />`;
  }

  // --- send ---

  private updateSendEnabled(): void {
    const send = this.el.querySelector<HTMLButtonElement>('#nav-send');
    if (!send) return;
    const ready = !!this.conn?.isConnected && this.options.length > 0 && !this.busy;
    send.disabled = !ready;
    send.textContent = this.conn?.isConnected
      ? 'Send Route to Clock'
      : 'Connect to send';
  }

  private async onSend(): Promise<void> {
    const opt = this.options[this.selected];
    if (!opt || !this.conn?.isConnected || this.busy) return;
    this.busy = true;
    this.updateSendEnabled();
    this.status('Sending route…', 'info');
    try {
      await this.conn.uploadRoute(opt.summary, (p) =>
        this.status(`Sending… ${p.bytesSent}/${p.totalBytes} bytes`, 'info'),
      );
      this.status('Route sent to clock ✓', 'ok');
    } catch (err) {
      this.status(`Send failed: ${errMsg(err)}`, 'error');
    } finally {
      this.busy = false;
      this.updateSendEnabled();
    }
  }

  private status(msg: string, kind: 'info' | 'ok' | 'error'): void {
    const el = this.q('#nav-status');
    el.className = kind === 'error' ? 'nav-error' : kind === 'ok' ? 'nav-ok' : 'muted-text';
    el.textContent = msg;
  }
}

// --- helpers ---

function fmtMiles(meters: number): string {
  return `${(meters / 1609.344).toFixed(1)} mi`;
}
function fmtDuration(sec: number): string {
  const m = Math.round(sec / 60);
  if (m < 60) return `${m} min`;
  return `${Math.floor(m / 60)}h ${m % 60}m`;
}
function errMsg(err: unknown): string {
  return err instanceof Error ? err.message : String(err);
}
function escapeHtml(s: string): string {
  return s
    .replace(/&/g, '&amp;')
    .replace(/</g, '&lt;')
    .replace(/>/g, '&gt;')
    .replace(/"/g, '&quot;');
}
