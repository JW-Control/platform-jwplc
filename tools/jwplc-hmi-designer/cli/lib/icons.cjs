// Catálogo de iconos del Designer en node (CLI): los mismos archivos que carga
// la app (poc/designer-icon-catalog.js y poc/designer-icons-core.js) más los
// iconos que un .jwhmi trae copiados en `iconAssets` (claves `id@tamaño`).
'use strict';

const catalog = require('../../poc/designer-icon-catalog.js');
const core = require('../../poc/designer-icons-core.js');

const isObject = (value) => value !== null && typeof value === 'object' && !Array.isArray(value);

// { id, name, category, stateNames, sizes: [..] } o null.
function iconInfo(id, iconAssets) {
  const icon = core.findIcon(catalog, id);
  if (icon) return { id, name: icon.name, category: icon.category, stateNames: icon.stateNames.slice(), sizes: core.sizesOf(icon) };
  if (!isObject(iconAssets)) return null;
  const copies = Object.entries(iconAssets).filter(([key, asset]) => key.startsWith(`${id}@`) && isObject(asset?.entry));
  if (!copies.length) return null;
  const [, first] = copies[0];
  const states = first.entry.states?.length || 0;
  return {
    id,
    name: first.name || id,
    category: first.category || '',
    stateNames: Array.isArray(first.stateNames) ? first.stateNames.slice() : Array.from({ length: states }, (_, i) => `Estado ${i}`),
    sizes: copies.map(([, a]) => Number(a.entry.width)).sort((a, b) => a - b)
  };
}

function resolveSize(info, size) {
  return core.resolveSize({ sizes: Object.fromEntries(info.sizes.map((s) => [s, {}])) }, size);
}

function listIcons() {
  return catalog.icons.map((icon) => ({ id: icon.id, name: icon.name, category: icon.category, stateNames: icon.stateNames.slice(), sizes: core.sizesOf(icon) }));
}

module.exports = { catalog, core, iconInfo, resolveSize, listIcons };
