import './styles.css';
import { StatusPage } from './ui/statusPage';

const root = document.getElementById('app');
if (root) {
  const page = new StatusPage();
  root.appendChild(page.el);
}

// Register the service worker (PWA / installable). Optional — failure is non-fatal.
if ('serviceWorker' in navigator) {
  window.addEventListener('load', () => {
    navigator.serviceWorker
      .register(`${import.meta.env.BASE_URL}sw.js`)
      .catch((err) => console.warn('SW registration failed', err));
  });
}
