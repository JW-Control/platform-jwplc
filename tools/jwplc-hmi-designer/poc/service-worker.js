'use strict';

const CACHE_NAME = 'jwplc-hmi-designer-a16-v49';
const CORE = [
  './desktop.html',
  './index.html',
  './styles.css',
  './desktop-responsive.css',
  './desktop-responsive-compat.css',
  './designer-cp437.js',
  './gfx-classic-font.js',
  './designer-fonts-data.js',
  './designer-fonts.js',
  './app.js',
  './ux-foundation.js',
  './designer-codegen.js',
  './designer-color-editor.js',
  './designer-bool.js',
  './designer-bar.js',
  './designer-zone-indicator.js',
  './designer-idle.js',
  './designer-field-inspector.js',
  './designer-value-format.js',
  './designer-codegen-core.js',
  './designer-icon-catalog.js',
  './designer-icons-core.js',
  './designer-icons.js',
  './designer-dialog.js',
  './designer-menu.js',
  './designer-field-visibility.js',
  './designer-pixelmap-compat.js',
  './designer-pixelmap.js',
  './designer-pixelmap-stability.js',
  './designer-pixelmap-workbench.js',
  './designer-pixelmap-optimizer.js',
  './designer-pixelmap-codegen-guard.js',
  './designer-project-integration-fix.js',
  './designer-openplc-host.js',
  './designer-inspector-state.js',
  './designer-pages.js',
  './designer-flasher.js',
  './designer-port-picker.js',
  './designer-live.js',
  './esptool-bundle.js',
  './firmware/jwplc.esp32.jwplcbasic/flash_args',
  './designer-project.js',
  './desktop-responsive.js',
  './designer-sketch-guide.js',
  './designer-sketch-menu.js',
  './designer-save-flow.js',
  './manifest.webmanifest',
  './jwplc-hmi-icon.svg'
];

self.addEventListener('install', (event) => {
  event.waitUntil(caches.open(CACHE_NAME).then((cache) => cache.addAll(CORE)));
  self.skipWaiting();
});

self.addEventListener('activate', (event) => {
  event.waitUntil(
    caches.keys().then((keys) => Promise.all(
      keys.filter((key) => key !== CACHE_NAME).map((key) => caches.delete(key))
    ))
  );
  self.clients.claim();
});

self.addEventListener('fetch', (event) => {
  if (event.request.method !== 'GET') return;
  const url = new URL(event.request.url);
  if (url.origin !== self.location.origin) return;

  // Network-first durante Alpha11: un git pull debe reflejarse al reiniciar la
  // app. El cache queda como respaldo si el servidor local desaparece.
  event.respondWith(
    fetch(event.request)
      .then((response) => {
        if (response && response.ok) {
          const copy = response.clone();
          caches.open(CACHE_NAME).then((cache) => cache.put(event.request, copy));
        }
        return response;
      })
      .catch(() => caches.match(event.request).then((cached) => cached || caches.match('./desktop.html')))
  );
});
