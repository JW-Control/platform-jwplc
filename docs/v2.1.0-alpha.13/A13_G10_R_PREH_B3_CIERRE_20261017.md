# Alpha13 — PRE-H B3: cierre integrado N0/N1/N2/N3 (17/10/2026)

**Dictamen:** `B3=CLOSED_NONPHYSICAL_CI_VERIFIED`.  
**HEAD exacto de código validado:** `ba1b0b1ddad78fbdc574b1590abae06bff1030a6`.  
**Rama:** `codex/alpha13-preh-tcp124-review-20261016` (PR #109, Draft).  
**Producto:** sin adopción de overlay G10-R; `A13-006=ADOPTION_BLOCKED`.

## Evidencia CI concluyente para este HEAD

| Nivel | Evidencia | Resultado | Alcance |
|---|---|---|---|
| N0 moderno | [B3 integración #39015874604](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874604) | Windows + Ubuntu **2/2 PASS** | Scope exacto, pruebas negativas de whitelist, congelación `JWPLC/`, GPIO4, archivos `.a`, Alpha15, flags físicos `False`, preflight y dry-run |
| N0 legado reconciliado | [#39015874600](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874600) | **2/2 PASS** | Mismo verificador compartido de rutas explícitas; se corrigió desfase de whitelist, sin permitir rutas amplias |
| N1 | [B3 integración #39015874604](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874604) | **1/1 PASS** | C++ source-backed mutex/lifecycle, pruebas negativas TCP lease/UART, DHCP/periféricos |
| N1 replay legado | [#39015874548](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874548) | **2/2 PASS** | Replay y negativos Ubuntu/Windows; host C++ fuente real **1/1 PASS** adicional |
| N2 | [G10-R build #39015874548](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874548) | COM4 + COM14 **2/2 PASS** | Arduino CLI, `jwplc_hbench` aislado, objetos `JWPLC_W5500_SafeAccess`, `JWPLC_ModbusTCP`, `JWPLC_Ethernet` `.cpp → .d → .o → ELF` |
| N2 dirigido TCP124 | [#39015874532](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874532) | Windows + Ubuntu **2/2 PASS** | La misma compilación source-first dirigida, sin USB/flash |
| N3 baseline | [G10-I original #38843599043](https://github.com/JW-Control/platform-jwplc/actions/runs/38843599043) | **11/11 PASS reutilizado** | El N0 actual comprueba `git diff --quiet a954cabd… HEAD -- JWPLC/`: fuente productiva exacta, por lo que no se repite compilación baseline |
| N3 candidato actual | [B3 #39015874604](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874604) | **11/11 PASS nuevo**, Ubuntu | 11 sketches contra staging experimental actualizado (incluye TCP124, DHCP B2), selección de biblioteca real, dependencias `.d`, objeto W5500, ELF y SHA |
| Contratos B1/B2 | [B1 #39015874536](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874536), [B2 #39015874640](https://github.com/JW-Control/platform-jwplc/actions/runs/39015874640) | **2/2 + 2/2 PASS** | Windows + Ubuntu |

El job N3 publica artifact `alpha13-b3-n3-39015874604` con `manifest.json`, resultados por sketch, hashes y logs. Los 11 nombres quedan enumerados en `a13_g10_r_b3_matrix.py`. Antes del build se comparan **todos** los `.a` encontrados byte a byte; la validación independiente del conjunto de cinco archives congelados se conserva en el compositor. `boards.txt` y `platform.txt` se comparan sin mutación.

## Decisiones y regresiones

1. Los workflows G10-R anteriores usaban whitelists de una etapa previa y rechazaban archivos PRE-H legítimos antes de ejecutar pruebas. Se reemplazaron ambos filtros por `a13_g10_r_b3_scope.py`, con rutas exactas, casos negativos y protección explícita de producto, Alpha15 y operadores físicos. **No se eliminó ninguna restricción de seguridad.**
2. Una primera corrida N3 B3 falló porque el verificador confundía el número de **cinco archives protegidos** con el número total de `.a` del package. Se corrigió el test para verificar todos los archives existentes por SHA y conservar el gate separado del conjunto congelado. Ese fallo inicial **no se registra como PASS**; su corrida sucesora definitiva es #39015874604.
3. N3 baseline se reutiliza por equivalencia de fuentes; N3 candidato sí se recompila porque su staging agrega hooks weak y observabilidad. No inferir equivalencia de candidate desde G10-I.
4. La matriz N3 representa integración con Arduino CLI y plataformas temporales en CI. No certifica compatibilidad de instalación final del package/Arduino IDE local, temporizaciones físicas, DHCP renew/rebind reales ni prueba prolongada.
5. No hubo upload, COM4/COM14 físico, contacto con router ni adopción. No se tocó `JWPLC/2.1.0`, GPIO4, archives ni Alpha15.

## Continuidad

```text
PREH_B1_TCP124=CLOSED_NONPHYSICAL
PREH_B2_DHCP_FAIRNESS=CLOSED_NONPHYSICAL
PREH_B3_N0_N1_N2_N3=CLOSED_NONPHYSICAL_CI_VERIFIED
PREH_B4_BENCH_AUTH_BACKUP=OPEN
PREH_STEPS_REMAINING=1
G10-R-H=BLOCKED_NOT_AUTHORIZED
R3_PHYSICAL_INJECTION_READY=FALSE
R3_COMPLETE_PHYSICAL_CAMPAIGN_READY=FALSE
A13-006=ADOPTION_BLOCKED
NEXT_GATE=PREH_BLOCK_4_PHYSICAL_SAFETY_BACKUP_AND_OPERATOR_AUTH
```
