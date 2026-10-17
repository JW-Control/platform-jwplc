# Gate F0b · OpenPLC: valores numéricos en la pantalla (VALUE y BAR ← `%MW0`)

Segunda prueba HMI Designer ↔ OpenPLC (Alpha14). Un contador del programa OpenPLC
se ve en la TFT como número (VALUE) y como barra (BAR), junto al BOOL del relé del
gate F0.

| Archivo | Qué es |
|---|---|
| `A14_F0b_OpenPLC_Valores.jwhmi` | Proyecto del Designer: BOOL "Rele Q0.0", VALUE "Contador" (5 dígitos, sin decimales) y BAR "Nivel" (0..10). |
| `JWPLC_HMI_Generated.h` | Header generado por el Designer (`bool rele_q0_0`, `float contador`, `float nivel`). |
| `bindings.json` | `rele_q0_0 <- RELE`, `contador <- CONTADOR`, `nivel <- CONTADOR`. |
| `preview_designer.png` | Vista previa del lienzo del Designer. |

Requiere el editor `develop/alpha14-openplc-hmi-v1` con enlaces numéricos (el
VPP 2.1.0-alpha.30 no cambia).

## Programa OpenPLC (programa `main`)

Variables:

```text
PULSADOR  : BOOL AT %IX0.0   (I0_0, o su alias)
RESET     : BOOL AT %IX0.1   (I0_1, o su alias)
RELE      : BOOL AT %QX0.0   (Q0_0, o su alias)
CONTADOR  : INT  AT %MW0
CTU0      : CTU              (la crea el editor al poner el bloque)
```

Ladder:

```text
Rung 1:  ──[ PULSADOR ]──────────────────( RELE )──

Rung 2:  ──[ PULSADOR ]──┬─────────┬──
                         │  CTU0   │
                         │CU     Q │
                  RESET ─┤R        │
                     10 ─┤PV    CV ├─ CONTADOR
                         └─────────┘
```

Cada pulsación de I0_0 suma 1 a `CONTADOR`; I0_1 lo vuelve a 0. `CTU` sigue
contando después de `PV`; la barra (0..10) se queda llena desde 10.

Sin ladder también se puede probar: desde el debugger, forzar `CONTADOR` a
distintos valores.

## Resultado esperado (PASS en placa el 2026-10-17)

| # | Acción | Esperado |
|---|---|---|
| F0b-1 | Arranque + cualquier tecla | Página con Rele (OFF), Contador 0 y barra vacía |
| F0b-2 | Pulsar I0_0 tres veces | Contador 3, barra al 30 %; Rele ON mientras se pulsa |
| F0b-3 | Pulsar I0_1 | Contador 0, barra vacía |
| F0b-4 | Llegar a 10 o más | Barra llena; el número sigue subiendo |
| F0b-5 | Forzar `CONTADOR` = -5 y = 1234 desde el debugger | El número muestra -5 y 1234 (INT con signo) |
| F0b-6 | Debugger y ESC | Siguen funcionando igual que en F0 |

## Verificación sin hardware `[ejecutado 2026-10-17]`

- Header generado con el Designer de `v2.1.0-alpha.15-HmiDesigner` (Chromium sin
  interfaz, mismo método que `tests/codegen-golden.cjs`), sin errores de página.
- Compilación con el runtime real de OpenPLC + HAL alpha.30 + `jwplc_local`:
  461349 B de flash, 38068 B de RAM.
