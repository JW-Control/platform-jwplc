# Gate F0c · OpenPLC: escritura HMI → PLC con el MENU (`%MW` / `%MD`)

Tercera prueba HMI Designer ↔ OpenPLC (Alpha14). Desde un MENU de la pantalla se
cambia el preset de un contador (`LIMITE`, INT en `%MW1`) y una consigna (`CONSIGNA`,
REAL en `%MD0`); el programa OpenPLC usa el nuevo valor en el scan siguiente.

| Archivo | Qué es |
|---|---|
| `A14_F0c_OpenPLC_Escritura.jwhmi` | Proyecto del Designer, 2 páginas: "Valores" (lo del F0b) y "Ajustes" (MENU). |
| `JWPLC_HMI_Generated.h` | Header generado por el Designer de `v2.1.0-alpha.15-HmiDesigner`. |
| `bindings.json` | Enlaces; `limite` y `consigna` con `"access": "RW"`. |
| `preview_designer.png` | Vista previa de la página 1 en el Designer. |

MENU "Ajustes":

| Nodo | Tipo | Variable HMI | Enlace |
|---|---|---|---|
| Contador | VIEW | `int32_t contador_menu` | `<- CONTADOR` (`%MW0`) |
| Limite | EDIT_NUM 1..100, paso 1 | `int32_t limite` | `<-> LIMITE` (`%MW1`, INT) |
| Consigna | EDIT_NUM 0..100, paso 0.5 | `float consigna` | `<-> CONSIGNA` (`%MD0`, REAL) |
| Lleno | VIEW (NO/SI) | `bool lleno` | `<- LLENO` (`%QX0.1`) |

Requiere el editor `develop/alpha14-openplc-hmi-v1` con enlaces RW. El VPP
2.1.0-alpha.30 no cambia: la escritura va en `jwplcHMIBindingsSnapshot()`.

## Cómo escribe la pantalla en el PLC

- El MENU sólo cambia la variable al confirmar con **OK** (ESC descarta).
- Al final de cada scan (`updateOutputBuffers()` del HAL), para cada enlace RW:
  1. si la variable HMI difiere de su copia sombra, se escribe en `%MW`/`%MD`
     (redondeada y limitada al rango del tipo IEC) y se actualiza la sombra;
  2. si el valor del PLC difiere de la sombra (lo cambió el programa, Modbus o
     el debugger), se copia a la pantalla.
- El programa ve el valor nuevo en el scan siguiente.
- Al arrancar manda el PLC: la pantalla toma el valor inicial del programa.
- Un forzado del debugger gana: lo que se confirme en pantalla vuelve al valor
  forzado.
- Sólo se escribe en memoria `%MW`/`%MD`; un `RW` sobre `%I`/`%Q` es error de
  compilación.

## Programa OpenPLC (programa `main`)

```text
VAR
    PULSADOR : BOOL AT PULSADOR;          (I0_0)
    RELE     : BOOL AT RELE;              (Q0_0)
    RESET    : BOOL AT RESET;             (I0_1)
    LLENO    : BOOL AT %QX0.1;            (Q0_1, o un alias en el I/O Mapping)
    CONTADOR : INT  AT %MW0;
    LIMITE   : INT  AT %MW1 := 10;
    CONSIGNA : REAL AT %MD0 := 50.0;
    CTU0     : CTU;
END_VAR
```

Ladder:

```text
Rung 1:  ──[ PULSADOR ]──────────────────( RELE )──

Rung 2:  ──[ PULSADOR ]──┬─────────┬──( LLENO )──
                         │  CTU0   │
                         │CU     Q ├
                  RESET ─┤R        │
                 LIMITE ─┤PV    CV ├─ CONTADOR
                         └─────────┘
```

`CONSIGNA` no se usa en el ladder: se observa en el debugger.

## Navegación en la placa

- Cualquier tecla: de la IDLE a la HMI (selección de página).
- LEFT/RIGHT: cambiar de página; OK: entrar al contenido de la página.
- En el MENU: UP/DOWN mover; OK editar; UP/DOWN cambiar (LEFT/RIGHT ±10 pasos);
  OK confirmar; ESC cancelar.
- ESC: salir del MENU → selección de página → IDLE.

## Resultado esperado (PASS en placa el 2026-10-17; ver `docs/v2.1.0-alpha.14/A14_HMI_V1_OPENPLC_20261017.md` §9)

| # | Acción | Esperado |
|---|---|---|
| F0c-1 | Arranque, página Ajustes | Limite 10, Consigna 50.0, Contador 0, Lleno NO |
| F0c-2 | Pulsar I0_0 hasta 10 | Lleno SI y Q0_1 encendida al llegar a 10 |
| F0c-3 | I0_1 (reset); editar Limite = 3 y OK | Debugger: `LIMITE` = 3; con 3 pulsos Lleno SI |
| F0c-4 | Editar Limite y ESC en vez de OK | No cambia nada en el PLC |
| F0c-5 | Editar Consigna = 72.5 y OK | Debugger: `CONSIGNA` = 72.5 |
| F0c-6 | Forzar `LIMITE` = 5 desde el debugger | La pantalla muestra 5; si se confirma otro valor, vuelve a 5 |
| F0c-7 | Quitar el forzado y escribir `LIMITE` desde el debugger | La pantalla muestra el valor nuevo |
| F0c-8 | Reiniciar la placa | Limite vuelve a 10 (valor inicial del programa, no es retentivo) |
| F0c-9 | ESC hasta la IDLE; debugger | Siguen funcionando |

## Verificación sin hardware `[ejecutado 2026-10-17]`

- Header generado con el Designer real (Chromium sin interfaz), sin errores.
- Glue del editor: 61 tests, 100 % de cobertura.
- Compilación con el runtime real de OpenPLC + HAL alpha.30 + `jwplc_local`:
  467749 B de flash, 38836 B de RAM; `HMI_MENUS`, `setMenus` y las sombras
  `jwplcHmiShadow_limite`/`_consigna` en el ELF.
