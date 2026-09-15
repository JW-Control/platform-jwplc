
const puppeteer = require('puppeteer');

(async () => {
  const browser = await puppeteer.launch();
  const page = await browser.newPage();
  
  page.on('console', msg => console.log('PAGE LOG:', msg.text()));
  page.on('pageerror', err => console.log('PAGE ERROR:', err.toString()));
  
  await page.goto('http://127.0.0.1:8765/desktop.html', { waitUntil: 'networkidle0' });
  
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
  
  const textBtn = await page.$('.tool[data-tool="textField"]');
  if (textBtn) {
    console.log("TEXT BTN FOUND");
    await textBtn.click();
    console.log("CLICKED TEXT BTN");
  }
  
  await browser.close();
})();
