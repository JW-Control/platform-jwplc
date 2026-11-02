(() => {
  'use strict';

  const STORAGE_KEY = 'jwplc_hmi_theme';

  function resolveInitialTheme() {
    try {
      const params = new URLSearchParams(window.location.search);
      const urlTheme = params.get('theme');
      if (urlTheme === 'light' || urlTheme === 'dark') {
        return urlTheme;
      }
    } catch (_) {}

    try {
      const saved = localStorage.getItem(STORAGE_KEY);
      if (saved === 'light' || saved === 'dark') {
        return saved;
      }
    } catch (_) {}

    return 'dark'; // Tema predeterminado: oscuro
  }

  function applyTheme(theme, save = true) {
    const isLight = theme === 'light';
    const root = document.documentElement;
    const body = document.body;

    if (root) {
      root.classList.toggle('theme-light', isLight);
    }
    if (body) {
      body.classList.toggle('theme-light', isLight);
    }

    const btn = document.getElementById('themeToggleButton');
    if (btn) {
      const sun = btn.querySelector('.theme-icon-sun');
      const moon = btn.querySelector('.theme-icon-moon');
      if (sun && moon) {
        sun.style.display = isLight ? 'none' : 'block';
        moon.style.display = isLight ? 'block' : 'none';
      }
      btn.title = isLight ? 'Cambiar a Modo Oscuro' : 'Cambiar a Modo Claro';
      btn.setAttribute('aria-label', btn.title);
    }

    if (save) {
      try {
        localStorage.setItem(STORAGE_KEY, theme);
      } catch (_) {}
    }

    try {
      window.dispatchEvent(new CustomEvent('jwplc:themechange', { detail: { theme, isLight } }));
    } catch (_) {}
  }

  function toggleTheme() {
    const current = document.documentElement.classList.contains('theme-light') ? 'light' : 'dark';
    const next = current === 'light' ? 'dark' : 'light';
    applyTheme(next, true);
    return next;
  }

  // Aplicación inmediata antes de renderizado para evitar destello
  const initialTheme = resolveInitialTheme();
  if (initialTheme === 'light') {
    document.documentElement.classList.add('theme-light');
  }

  function init() {
    applyTheme(resolveInitialTheme(), false);
    const btn = document.getElementById('themeToggleButton');
    if (btn && !btn.dataset.themeBound) {
      btn.dataset.themeBound = 'true';
      btn.addEventListener('click', () => {
        toggleTheme();
      });
    }
  }

  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', init);
  } else {
    init();
  }

  window.JWPLCHMITheme = {
    getTheme: () => (document.documentElement.classList.contains('theme-light') ? 'light' : 'dark'),
    isLight: () => document.documentElement.classList.contains('theme-light'),
    setTheme: (theme) => applyTheme(theme, true),
    toggleTheme: toggleTheme
  };
})();
