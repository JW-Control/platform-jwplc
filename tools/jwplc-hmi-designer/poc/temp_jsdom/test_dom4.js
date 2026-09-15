const fs = require('fs');
const { JSDOM } = require('jsdom');
const path = require('path');

const pocPath = path.resolve('..');
const html = fs.readFileSync(path.join(pocPath, 'index.html'), 'utf8');

const dom = new JSDOM(html, {
  url: "http://localhost/",
  runScripts: "dangerously"
});

dom.window.matchMedia = () => ({ matches: false, addListener: () => {}, removeListener: () => {} });
dom.window.requestAnimationFrame = (cb) => setTimeout(cb, 0);

// Add global error handler inside the DOM
dom.window.eval(`
  window.addEventListener('error', (e) => {
    console.error("DOM ERROR:", e.error ? e.error.stack : e.message);
  });
`);

// MOCK CANVAS
dom.window.HTMLCanvasElement.prototype.getContext = function () {
  return {
    fillRect: () => {},
    clearRect: () => {},
    getImageData: () => ({ data: new Uint8ClampedArray(100) }),
    putImageData: () => {},
    createImageData: () => ({ data: new Uint8ClampedArray(100) }),
    setTransform: () => {},
    drawImage: () => {},
    save: () => {},
    restore: () => {},
    beginPath: () => {},
    moveTo: () => {},
    lineTo: () => {},
    stroke: () => {},
    fill: () => {},
    measureText: () => ({ width: 0 }),
    fillText: () => {},
    strokeRect: () => {},
    arc: () => {},
    setLineDash: () => {},
  };
};

function inject(file) {
  console.log("INJECTING " + file);
  const code = fs.readFileSync(path.join(pocPath, file), 'utf8');
  const script = dom.window.document.createElement('script');
  script.textContent = code;
  dom.window.document.body.appendChild(script);
}

inject('gfx-classic-font.js');
inject('app.js');
inject('ux-foundation.js');
inject('designer-codegen.js');
inject('designer-pixelmap.js');
inject('designer-pixelmap-workbench.js');
inject('designer-bar.js');
inject('designer-bool.js');
inject('designer-pages.js');
inject('designer-live.js');

console.log("DONE ALL INJECTIONS");
