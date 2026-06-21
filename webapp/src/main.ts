import './styles.css';
import { StatusPage } from './ui/statusPage';
import { warmupApi } from './firmware/firmwareBrowser';

const root = document.getElementById('app');
if (root) {
  const page = new StatusPage();
  root.appendChild(page.el);
}

// Warm up the backend API (free-tier, cold-starts after idle) so it's awake by
// the time the user clicks Update. Fire-and-forget; failure is harmless.
warmupApi();

// Register the service worker (PWA / installable). Optional — failure is non-fatal.
if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker
      .register(`${import.meta.env.BASE_URL}sw.js`)
      .catch((err) => console.warn('SW registration failed', err));
  });
}
