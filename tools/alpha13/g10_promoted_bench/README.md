# Alpha13 G10 PH0 — banco de dos JWPLC conectados al router

**Decisión del operador 20/10/2026:** COM4 Master y COM14 Slave disponibles y autorizados para actualizar firmware G10-PH0. No realizar backup de flash, no consultar MAC ni exigir aprobación adicional por placa. Esta autorización se limita al upload de estos dos sketches; R1–R4, merge y release siguen separados. Antes de conectar cada equipo: banco aislado, Q sin cargas/tensiones y monitor serie cerrado durante upload. Si la identidad de puerto/rol es dudosa, STOP sin flashear.

## Corrección F135 (tooling PH0): nombre ELF/bin Arduino y recuperación sin recompilar

Con el HEAD `c08147eedf...`, N0 local dio PASS pero la preparación falló
en `ELF_MISSING_COM14_SLAVE`. `g10p_dual_build.py` terminó correctamente:
el verificador añadido asumió `<sketch>.elf` cuando el CLI genera
`<sketch>.ino.elf` (mismo patrón en `.ino.bin`, bootloader y particiones).
La corrección reconoce exclusivamente nombres de sketch `.ino`, coteja SHA
con MANIFEST y requiere los 3 binarios por placa. Ningún cambio en `JWPLC/`
ni en los sketches. Las compilaciones de CI se verifican ahora con
`--seal-existing --ci-detached`, no solamente con el N0 sintáctico.

**Para reutilizar sin recompilar la build local anterior**, tras un
`git pull --ff-only` con árbol limpio y nuevo HEAD, hallar directorios
temporales del intento previo:

```powershell
$head = (git rev-parse HEAD).Trim()
Get-ChildItem $env:TEMP -Directory -Filter 'g10p_ph0_c08147eedf3_*' |
  Sort-Object LastWriteTime -Descending |
  Select-Object -First 5 FullName, LastWriteTime
```

Elegir solo la carpeta de esa preparación, con `MANIFEST.json`,
`build_COM14_SLAVE` y `build_COM4_MASTER`. **No eliminarla ni modificarla**.
Luego:

```powershell
$out = 'PEGA_RUTA_FULLNAME_DEL_INTENTO_ANTERIOR'
py -3 tools/alpha13/g10_promoted_bench/g10p_ph0_upload.py --seal-existing --expected-head $head --output $out
```

Este modo no llama Arduino CLI ni abre COM, y aborta si detecta cambios en
package, sketches, contrato o compilador entre el HEAD que generó los binarios
y el actual. Exige coincidencia de fuente→objeto→ELF y hashes de los
archives `.a`, y produce `PH0_SEAL.json`. Si falta un binario o no se puede
demostrar procedencia, STOP y compilar con `--prepare` en un nuevo directorio
(ningún `--clean` sobre el repo).

## Flujo local versión controlada (Windows)

Ejecutar desde la rama limpia `codex/alpha13-g10-promote-functional-20261019`, tras `git pull --ff-only` **en esa misma rama**. No descargar ejecutables ni copiar scripts de chats. Requiere Arduino CLI y las herramientas del package instaladas, según el gate dual ya validado en CI.

```powershell
$head = (git rev-parse HEAD).Trim()
py -3 tools/alpha13/g10_promoted_bench/g10p_ph0_upload.py --check --expected-head $head
py -3 tools/alpha13/g10_promoted_bench/g10p_ph0_upload.py --prepare --expected-head $head
```

El segundo comando compila **ambos** sketches una sola vez en un proveedor Arduino temporal `jwplc_g10ph0:esp32:jwplcbasic`. Verifica fuente→objeto→ELF y hashes de binarios. No abre puertos USB ni hace upload. Anotar el directorio absoluto impreso en `G10P_PH0_OUTPUT` como `$out`:

```powershell
$out = 'RUTA_QUE_IMPRIMIO_G10P_PH0_OUTPUT'
py -3 tools/alpha13/g10_promoted_bench/g10p_ph0_upload.py --upload-com14 --expected-head $head --output $out
```

**Parar después de COM14** y verificar por UART 115200 `G10P_COM14_BOOT`, `G10P_COM14_HEARTBEAT`, contador ascendente y ausencia de watchdog/panic. Cerrar monitor COM14 antes de futuras operaciones sobre ese puerto. No continuar si falla su arranque.

```powershell
py -3 tools/alpha13/g10_promoted_bench/g10p_ph0_upload.py --upload-com4 --expected-head $head --output $out
```

Comprobar por UART COM4 a 115200 `G10P_COM4_BOOT` y `G10P_COM4_HEARTBEAT`, mismo BOOT y HB ascendente sin resets. El ejecutor exige comprobante local de upload exitoso de COM14 para permitir el paso COM4, pero **no reemplaza la observación humana UART**. Usa `arduino-cli upload --input-dir` con los binarios compilados, no recompila ni ejecuta `erase-all`, no lee explícitamente MAC ni copia flash. Un loader puede mostrar identificadores automáticamente en consola; no publicarlos.

Con ambos boots estables, conectar RS-485 previamente validado (A/P, B/N, referencia) y comprobar `RTU_OK` en COM4 y `RTU_REQUESTS` en COM14. Luego conectar RJ45 de **ambas placas al mismo router DHCP aislado**, PC en esa misma LAN, con dos IP privadas diferentes y `PHY=1 ETH=1 TCP=1`. Ejecutar desde el repo:

