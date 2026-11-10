'use strict';

const assert = require('assert');
const fs = require('fs');
const path = require('path');

// Mini-DOM mock for Node.js testing of event propagation and querySelectorAll
class ClassList {
  constructor(el) {
    this.el = el;
    this.classes = new Set();
  }
  add(cls) { this.classes.add(cls); }
  remove(cls) { this.classes.delete(cls); }
  toggle(cls, force) {
    if (force === undefined) {
      if (this.classes.has(cls)) this.classes.delete(cls);
      else this.classes.add(cls);
    } else if (force) {
      this.classes.add(cls);
    } else {
      this.classes.delete(cls);
    }
  }
  contains(cls) { return this.classes.has(cls); }
}

class MockElement {
  constructor(tag, id = '', className = '') {
    this.tagName = tag.toUpperCase();
    this.id = id;
    this.className = className;
    this.classList = new ClassList(this);
    if (className) {
      className.split(/\s+/).filter(Boolean).forEach((c) => this.classList.add(c));
    }
    this.style = {};
    this.children = [];
    this.parentNode = null;
    this.listeners = {};
  }
  appendChild(child) {
    child.parentNode = this;
    this.children.push(child);
    return child;
  }
  contains(other) {
    if (other === this) return true;
    for (const child of this.children) {
      if (child.contains(other)) return true;
    }
    return false;
  }
  addEventListener(type, fn, opts = {}) {
    if (!this.listeners[type]) this.listeners[type] = [];
    this.listeners[type].push({ fn, capture: Boolean(opts.capture) });
  }
  querySelector(sel) {
    return this.querySelectorAll(sel)[0] || null;
  }
  querySelectorAll(sel) {
    const results = [];
    const parts = sel.split(',').map((s) => s.trim());
    const matchesPart = (el, p) => {
      // Handles simple selectors like '#id', '.class', '.class.sub'
      const tokens = p.split(/(?=[.#])/);
      for (const t of tokens) {
        if (t.startsWith('#') && el.id !== t.slice(1)) return false;
        if (t.startsWith('.') && !el.classList.contains(t.slice(1))) return false;
      }
      return true;
    };
    const walk = (el) => {
      for (const child of el.children) {
        if (parts.some((p) => matchesPart(child, p))) results.push(child);
        walk(child);
      }
    };
    walk(this);
    return results;
  }
}

class MockDocument extends MockElement {
  constructor() {
    super('DOCUMENT');
    this.body = new MockElement('BODY');
    this.appendChild(this.body);
  }
  getElementById(id) {
    const walk = (el) => {
      if (el.id === id) return el;
      for (const c of el.children) {
        const found = walk(c);
        if (found) return found;
      }
      return null;
    };
    return walk(this);
  }
}

function dispatchClick(target, doc) {
  let stopped = false;
  const event = {
    target,
    type: 'click',
    stopPropagation: () => { stopped = true; },
    composedPath: () => {
      const p = [];
      let cur = target;
      while (cur) {
        p.push(cur);
        cur = cur.parentNode;
      }
      return p;
    }
  };

  // 1. Capture phase on document
  const docListeners = doc.listeners['click'] || [];
  for (const l of docListeners.filter((l) => l.capture)) {
    if (stopped) break;
    l.fn(event);
  }

  // 2. Target listeners
  if (!stopped) {
    const targetListeners = target.listeners['click'] || [];
    for (const l of targetListeners.filter((l) => !l.capture)) {
      l.fn(event);
      if (stopped) break;
    }
  }

  // 3. Bubble phase to document
  if (!stopped) {
    for (const l of docListeners.filter((l) => !l.capture)) {
      l.fn(event);
    }
  }
}

console.log('TOOLBAR DROPDOWNS: Exclusividad mutua de menús');

// Build the DOM tree mimicking index.html + designer-icons.js
const doc = new MockDocument();

// 1. Variable dropdown
const varContainer = new MockElement('div', 'variableDropdownContainer', 'variable-dropdown-container');
const varBtn = new MockElement('button', 'variableToolBtn', 'canvas-tool variable-tool-btn');
const varMenu = new MockElement('div', 'variableDropdownMenu', 'variable-dropdown-menu');
varMenu.style.display = 'none';
varContainer.appendChild(varBtn);
varContainer.appendChild(varMenu);
doc.body.appendChild(varContainer);

// 2. Text dropdown
const textContainer = new MockElement('div', 'textDropdownContainer', 'text-dropdown-container');
const textBtn = new MockElement('button', 'textToolBtn', 'canvas-tool text-tool-btn');
const textMenu = new MockElement('div', 'textDropdownMenu', 'text-dropdown-menu');
textMenu.style.display = 'none';
textContainer.appendChild(textBtn);
textContainer.appendChild(textMenu);
doc.body.appendChild(textContainer);

// 3. Icon dropdown
const iconContainer = new MockElement('div', 'iconDropdownContainer', 'icon-dropdown-container');
const iconBtn = new MockElement('button', 'iconToolBtn', 'canvas-tool icon-tool-btn');
const iconMenu = new MockElement('div', 'iconDropdownMenu', 'text-dropdown-menu icon-dropdown-menu icons-block');
iconMenu.style.display = 'none';
iconContainer.appendChild(iconBtn);
iconContainer.appendChild(iconMenu);
doc.body.appendChild(iconContainer);

// Wire handlers exactly as in app.js and designer-icons.js
function closeAllDropdowns() {
  if (varMenu) varMenu.style.display = 'none';
  varContainer.classList.remove('open');
  if (textMenu) textMenu.style.display = 'none';
  textContainer.classList.remove('open');
  const im = doc.getElementById('iconDropdownMenu');
  if (im) im.style.display = 'none';
  doc.getElementById('iconDropdownContainer')?.classList.remove('open');
}

varBtn.addEventListener('click', (e) => {
  e.stopPropagation();
  const isOpen = varMenu.style.display !== 'none';
  closeAllDropdowns();
  if (!isOpen) {
    varMenu.style.display = 'flex';
    varContainer.classList.add('open');
  }
});

textBtn.addEventListener('click', (e) => {
  e.stopPropagation();
  const isOpen = textMenu.style.display !== 'none';
  closeAllDropdowns();
  if (!isOpen) {
    textMenu.style.display = 'flex';
    textContainer.classList.add('open');
  }
});

doc.addEventListener('click', (e) => {
  const ic = doc.getElementById('iconDropdownContainer');
  if (
    (!varContainer.contains(e.target)) &&
    (!textContainer.contains(e.target)) &&
    (!ic || !ic.contains(e.target))
  ) {
    closeAllDropdowns();
  }
});

// designer-icons.js wiring
function setIconMenuOpen(open) {
  iconMenu.style.display = open ? 'flex' : 'none';
  iconContainer.classList.toggle('open', open);
}

iconBtn.addEventListener('click', (e) => {
  e.stopPropagation();
  doc.querySelectorAll('.variable-dropdown-menu, .var-dropdown-menu, .text-dropdown-menu').forEach((m) => {
    if (m !== iconMenu) m.style.display = 'none';
  });
  doc.querySelectorAll('.variable-dropdown-container.open, .var-dropdown-container.open, .text-dropdown-container.open').forEach((c) => {
    c.classList.remove('open');
  });
  setIconMenuOpen(iconMenu.style.display === 'none');
});

doc.addEventListener('click', (e) => {
  if (!e.composedPath().includes(iconContainer)) setIconMenuOpen(false);
}, { capture: true });

// TEST CASES
// 1. Initially all closed
assert.strictEqual(varMenu.style.display, 'none');
assert.strictEqual(iconMenu.style.display, 'none');
assert.strictEqual(textMenu.style.display, 'none');
console.log('  PASS todos los menús inician cerrados');

// 2. Open variable menu
dispatchClick(varBtn, doc);
assert.strictEqual(varMenu.style.display, 'flex');
assert.strictEqual(varContainer.classList.contains('open'), true);
assert.strictEqual(iconMenu.style.display, 'none');
console.log('  PASS clic en variableToolBtn abre el menú de variables');

// 3. Click icon tool button while variable menu is open
dispatchClick(iconBtn, doc);
assert.strictEqual(varMenu.style.display, 'none', 'El menú de variables debe cerrarse');
assert.strictEqual(varContainer.classList.contains('open'), false, 'El contenedor de variables no debe tener .open');
assert.strictEqual(iconMenu.style.display, 'flex', 'El menú de iconos debe abrirse');
assert.strictEqual(iconContainer.classList.contains('open'), true, 'El contenedor de iconos debe tener .open');
console.log('  PASS clic en iconToolBtn cierra el menú de variables y abre el de iconos');

// 4. Click variable tool button while icon menu is open
dispatchClick(varBtn, doc);
assert.strictEqual(iconMenu.style.display, 'none', 'El menú de iconos debe cerrarse');
assert.strictEqual(iconContainer.classList.contains('open'), false);
assert.strictEqual(varMenu.style.display, 'flex', 'El menú de variables debe abrirse');
assert.strictEqual(varContainer.classList.contains('open'), true);
console.log('  PASS clic en variableToolBtn cierra el menú de iconos y abre el de variables');

// 5. Click text tool button while variable menu is open
dispatchClick(textBtn, doc);
assert.strictEqual(varMenu.style.display, 'none');
assert.strictEqual(textMenu.style.display, 'flex');
assert.strictEqual(iconMenu.style.display, 'none');
console.log('  PASS clic en textToolBtn cierra el menú de variables y abre el de texto');

// 6. Click icon tool button while text menu is open
dispatchClick(iconBtn, doc);
assert.strictEqual(textMenu.style.display, 'none');
assert.strictEqual(iconMenu.style.display, 'flex');
console.log('  PASS clic en iconToolBtn cierra el menú de texto y abre el de iconos');

// 7. Click outside closes all
const outsideElement = new MockElement('div', 'canvas');
doc.body.appendChild(outsideElement);
dispatchClick(outsideElement, doc);
assert.strictEqual(varMenu.style.display, 'none');
assert.strictEqual(textMenu.style.display, 'none');
assert.strictEqual(iconMenu.style.display, 'none');
assert.strictEqual(varContainer.classList.contains('open'), false);
assert.strictEqual(iconContainer.classList.contains('open'), false);
console.log('  PASS clic fuera cierra todos los menús');

console.log('\nTodas las pruebas de exclusividad mutua pasaron.');
