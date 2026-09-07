(() => {
  'use strict';

  if (window.JWPLCHMIPixelCodegenGuard) return;

  const ENUM_PATTERN = /(?:\r?\n)*enum\s+HMIPixelMapId\s*:\s*uint8_t\s*\r?\n?\{[\s\S]*?\r?\n?\};(?:\r?\n)*/g;
  const MARKER = '// PixelMaps estáticos RGB565 · JWPLC HMI Designer';
  const codeOutput = document.getElementById('codeOutput');

  let applying = false;
  let buildWrapped = false;

  function canonicalEnum() {
    return String(window.JWPLCHMIPixelStability?.pixelMapEnumBlock?.() || '').trim();
  }

  function stripEnums(text) {
    return String(text || '').replace(ENUM_PATTERN, '\n');
  }

  function insertCanonicalEnum(text, enumBlock) {
    if (!enumBlock) return text;

    const markerIndex = text.indexOf(MARKER);
    if (markerIndex >= 0) {
      const before = text.slice(0, markerIndex).trimEnd();
      const after = text.slice(markerIndex).trimStart();
      return `${before}\n\n${enumBlock}\n\n${after}`;
    }

    const setupIndex = text.indexOf('void jwplcHMISetup()');
    if (setupIndex >= 0) {
      const before = text.slice(0, setupIndex).trimEnd();
      const after = text.slice(setupIndex).trimStart();
      return `${before}\n\n${enumBlock}\n\n${after}`;
    }

    return `${enumBlock}\n\n${text.trimStart()}`;
  }

  function normalizeText(text) {
    const source = String(text || '');
    if (!source.includes(MARKER) && !source.includes('HMIPixelMapId')) return source;

    // La capa stability es la única fuente canónica de IDs. Si todavía no está
    // lista, no tocamos el texto para evitar eliminar un enum válido durante boot.
    const enumBlock = canonicalEnum();
    if (!enumBlock) return source;

    const stripped = stripEnums(source)
      .replace(/\n{3,}/g, '\n\n')
      .trimEnd();

    return insertCanonicalEnum(stripped, enumBlock).trimEnd();
  }

  function normalizeGenerated(result) {
    if (!result || typeof result !== 'object' || typeof result.block !== 'string') return result;
    const normalized = normalizeText(result.block);
    return normalized === result.block ? result : { ...result, block: normalized };
  }

  function wrapBuildCode() {
    const pm = window.JWPLCHMIPixelMaps;
    if (!pm?.buildCode || buildWrapped || pm.__a11EnumGuardWrapped) return Boolean(pm?.__a11EnumGuardWrapped);

    const original = pm.buildCode.bind(pm);
    pm.buildCode = () => normalizeGenerated(original());
    pm.__a11EnumGuardWrapped = true;
    buildWrapped = true;
    return true;
  }

  function applyToOutput() {
    if (applying || !codeOutput) return false;
    const current = codeOutput.textContent || '';
    const normalized = normalizeText(current);
    if (normalized === current) return false;

    applying = true;
    codeOutput.textContent = normalized;
    applying = false;
    return true;
  }

  const observer = codeOutput ? new MutationObserver(() => {
    if (applying) return;
    queueMicrotask(applyToOutput);
  }) : null;

  observer?.observe(codeOutput, {
    childList: true,
    subtree: true,
    characterData: true
  });

  const wrapTimer = setInterval(() => {
    if (!window.JWPLCHMIPixelMaps ||
        !window.JWPLCHMIPixelOptimizer ||
        !window.JWPLCHMIPixelStability?.pixelMapEnumBlock) return;
    wrapBuildCode();
    applyToOutput();
    clearInterval(wrapTimer);
  }, 25);
  setTimeout(() => clearInterval(wrapTimer), 5000);

  ['generateButton', 'contractTab', 'updateHmiButton'].forEach((id) => {
    document.getElementById(id)?.addEventListener('click', () => {
      queueMicrotask(applyToOutput);
      setTimeout(applyToOutput, 0);
      setTimeout(applyToOutput, 120);
    });
  });

  window.addEventListener('jwplc:pixelmap-changed', () => setTimeout(applyToOutput, 0));
  window.addEventListener('jwplc:editor-refresh', () => setTimeout(applyToOutput, 0));
  window.addEventListener('jwplc:project-loaded', () => setTimeout(applyToOutput, 0));

  window.JWPLCHMIPixelCodegenGuard = {
    normalize: normalizeText,
    normalizeGenerated,
    apply: applyToOutput,
    wrap: wrapBuildCode
  };
})();
