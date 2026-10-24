# Gate A15-8 · MENU generado por el Designer

- `A15_8_Menu_Designer.jwhmi`: proyecto del Designer (abrir con "Abrir").
- `JWPLC_HMI_Generated.h`: header que genera el Designer para ese proyecto
  (lo produce `tests/menu-smoke.cjs`).
- `A15_8_Menu_Designer.ino`: sketch que lo usa (`jwplcHMISetup()` +
  `menuChanged()`).

Una sola página: el MENU recibe las teclas sin pasar por el selector de páginas.
Mismos pasos que `../A15_8_Menu/README.md` (M3 en adelante). Serial a 115200
imprime cada valor confirmado.
