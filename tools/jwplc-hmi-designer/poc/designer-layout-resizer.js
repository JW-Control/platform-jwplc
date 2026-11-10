/**
 * designer-layout-resizer.js — Redimensionamiento interactivo de paneles y barra inferior
 * para JWPLC HMI Designer.
 *
 * Proporciona splitters accesibles con PointerEvents, soporte de teclado,
 * límites ergonómicos de seguridad, persistencia en localStorage y reseteo por doble clic.
 */
(function (root, factory) {
  'use strict';
  if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.JWPLCHMILayoutResizer = factory();
  }
})(typeof window !== 'undefined' ? window : globalThis, function () {
  'use strict';

  const STORAGE = 'jwplc-hmi-layout.';
  const KEYS = {
    left: `${STORAGE}leftPanelWidth`,
    right: `${STORAGE}rightPanelWidth`,
    bottom: `${STORAGE}bottomPanelHeight`
  };

  const LIMITS = {
    minLeft: 180,
    maxLeft: 520,
    minRight: 240,
    maxRight: 650,
    minEditor: 350,
    minBottom: 55,
    maxBottom: 650,
    defaultLeft: 280,
    defaultRight: 360,
    defaultBottom: 150
  };

  function clamp(val, min, max) {
    if (Number.isNaN(val)) return min;
    return Math.max(min, Math.min(max, val));
  }

  function getStoredNumber(key) {
    try {
      if (typeof localStorage === 'undefined') return null;
      const raw = localStorage.getItem(key);
      if (raw === null || raw === '') return null;
      const num = parseFloat(raw);
      return Number.isFinite(num) ? num : null;
    } catch (_) {
      return null;
    }
  }

  function setStoredNumber(key, value) {
    try {
      if (typeof localStorage === 'undefined') return;
      if (value === null || value === undefined) {
        localStorage.removeItem(key);
      } else {
        localStorage.setItem(key, String(Math.round(value)));
      }
    } catch (_) {}
  }

  let initialized = false;

  function getSizes() {
    if (typeof document === 'undefined') {
      return {
        left: getStoredNumber(KEYS.left) || LIMITS.defaultLeft,
        right: getStoredNumber(KEYS.right) || LIMITS.defaultRight,
        bottom: getStoredNumber(KEYS.bottom) || LIMITS.defaultBottom
      };
    }
    const style = getComputedStyle(document.documentElement);
    const parse = (varName, fallback) => {
      const val = parseFloat(style.getPropertyValue(varName));
      return Number.isFinite(val) ? val : fallback;
    };
    return {
      left: parse('--left-panel-width', getStoredNumber(KEYS.left) || LIMITS.defaultLeft),
      right: parse('--right-panel-width', getStoredNumber(KEYS.right) || LIMITS.defaultRight),
      bottom: parse('--bottom-panel-height', getStoredNumber(KEYS.bottom) || LIMITS.defaultBottom)
    };
  }

  function setSizes(sizes = {}) {
    const rootStyle = typeof document !== 'undefined' ? document.documentElement.style : null;

    if (sizes.left !== undefined) {
      if (sizes.left === null) {
        if (rootStyle) rootStyle.removeProperty('--left-panel-width');
        setStoredNumber(KEYS.left, null);
      } else {
        const val = clamp(sizes.left, LIMITS.minLeft, LIMITS.maxLeft);
        if (rootStyle) rootStyle.setProperty('--left-panel-width', `${val}px`);
        setStoredNumber(KEYS.left, val);
      }
    }

    if (sizes.right !== undefined) {
      if (sizes.right === null) {
        if (rootStyle) rootStyle.removeProperty('--right-panel-width');
        setStoredNumber(KEYS.right, null);
      } else {
        const val = clamp(sizes.right, LIMITS.minRight, LIMITS.maxRight);
        if (rootStyle) rootStyle.setProperty('--right-panel-width', `${val}px`);
        setStoredNumber(KEYS.right, val);
      }
    }

    if (sizes.bottom !== undefined) {
      if (sizes.bottom === null) {
        if (rootStyle) rootStyle.removeProperty('--bottom-panel-height');
        setStoredNumber(KEYS.bottom, null);
      } else {
        const val = clamp(sizes.bottom, LIMITS.minBottom, LIMITS.maxBottom);
        if (rootStyle) rootStyle.setProperty('--bottom-panel-height', `${val}px`);
        setStoredNumber(KEYS.bottom, val);
      }
    }

    notifyResize();
  }

  function reset(target) {
    const rootStyle = typeof document !== 'undefined' ? document.documentElement.style : null;
    if (!target || target === 'left') {
      if (rootStyle) rootStyle.removeProperty('--left-panel-width');
      setStoredNumber(KEYS.left, null);
    }
    if (!target || target === 'right') {
      if (rootStyle) rootStyle.removeProperty('--right-panel-width');
      setStoredNumber(KEYS.right, null);
    }
    if (!target || target === 'bottom') {
      if (rootStyle) rootStyle.removeProperty('--bottom-panel-height');
      setStoredNumber(KEYS.bottom, null);
    }
    notifyResize();
  }

  let resizeTimer = null;
  function notifyResize() {
    if (typeof window === 'undefined') return;
    clearTimeout(resizeTimer);
    resizeTimer = setTimeout(() => {
      window.dispatchEvent(new Event('resize'));
    }, 16);
  }

  function restoreSavedSizes() {
    if (typeof document === 'undefined') return;
    const left = getStoredNumber(KEYS.left);
    const right = getStoredNumber(KEYS.right);
    const bottom = getStoredNumber(KEYS.bottom);

    const rootStyle = document.documentElement.style;
    if (left !== null) rootStyle.setProperty('--left-panel-width', `${left}px`);
    if (right !== null) rootStyle.setProperty('--right-panel-width', `${right}px`);
    if (bottom !== null) rootStyle.setProperty('--bottom-panel-height', `${bottom}px`);
  }

  function bindResizer(element, type) {
    if (!element) return;

    let isDragging = false;
    let startCoord = 0;
    let startDimension = 0;

    element.setAttribute('role', 'separator');
    element.setAttribute('tabindex', '0');
    if (type === 'bottom') {
      element.setAttribute('aria-orientation', 'horizontal');
      element.setAttribute('aria-label', 'Redimensionar barra inferior de código (Doble clic para restablecer)');
    } else {
      element.setAttribute('aria-orientation', 'vertical');
      element.setAttribute('aria-label', type === 'left'
        ? 'Redimensionar panel izquierdo (Doble clic para restablecer)'
        : 'Redimensionar panel derecho (Doble clic para restablecer)');
    }

    function onPointerDown(e) {
      if (e.button !== 0) return; // Sólo botón primario
      e.preventDefault();
      e.stopPropagation();

      isDragging = true;
      element.classList.add('is-dragging');
      document.body.classList.add('is-resizing');
      document.body.classList.add(type === 'bottom' ? 'is-resizing-row' : 'is-resizing-col');

      try {
        element.setPointerCapture(e.pointerId);
      } catch (_) {}

      const currentSizes = getSizes();
      if (type === 'left') {
        startCoord = e.clientX;
        startDimension = currentSizes.left;
      } else if (type === 'right') {
        startCoord = e.clientX;
        startDimension = currentSizes.right;
      } else if (type === 'bottom') {
        startCoord = e.clientY;
        startDimension = currentSizes.bottom;
      }

      element.addEventListener('pointermove', onPointerMove);
      element.addEventListener('pointerup', onPointerUp);
      element.addEventListener('pointercancel', onPointerUp);
      element.addEventListener('lostpointercapture', onPointerUp);
    }

    function onPointerMove(e) {
      if (!isDragging) return;
      e.preventDefault();

      if (type === 'left') {
        const workspace = document.querySelector('.workspace');
        const workspaceRect = workspace ? workspace.getBoundingClientRect() : { left: 0, width: window.innerWidth };
        const rawNewWidth = e.clientX - workspaceRect.left;
        const currentRight = getSizes().right;
        const maxAllowed = Math.min(LIMITS.maxLeft, workspaceRect.width - currentRight - LIMITS.minEditor);
        const newWidth = clamp(rawNewWidth, LIMITS.minLeft, Math.max(LIMITS.minLeft, maxAllowed));
        document.documentElement.style.setProperty('--left-panel-width', `${newWidth}px`);
        element.setAttribute('aria-valuenow', String(Math.round(newWidth)));
      } else if (type === 'right') {
        const workspace = document.querySelector('.workspace');
        const workspaceRect = workspace ? workspace.getBoundingClientRect() : { right: window.innerWidth, width: window.innerWidth };
        const rawNewWidth = workspaceRect.right - e.clientX;
        const currentLeft = getSizes().left;
        const maxAllowed = Math.min(LIMITS.maxRight, workspaceRect.width - currentLeft - LIMITS.minEditor);
        const newWidth = clamp(rawNewWidth, LIMITS.minRight, Math.max(LIMITS.minRight, maxAllowed));
        document.documentElement.style.setProperty('--right-panel-width', `${newWidth}px`);
        element.setAttribute('aria-valuenow', String(Math.round(newWidth)));
      } else if (type === 'bottom') {
        const statusbar = document.querySelector('.statusbar');
        const statusbarTop = statusbar ? statusbar.getBoundingClientRect().top : window.innerHeight - 28;
        const rawNewHeight = statusbarTop - e.clientY;

        // Si estaba colapsado y el usuario arrastra hacia arriba, descolapsar
        if (document.body.classList.contains('bottom-collapsed') && rawNewHeight > 45) {
          document.body.classList.remove('bottom-collapsed');
          const collapseBtn = document.getElementById('collapseBottomButton');
          if (collapseBtn) collapseBtn.textContent = '⌄';
        }

        const maxAllowed = Math.min(LIMITS.maxBottom, window.innerHeight - 150);
        const newHeight = clamp(rawNewHeight, LIMITS.minBottom, Math.max(LIMITS.minBottom, maxAllowed));
        document.documentElement.style.setProperty('--bottom-panel-height', `${newHeight}px`);
        element.setAttribute('aria-valuenow', String(Math.round(newHeight)));
      }

      notifyResize();
    }

    function onPointerUp(e) {
      if (!isDragging) return;
      isDragging = false;
      element.classList.remove('is-dragging');
      document.body.classList.remove('is-resizing', 'is-resizing-col', 'is-resizing-row');

      element.removeEventListener('pointermove', onPointerMove);
      element.removeEventListener('pointerup', onPointerUp);
      element.removeEventListener('pointercancel', onPointerUp);
      element.removeEventListener('lostpointercapture', onPointerUp);

      try {
        if (e?.pointerId && element.hasPointerCapture(e.pointerId)) {
          element.releasePointerCapture(e.pointerId);
        }
      } catch (_) {}

      // Persistir valores finales
      const finalSizes = getSizes();
      if (type === 'left') setStoredNumber(KEYS.left, finalSizes.left);
      if (type === 'right') setStoredNumber(KEYS.right, finalSizes.right);
      if (type === 'bottom') setStoredNumber(KEYS.bottom, finalSizes.bottom);

      notifyResize();
    }

    function onDblClick(e) {
      e.preventDefault();
      e.stopPropagation();
      reset(type);
    }

    function onKeyDown(e) {
      const STEP = 10;
      let handled = false;
      const current = getSizes();

      if (type === 'left') {
        if (e.key === 'ArrowLeft') {
          setSizes({ left: current.left - STEP });
          handled = true;
        } else if (e.key === 'ArrowRight') {
          setSizes({ left: current.left + STEP });
          handled = true;
        } else if (e.key === 'Home' || e.key === 'Delete') {
          reset('left');
          handled = true;
        }
      } else if (type === 'right') {
        if (e.key === 'ArrowLeft') {
          setSizes({ right: current.right + STEP });
          handled = true;
        } else if (e.key === 'ArrowRight') {
          setSizes({ right: current.right - STEP });
          handled = true;
        } else if (e.key === 'Home' || e.key === 'Delete') {
          reset('right');
          handled = true;
        }
      } else if (type === 'bottom') {
        if (e.key === 'ArrowUp') {
          if (document.body.classList.contains('bottom-collapsed')) {
            document.body.classList.remove('bottom-collapsed');
            const collapseBtn = document.getElementById('collapseBottomButton');
            if (collapseBtn) collapseBtn.textContent = '⌄';
          }
          setSizes({ bottom: current.bottom + STEP });
          handled = true;
        } else if (e.key === 'ArrowDown') {
          setSizes({ bottom: current.bottom - STEP });
          handled = true;
        } else if (e.key === 'Home' || e.key === 'Delete') {
          reset('bottom');
          handled = true;
        }
      }

      if (handled) {
        e.preventDefault();
        e.stopPropagation();
      }
    }

    element.addEventListener('pointerdown', onPointerDown);
    element.addEventListener('dblclick', onDblClick);
    element.addEventListener('keydown', onKeyDown);
  }

  function init() {
    if (initialized || typeof document === 'undefined') return;
    initialized = true;

    restoreSavedSizes();

    const left = document.getElementById('resizerLeft');
    const right = document.getElementById('resizerRight');
    const bottom = document.getElementById('resizerBottom');

    if (left) bindResizer(left, 'left');
    if (right) bindResizer(right, 'right');
    if (bottom) bindResizer(bottom, 'bottom');
  }

  // Restaurar de inmediato en cuanto se evalúa el script para prevenir FOUC
  restoreSavedSizes();

  if (typeof document !== 'undefined') {
    if (document.readyState === 'loading') {
      document.addEventListener('DOMContentLoaded', init, { once: true });
    } else {
      setTimeout(init, 0);
    }
  }

  return {
    init,
    reset,
    getSizes,
    setSizes,
    clamp,
    LIMITS,
    KEYS
  };
});
