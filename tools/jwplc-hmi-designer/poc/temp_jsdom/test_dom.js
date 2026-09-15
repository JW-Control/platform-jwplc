const fs = require('fs');
const { JSDOM } = require('jsdom');
const path = require('path');

const pocPath = path.resolve('..');
const html = fs.readFileSync(path.join(pocPath, 'index.html'), 'utf8');

const dom = new JSDOM(html, {
  url: "http://localhost/",
  runScripts: "dangerously",
  resources: "usable"
});

// Provide mocks
dom.window.matchMedia = () => ({ matches: false, addListener: () => {}, removeListener: () => {} });
dom.window.requestAnimationFrame = (cb) => setTimeout(cb, 0);

const appJs = fs.readFileSync(path.join(pocPath, 'app.js'), 'utf8');
const uxJs = fs.readFileSync(path.join(pocPath, 'ux-foundation.js'), 'utf8');

try {
  dom.window.eval(appJs);
  console.log("APP.JS LOADED OK");
} catch (e) {
  console.error("APP.JS ERROR:", e.stack);
}

try {
  dom.window.eval(uxJs);
  console.log("UX.JS LOADED OK");
} catch (e) {
  console.error("UX.JS ERROR:", e.stack);
}