```powershell
py -3 tools/alpha13/g10_promoted_bench/g10p_modbus_tcp_readonly.py --com4-ip IP_COM4 --com14-ip IP_COM14
```

Sustituir los marcadores IP por valores capturados en UART. Esperados FC03: COM4 `1234 2345 3456 4567`; COM14 `5678 6789 789A 89AB` (hex). La comprobación RTU y FC03 física todavía son `NOT_RUN` hasta recibir resultados del banco. Prueba TCP directa entre PLC y R1–R4 no quedan autorizadas automáticamente.

## Antecedente y topología

PR #124 continúa Draft y F1 CS GPIO5 queda intacta. La sección siguiente describe el diseño original del gate; la decisión de no hacer backup ni consultar MAC de arriba prevalece para las dos placas de laboratorio.

## Topología de banco

    Router/switch de laboratorio (DHCP activo, no planta)
          | Ethernet RJ45             | Ethernet RJ45
    JWPLC COM4 Master                 JWPLC COM14 Slave
          |___________ RS-485 A/B ____________|
                  PC en misma LAN para FC03 TCP

No confundir RJ45 con el bus RS-485. Cada PLC tiene USB serial independiente y una dirección IP propia. COM14 no tiene microSD; el sketch no la accede.

- COM4: G10P_COM4_Master. Ethernet DHCP, Modbus TCP server (Unit 1 / TCP 502), UDP eco puerto 15020, RTU master 115200 8N1 que lee cuatro holding del Slave ID 2. Periféricos autoload normales, SD/FRAM solo lectura y Q sin tocar.
- COM14: G10P_COM14_Slave. Ethernet DHCP, Modbus TCP server (Unit 1 / TCP 502), RTU slave ID 2 / 115200 / 8N1. Holding RTU separado de holding TCP para no crear carreras entre protocolos.

## Gates y orden

1. CI dual Ubuntu/Windows build-only: compilar los dos sketches con FQBN aislado jwplc_g10ph0:esp32:jwplcbasic; comprobar plataforma fuente, dependencias GCC .d, objeto .o, ELF y SHA. Ningún USB ni upload.
2. Gate físico PH0 autorizado para COM4 y COM14: comprobar puerto/rol, no cargas/tensiones Q, red aislada y ausencia de monitor serial durante upload. El operador descartó backup de flash y comprobación de MAC para este banco; no se extraen identificadores por iniciativa del runner.
3. Tras autorización y upload validado, comprobar COM14 con etiquetas G10P_COM14_BOOT/HEARTBEAT y COM4 con etiquetas G10P_COM4_BOOT/HEARTBEAT, 115200. Ambos deben progresar sin WDT/reset imprevisto.
4. Conectar AMBOS al mismo router/switch aislado y observar PHY=1, ETH=1, TCP=1 e IPs LAN diferentes (sin conflictos DHCP) en COM4 y COM14. PHY=2 inicial puede significar negociación. PHY=0 persistente o watchdog significa parar.
5. Desde PC en la misma LAN, ejecutar exclusivamente la consulta FC03 de lectura a las dos IP ya observadas. El script usa Modbus TCP 502/Unit ID 1/holding 0..3 y no escribe registros, firmware ni flash. Valores esperados COM4: 1234,2345,3456,4567 hex; COM14: 5678,6789,789A,89AB hex.

    py -3 tools/alpha13/g10_promoted_bench/g10p_modbus_tcp_readonly.py --com4-ip 192.168.X.Y --com14-ip 192.168.X.Z

6. Confirmar por UART que COM4 RTU_OK crece y COM14 RTU_REQUESTS crece mientras RS-485 A/P, B/N y tierra/terminación permanecen como en banco validado. Este gate comprueba coexistencia TCP y RTU, no comunicación TCP directa entre PLCs.
7. Una prueba opcional posterior COM4→COM14 por Modbus TCP y las campañas físicas R1–R4 requieren sus propios criterios/autorizaciones. No reutilizar el sketch experimental g10_r_integrated.ino con sentinela LINK=-9, hooks RTC y funciones privadas.

## Comandos build-only locales (cuando CI esté verde)

En una rama limpia del PR #124:

    $head = (git rev-parse HEAD).Trim()
    $out = Join-Path $env:TEMP ("g10p_dual_" + $head.Substring(0,12) + "_" + (Get-Date -Format "yyyyMMddHHmmss"))
    py -3 tools/alpha13/g10_promoted_bench/g10p_dual_build.py --expected-head $head --output $out

El comando no abre COM4/COM14. Deja manifiesto SHA y logs en %TEMP%; no publica ELF/binarios. El uploader de ambos roles se incorporó como g10p_ph0_upload.py, con preparación y upload separado por placa y sello SHA de binarios. Consultar arriba.

## Estado

    G10_PH0_SCOPE=COM4_AND_COM14_APPROVED_FOR_PREPARATION
    G10_PH0_DUAL_ETHERNET_ROUTER=PLANNED_NOT_RUN
    G10_PH0_BUILD_BOTH=CI_PENDING_OR_COMPLETED_REVIEW
    G10_PH0_UPLOAD=AUTHORIZED_FOR_BOTH_NOT_YET_EXECUTED
    G10_PH0_TCP_FC03=NOT_RUN
    G10_PH0_RTU_MASTER_SLAVE=NOT_RUN
    G10_R_R1_R2_R3_R4=NOT_RUN_NOT_AUTHORIZED
    G10_ALPHA13_BASE_MERGE=NOT_RUN
