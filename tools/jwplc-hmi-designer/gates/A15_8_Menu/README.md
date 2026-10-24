# Gate A15-8 · MENU en la placa

`A15_8_Menu.ino` declara el MENU a mano (sin Designer). Página 01: un VALUE con
la temperatura. Página 02: el MENU "Ajustes" con 3 niveles.

Requiere el package con `JWPLC_Display` 1.1.0 (`libJWPLC_Display.a` de la rama
`v2.1.0-alpha.15-HmiDesigner`).

## Pasos y resultado esperado

| # | Acción | Esperado |
|---|---|---|
| M1 | Encender, entrar a USER | Página 01 con "Temp" que sube cada 0.5 s |
| M2 | RIGHT | Indicador 02/02; se ve el MENU con "Temperatura" con contorno cian (sin foco) |
| M3 | OK | Indicador sombreado; la fila "Temperatura" pasa a relleno cian |
| M4 | DOWN, DOWN, UP | La selección se mueve sin parpadeo del resto del menú |
| M5 | OK sobre "Consigna", UP ×3 | Fila naranja con `<46.5 C>`; mantener UP acelera |
| M6 | ESC | Vuelve a 45.0 C (no se escribió) |
| M7 | OK, UP, RIGHT, OK | Consigna 50.5 C; Serial imprime `menu: nodo 1 ...` |
| M8 | DOWN a "Bomba", OK, UP, OK | MARCHA; "Temperatura" empieza a bajar sola en el menú |
| M9 | DOWN a "Avanzado", OK | Título `< Avanzado`; "Ciclos" cuenta solo |
| M10 | DOWN, DOWN, RIGHT (Alarmas) | Título `< Alarmas` (nivel 3) |
| M11 | ESC, ESC | Vuelve a la raíz con "Avanzado" seleccionado |
| M12 | ESC en la raíz | Vuelve al selector de páginas (indicador sin sombrear) |
| M13 | Ir a página 01 y volver | El MENU se redibuja completo, en la raíz |

Anotar en `docs/v2.1.0-alpha.15/` el resultado de cada paso, foto de la TFT para
M2, M5 y M10 (comparar con `menu_*.png`, que es la simulación del firmware en
host) y si la repetición de UP/DOWN se siente bien.

`menu_*.png`: framebuffer de `tests/menu-host-sim.cjs` (mismo código de firmware).
