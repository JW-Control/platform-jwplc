(() => {
  'use strict';

  if (window.JWPLCHMIDesktopResponsive) return;

  const COMPACT_QUERY = '(max-width: 1180px)';
  const media = window.matchMedia(COMPACT_QUERY);
  let inspectorOpen = false;

  function toolbar() {
    return document.querySelector('.toolbar');
  }

  function ensureInspectorButton() {
    let button = document.getElementById('compactInspectorButton');
    if (button) return button;

    button = document.createElement('button');
    button.id = 'compactInspectorButton';
    button.type = 'button';
    button.textContent = 'Inspector';
    button.title = 'Mostrar u ocultar el Inspector en ventana compacta';
    button.addEventListener('click', () => {
      inspectorOpen = !inspectorOpen;
      syncLayout();
    });

    const fitButton = document.getElementById('fitButton');
    if (fitButton) fitButton.insertAdjacentElement('beforebegin', button);
    else toolbar()?.appendChild(button);
    return button;
  }

  function parseSketchName() {
    const apiName = window.JWPLCHMIProject?.linkedSketchName?.();
    if (apiName) return apiName;

    const text = document.getElementById('sketchLinkStatus')?.textContent || '';
    const match = text.match(/^Sketch:\s*([^·]+?)(?:\s*·|$)/);
    const name = match?.[1]?.trim();
    return name && name !== 'sin vincular' ? name : null;
  }

  function syncSketchIdentity() {
    const name = parseSketchName();
    const button = document.getElementById('linkSketchButton');

    if (button) {
      if (name) {
        button.textContent = `Sketch: ${name}`;
        button.title = `Sketch vinculado: ${name}. Clic para cambiar la vinculación.`;
      } else {
        button.textContent = 'Vincular sketch…';
        button.title = 'Seleccionar la carpeta del sketch Arduino';
      }
    }

    if (name) {
      document.title = `${name} · JWPLC HMI Designer — Alpha11`;
    } else {
      document.title = 'JWPLC HMI Designer — Alpha11';
    }
  }

  function syncLayout() {
    const compact = media.matches;
    const button = ensureInspectorButton();

    if (!compact) inspectorOpen = false;
    document.body.classList.toggle('compact-inspector-open', compact && inspectorOpen);

    if (button) {
      button.classList.toggle('active', compact && inspectorOpen);
      button.textContent = compact && inspectorOpen ? 'Cerrar inspector' : 'Inspector';
      button.setAttribute('aria-expanded', compact && inspectorOpen ? 'true' : 'false');
    }

    window.dispatchEvent(new Event('resize'));
  }

  function observeSketchStatus() {
    const node = document.getElementById('sketchLinkStatus');
    if (!node || node.dataset.a11ResponsiveObserved === '1') return;
    node.dataset.a11ResponsiveObserved = '1';
    new MutationObserver(() => syncSketchIdentity())
      .observe(node, { childList: true, characterData: true, subtree: true });
  }

  media.addEventListener?.('change', syncLayout);
  window.addEventListener('jwplc:editor-refresh', () => {
    observeSketchStatus();
    syncSketchIdentity();
  });
  window.addEventListener('jwplc:header-written', syncSketchIdentity);
  window.addEventListener('jwplc:project-loaded', syncSketchIdentity);
  window.addEventListener('resize', () => {
    if (!media.matches && inspectorOpen) {
      inspectorOpen = false;
      document.body.classList.remove('compact-inspector-open');
    }
  });

  setTimeout(() => {
    ensureInspectorButton();
    observeSketchStatus();
    syncSketchIdentity();
    syncLayout();
  }, 0);

  window.JWPLCHMIDesktopResponsive = {
    isCompact: () => media.matches,
    isInspectorOpen: () => inspectorOpen,
    setInspectorOpen: (open) => {
      inspectorOpen = Boolean(open);
      syncLayout();
    }
  };
})();
