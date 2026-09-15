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

dom.window.HTMLCanvasElement.prototype.getContext = function () {
  return {
    fillRect: () => {}, clearRect: () => {},
    getImageData: () => ({ data: new Uint8ClampedArray(100) }),
    putImageData: () => {}, createImageData: () => ({ data: new Uint8ClampedArray(100) }),
    setTransform: () => {}, drawImage: () => {},
    save: () => {}, restore: () => {},
    beginPath: () => {}, moveTo: () => {}, lineTo: () => {}, stroke: () => {}, fill: () => {},
    measureText: () => ({ width: 0 }), fillText: () => {}, strokeRect: () => {}, arc: () => {}, setLineDash: () => {},
  };
};

function inject(file) {
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

setTimeout(() => {
  const btn = dom.window.document.querySelector('.tool[data-tool="valueField"]');
  if (btn) {
    console.log("CLICKING VALUE BTN");
    btn.click();
    console.log("IS ACTIVE?", btn.classList.contains('active'));
    
    // Output selected object list
    const objectList = dom.window.document.getElementById('textObjectItem')?.parentElement || dom.window.document.querySelector('.object-list');
    console.log("OBJECT LIST:", objectList.innerHTML);
  } else {
    console.log("VALUE BTN NOT FOUND");
  }
}, 500);
