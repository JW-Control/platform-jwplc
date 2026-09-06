'use strict';

const CACHE_NAME = 'jwplc-hmi-designer-alpha11-a11-6-v1';
const CORE = [
  './desktop.html',
  './index.html',
  './styles.css',
  './gfx-classic-font.js',
  './app.js',
  './ux-foundation.js',
  './designer-codegen.js',
  './designer-bool.js',
  './designer-bar.js',
  './designer-field-visibility.js',
  './designer-pages.js',
  './designer-live.js',
  './designer-project.js',
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
