const puppeteer = require('puppeteer');
const path = require('path');

(async () => {
  const browser = await puppeteer.launch();
  const page = await browser.newPage();
  
  page.on('console', msg => console.log('PAGE LOG:', msg.text()));
  page.on('pageerror', err => console.log('PAGE ERROR:', err.toString()));
  
  const pocPath = path.resolve('..', 'desktop.html');
  await page.goto('file://' + pocPath, { waitUntil: 'networkidle0' });
  
  console.log("PAGE LOADED");
  
  // Try to find the line button
  const lineBtn = await page.$('.tool[data-tool="line"]');
  if (lineBtn) {
    console.log("LINE BTN FOUND");
    await lineBtn.click();
    console.log("CLICKED LINE BTN");
  } else {
    console.log("LINE BTN NOT FOUND");
  }
  
  // Get body innerHTML
  const html = await page.evaluate(() => document.body.innerHTML.substring(0, 500));
  console.log("BODY HTML:", html);
  
  await browser.close();
})();
