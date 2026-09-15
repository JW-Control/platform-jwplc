const fs = require('fs');
const { JSDOM } = require('jsdom');
const path = require('path');

const pocPath = path.resolve('..');
const html = fs.readFileSync(path.join(pocPath, 'index.html'), 'utf8');

const appJs = fs.readFileSync(path.join(pocPath, 'app.js'), 'utf8');
const uxJs = fs.readFileSync(path.join(pocPath, 'ux-foundation.js'), 'utf8');

const dom = new JSDOM(html, {
  url: "http://localhost/",
  runScripts: "dangerously"
});

dom.window.matchMedia = () => ({ matches: false, addListener: () => {}, removeListener: () => {} });
dom.window.requestAnimationFrame = (cb) => setTimeout(cb, 0);

// Add global error handler inside the DOM
dom.window.eval(`
  window.addEventListener('error', (e) => {
    console.error("DOM ERROR:", e.error.stack);
  });
`);

// Inject the scripts as tags
function inject(code) {
  const script = dom.window.document.createElement('script');
  script.textContent = code;
  dom.window.document.body.appendChild(script);
}

console.log("INJECTING APP");
inject(appJs);
console.log("INJECTING UX");
inject(uxJs);

console.log("DONE");
