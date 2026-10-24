// Añade el Device "JWPLC Basic" desde la Device Library de mBlock usando clics
// DOM (funciona aunque la ventana esté minimizada). Admite la interfaz en
// español o en inglés. Uso: node cdp.cjs cdp-add-device.js
(async () => {
    const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
    const vis = (e) => { const r = e.getBoundingClientRect(); return r.width > 0 && r.height > 0; };
    const clickText = (texts) => {
        for (const t of texts) {
            const els = [...document.querySelectorAll('body *')].filter((e) =>
                vis(e) && (e.innerText || '').trim() === t &&
                ![...e.children].some((c) => (c.innerText || '').trim() === t));
            const e = els[els.length - 1];
            if (e) { e.click(); return t; }
        }
        throw new Error('no encontrado: ' + texts.join(' / '));
    };
    if (!window.__jwReq) window.webpackChunkmblock_next.push([[Symbol('jw')], {}, (r) => { window.__jwReq = r; }]);
    window.__jwA = window.__jwReq(98243).default;

    clickText(['Añadir', 'Add']);
    await sleep(2500);
    const input = [...document.querySelectorAll('input')].find((i) => vis(i) && (i.placeholder === 'Buscar' || i.placeholder === 'Search'));
    const setValue = Object.getOwnPropertyDescriptor(HTMLInputElement.prototype, 'value').set;
    setValue.call(input, 'JWPLC Basic');
    input.dispatchEvent(new Event('input', { bubbles: true }));
    input.dispatchEvent(new KeyboardEvent('keydown', { key: 'Enter', keyCode: 13, bubbles: true }));
    await sleep(2500);
    clickText(['JWPLC Basic']);
    await sleep(800);
    clickText(['Aceptar', 'OK']);
    await sleep(5000);
    return {
        device: window.__jwA.vmCtx.target.deviceId,
        categories: [...document.querySelectorAll('.scratchCategoryMenuItemLabel')].map((e) => e.innerText),
    };
})()
