(() => {
  'use strict';

  if (window.JWPLCHMIPixelOptimizer) return;

  const MAX_COLORS = 16;
  const VALUES_PER_LINE = 8;
  const LEGACY_RUN_BYTES = 8;
  const LEGACY_MAP_BYTES = 12;
  const PACKED_SPAN_BYTES = 4;
  const PACKED_MAP_BYTES = 20;

  const waitForPixelCodegen = () => new Promise((resolve) => {
    const started = performance.now();
    const timer = setInterval(() => {
      const pm = window.JWPLCHMIPixelMaps;
      if (pm?.buildCode && (pm.__a11RuntimeIdsWrapped || window.JWPLCHMIPixelStability)) {
        clearInterval(timer);
        resolve(pm);
        return;
      }
      if (performance.now() - started > 5000) {
        clearInterval(timer);
        resolve(pm || null);
      }
    }, 25);
  });

  const hex565 = (value) => `0x${(Number(value) & 0xFFFF).toString(16).toUpperCase().padStart(4, '0')}`;
  const hex32 = (value) => `0x${(Number(value) >>> 0).toString(16).toUpperCase().padStart(8, '0')}u`;

  function cppToken(value, fallback) {
    let token = String(value || '')
      .normalize('NFD')
      .replace(/[\u0300-\u036f]/g, '')
      .trim()
      .replace(/[^A-Za-z0-9_]+/g, '_')
      .replace(/^_+|_+$/g, '')
      .toUpperCase() || fallback;
    if (!token.startsWith('PIXEL_')) token = `PIXEL_${token}`;
    if (!/^[A-Z_]/.test(token)) token = `PIXEL_${token}`;
    return token;
  }

  function absolutePixels(map) {
    const byCoordinate = new Map();
    (map?.pixels || []).forEach((pixel) => {
      const x = Number(map.x || 0) + Number(pixel.x || 0);
      const y = Number(map.y || 0) + Number(pixel.y || 0);
      if (x < 0 || x > 319 || y < 0 || y > 169) return;
      byCoordinate.set(`${x},${y}`, { x, y, color: Number(pixel.color) & 0xFFFF });
    });
    return [...byCoordinate.values()];
  }

  function legacyRuns(map) {
    const pixels = absolutePixels(map)
      .sort((a, b) => a.y - b.y || a.x - b.x || a.color - b.color);
    const runs = [];
    pixels.forEach((pixel) => {
      const last = runs[runs.length - 1];
      if (last && last.y === pixel.y && last.color === pixel.color && last.x + last.length === pixel.x) {
        last.length += 1;
      } else {
        runs.push({ x: pixel.x, y: pixel.y, length: 1, color: pixel.color, vertical: false });
      }
    });
    return runs;
  }

  function horizontalSpans(points, colorIndex) {
    const sorted = points.slice().sort((a, b) => a.y - b.y || a.x - b.x);
    const spans = [];
    sorted.forEach((point) => {
      const last = spans[spans.length - 1];
      if (last && last.y === point.y && last.x + last.length === point.x) {
        last.length += 1;
      } else {
        spans.push({ x: point.x, y: point.y, length: 1, colorIndex, vertical: false });
      }
    });
    return spans;
  }

  function verticalSpans(points, colorIndex) {
    const sorted = points.slice().sort((a, b) => a.x - b.x || a.y - b.y);
    const spans = [];
    sorted.forEach((point) => {
      const last = spans[spans.length - 1];
      if (last && last.x === point.x && last.y + last.length === point.y) {
        last.length += 1;
      } else {
        spans.push({ x: point.x, y: point.y, length: 1, colorIndex, vertical: true });
      }
    });
    return spans;
  }

  function packedPlanForMap(map) {
    const pixels = absolutePixels(map);
    const palette = [...new Set(pixels.map((pixel) => pixel.color))].sort((a, b) => a - b);
    const legacy = legacyRuns(map);

    if (!pixels.length) {
      return {
        eligible: true,
        pixels,
        palette,
        spans: [],
        legacy,
        legacyBytes: LEGACY_MAP_BYTES,
        packedBytes: PACKED_MAP_BYTES
      };
    }

    if (palette.length > MAX_COLORS) {
      return {
        eligible: false,
        reason: `paleta ${palette.length} > ${MAX_COLORS}`,
        pixels,
        palette,
        spans: [],
        legacy,
        legacyBytes: legacy.length * LEGACY_RUN_BYTES + LEGACY_MAP_BYTES,
        packedBytes: Infinity
      };
    }

    const spans = [];
    palette.forEach((color, colorIndex) => {
      const points = pixels.filter((pixel) => pixel.color === color);
      const horizontal = horizontalSpans(points, colorIndex);
      const vertical = verticalSpans(points, colorIndex);
      spans.push(...(vertical.length < horizontal.length ? vertical : horizontal));
    });

    spans.sort((a, b) => a.y - b.y || a.x - b.x || Number(a.vertical) - Number(b.vertical) || a.colorIndex - b.colorIndex);

    return {
      eligible: true,
      pixels,
      palette,
      spans,
      legacy,
      legacyBytes: legacy.length * LEGACY_RUN_BYTES + LEGACY_MAP_BYTES,
      packedBytes: spans.length * PACKED_SPAN_BYTES + palette.length * 2 + PACKED_MAP_BYTES
    };
  }

  function analyzeProject(pm = window.JWPLCHMIPixelMaps) {
    const maps = (pm?.getAll?.() || []).filter((map) => Array.isArray(map.pixels) && map.pixels.length);
    const plans = maps.map((map) => ({ map, ...packedPlanForMap(map) }));
    const eligible = plans.every((plan) => plan.eligible);
    const legacyBytes = plans.reduce((sum, plan) => sum + plan.legacyBytes, 0);
    const packedBytes = eligible ? plans.reduce((sum, plan) => sum + plan.packedBytes, 0) : Infinity;
    const usePacked = Boolean(plans.length && eligible && packedBytes < legacyBytes);
    const pixelCount = plans.reduce((sum, plan) => sum + plan.pixels.length, 0);
    const legacyRuns = plans.reduce((sum, plan) => sum + plan.legacy.length, 0);
    const packedSpans = plans.reduce((sum, plan) => sum + plan.spans.length, 0);
    const paletteEntries = plans.reduce((sum, plan) => sum + plan.palette.length, 0);
    const savings = usePacked && legacyBytes > 0 ? Math.round((1 - packedBytes / legacyBytes) * 100) : 0;

    return {
      maps,
      plans,
      eligible,
      usePacked,
      format: usePacked ? 'PACKED_SPAN16' : 'RGB565_RUN',
      pixelCount,
      legacyRuns,
      packedSpans,
      paletteEntries,
      legacyBytes,
      packedBytes,
      selectedBytes: usePacked ? packedBytes : legacyBytes,
      savings
    };
  }

  function packSpan(span) {
    const x = Number(span.x) & 0x01FF;
    const y = Number(span.y) & 0x00FF;
    const length = Math.max(1, Math.min(320, Number(span.length) || 1));
    const color = Number(span.colorIndex) & 0x0F;
    const direction = span.vertical ? 1 : 0;
    return (
      (x) |
      (y << 9) |
      ((length - 1) << 17) |
      (color << 26) |
      (direction << 30)
    ) >>> 0;
  }

  function valuesBlock(values, formatter, indent = '    ') {
    const lines = [];
    for (let i = 0; i < values.length; i += VALUES_PER_LINE) {
      lines.push(`${indent}${values.slice(i, i + VALUES_PER_LINE).map(formatter).join(', ')}`);
    }
    return lines.join(',\n');
  }

  function enumBlockForMaps(maps) {
    const used = new Set();
    const lines = maps.map((map, index) => {
      let symbol = cppToken(map.name, `PIXEL_${index + 1}`);
      if (used.has(symbol)) symbol = `${symbol}_${index + 1}`;
      used.add(symbol);
      return `    ${symbol} = ${index}`;
    });
    return lines.length ? `enum HMIPixelMapId : uint8_t\n{\n${lines.join(',\n')}\n};` : '';
  }

  function existingEnumBlock(block, maps) {
    const match = String(block || '').match(/enum HMIPixelMapId : uint8_t\n\{[\s\S]*?\n\};/);
    return match?.[0] || enumBlockForMaps(maps);
  }

  function packedCode(analysis, originalGenerated) {
    const sections = [];
    const enumBlock = existingEnumBlock(originalGenerated?.block, analysis.maps);
    if (enumBlock) sections.push(enumBlock);

    sections.push(
      `// PixelMaps compactos RGB565 · PACKED_SPAN16 · JWPLC HMI Designer\n` +
      `// ${analysis.pixelCount} px -> ${analysis.packedSpans} spans · ${analysis.paletteEntries} entradas de paleta\n` +
      `// Estimado: ${analysis.packedBytes} B vs ${analysis.legacyBytes} B RGB565_RUN · ahorro ${analysis.savings}%`);

    analysis.plans.forEach((plan, index) => {
      const id = index + 1;
      const paletteName = `HMI_PM_${id}_PAL`;
      const dataName = `HMI_PM_${id}_DATA`;
      sections.push(
        `static const uint16_t ${paletteName}[] =\n{\n` +
        `${valuesBlock(plan.palette, (value) => hex565(value))}\n` +
        `};\n\n` +
        `static const uint32_t ${dataName}[] =\n{\n` +
        `${valuesBlock(plan.spans.map(packSpan), (value) => hex32(value))}\n` +
        `};`);
    });

    const entries = analysis.plans.map((plan, index) => {
      const id = index + 1;
      const paletteName = `HMI_PM_${id}_PAL`;
      const dataName = `HMI_PM_${id}_DATA`;
      return `    JWPLC_UIPixelPackedMap(${Number(plan.map.page || 0)}, ${paletteName}, sizeof(${paletteName}) / sizeof(${paletteName}[0]), ${dataName}, sizeof(${dataName}) / sizeof(${dataName}[0]))`;
    });

    sections.push(
      `static const JWPLC_UIPixelPackedMap HMI_PIXEL_MAPS[] =\n{\n` +
      `${entries.join(',\n')}\n` +
      `};`);

    return {
      block: sections.join('\n\n'),
      registration: `    JWPLC_Display.setPackedPixelMaps(HMI_PIXEL_MAPS, sizeof(HMI_PIXEL_MAPS) / sizeof(HMI_PIXEL_MAPS[0]));`,
      format: 'PACKED_SPAN16',
      stats: analysis
    };
  }

  function wrapBuildCode(pm) {
    if (!pm?.buildCode || pm.__a11OptimizerWrapped) return;
    const original = pm.buildCode.bind(pm);
    pm.buildCode = () => {
      const legacyGenerated = original();
      const analysis = analyzeProject(pm);
      if (!analysis.usePacked) {
        return {
          ...legacyGenerated,
          format: 'RGB565_RUN',
          stats: analysis
        };
      }
      return packedCode(analysis, legacyGenerated);
    };
    pm.__a11OptimizerWrapped = true;
  }

  let optimizationDetails = null;
  let formatValue = null;
  let pixelValue = null;
  let colorValue = null;
  let spanValue = null;
  let byteValue = null;
  let savingValue = null;
  let optimizationNote = null;

  function injectInspector() {
    if (optimizationDetails?.isConnected) return;
    const inspector = document.getElementById('pixelMapControlsSection');
    if (!inspector) return;

    optimizationDetails = document.createElement('details');
    optimizationDetails.id = 'pixelMapOptimizationDetails';
    optimizationDetails.open = true;
    optimizationDetails.innerHTML = `
      <summary>Optimización C++</summary>
      <div class="inspector-body two-cols">
        <div class="readout"><span>Formato proyecto</span><strong id="pixelOptFormat">—</strong></div>
        <div class="readout"><span>Píxeles objeto</span><strong id="pixelOptPixels">—</strong></div>
        <div class="readout"><span>Colores objeto</span><strong id="pixelOptColors">—</strong></div>
        <div class="readout"><span>Runs / spans</span><strong id="pixelOptSpans">—</strong></div>
        <div class="readout"><span>Tamaño estimado</span><strong id="pixelOptBytes">—</strong></div>
        <div class="readout"><span>Ahorro proyecto</span><strong id="pixelOptSaving">—</strong></div>
        <p id="pixelOptNote" class="a11-pixel-note full"></p>
      </div>`;

    const objectDetails = [...inspector.querySelectorAll('details')]
      .find((node) => node.querySelector('summary')?.textContent?.trim() === 'Objeto');
    if (objectDetails) inspector.insertBefore(optimizationDetails, objectDetails);
    else inspector.appendChild(optimizationDetails);

    formatValue = document.getElementById('pixelOptFormat');
    pixelValue = document.getElementById('pixelOptPixels');
    colorValue = document.getElementById('pixelOptColors');
    spanValue = document.getElementById('pixelOptSpans');
    byteValue = document.getElementById('pixelOptBytes');
    savingValue = document.getElementById('pixelOptSaving');
    optimizationNote = document.getElementById('pixelOptNote');
  }

  function formatBytes(value) {
    if (!Number.isFinite(value)) return '—';
    if (value < 1024) return `${value} B`;
    return `${(value / 1024).toFixed(2)} KB`;
  }

  function refreshInspector(pm = window.JWPLCHMIPixelMaps) {
    injectInspector();
    if (!optimizationDetails) return;

    const selected = pm?.getSelected?.() || null;
    const analysis = analyzeProject(pm);
    const plan = selected ? analysis.plans.find((item) => item.map === selected) : null;
    optimizationDetails.hidden = !selected;
    if (!selected || !plan) return;

    formatValue.textContent = analysis.format;
    pixelValue.textContent = String(plan.pixels.length);
    colorValue.textContent = `${plan.palette.length}${plan.palette.length > MAX_COLORS ? ' > 16' : ''}`;
    spanValue.textContent = analysis.usePacked
      ? `${plan.legacy.length} -> ${plan.spans.length}`
      : String(plan.legacy.length);
    byteValue.textContent = formatBytes(analysis.usePacked ? plan.packedBytes : plan.legacyBytes);
    savingValue.textContent = analysis.usePacked ? `${analysis.savings}%` : '0%';

    if (!analysis.eligible) {
      const blocked = analysis.plans.find((item) => !item.eligible);
      optimizationNote.textContent = `Fallback RGB565_RUN: ${blocked?.reason || 'el proyecto no es elegible para PACKED_SPAN16'}.`;
    } else if (analysis.usePacked) {
      optimizationNote.textContent = `Selección automática H/V por color. El header agrupa ${VALUES_PER_LINE} spans por línea; la imagen permanece pixel-perfect.`;
    } else {
      optimizationNote.textContent = 'RGB565_RUN ocupa menos o igual para este proyecto; el Designer conserva el formato compatible.';
    }
  }

  function patchGate() {
    const pageGate = document.querySelector('.page-tabs .gate');
    const bottomSummary = document.querySelector('.bottom-summary');
    const statusGate = [...document.querySelectorAll('.statusbar span')]
      .find((node) => node.textContent.trim().startsWith('Gate:'));
    if (pageGate) pageGate.textContent = 'Gate: A11-7D · PIXEL codegen optimizer';
    if (bottomSummary) bottomSummary.textContent = 'A11-7D · PACKED_SPAN16 + fallback RGB565_RUN';
    if (statusGate) statusGate.textContent = 'Gate: A11-7D PIXEL OPTIMIZER';
  }

  async function init() {
    const pm = await waitForPixelCodegen();
    if (!pm) return;
    wrapBuildCode(pm);
    injectInspector();
    refreshInspector(pm);
    patchGate();

    window.addEventListener('jwplc:pixelmap-changed', () => refreshInspector(pm));
    window.addEventListener('jwplc:editor-refresh', () => refreshInspector(pm));
    window.addEventListener('jwplc:project-loaded', () => refreshInspector(pm));
    document.querySelector('.left-panel')?.addEventListener('click', () => setTimeout(() => refreshInspector(pm), 0));
  }

  window.JWPLCHMIPixelOptimizer = {
    analyze: () => analyzeProject(window.JWPLCHMIPixelMaps),
    refresh: () => refreshInspector(window.JWPLCHMIPixelMaps)
  };

  init().catch((error) => console.error('[JWPLC HMI Pixel optimizer]', error));
})();
