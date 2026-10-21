# Alpha13 H-F137 — preparación COM4 Master sin respaldos

**Rama:** codex/alpha13-precompile-benchmark-docs-20261020 (PR #125 Draft).

**Excepción vigente (17/10/2026):** COM4 Master y COM14 Slave son placas de desarrollo, con firmware previo prescindible. No se exige respaldo de sketch ni full-flash. Esta excepción no aplica a PLC en servicio o de clientes.

## Contrato técnico

Se reutiliza el sketch G10P_COM4_Master ya validado en N3. El ejecutor tools/alpha13/h_f137_com4_build.py prepara SOLO COM4 desde una copia completa del package del PR. Comprueba HEAD, árbol limpio, SHA-256 de los dos archives, precompiled=full, origen de la plataforma, opciones -L/-l y cero recompilaciones de las fuentes de Ethernet/Modbus TCP. Sella un ELF, firmware, bootloader y particiones (todos asociados a esa configuración de build) con sus hashes en MANIFEST.json fuera del checkout.

La CI Windows+Ubuntu corre el modo --prepare, pero **no abre USB ni tiene permiso de upload**. El bootloader del build no se considera definitivo para la plataforma.

## Operación Windows PowerShell

En el checkout ya existente:

    git status --short
    git switch codex/alpha13-precompile-benchmark-docs-20261020
    git pull --ff-only
    $head = (git rev-parse HEAD).Trim()

Si Git muestra archivos tracked modificados, detenerse: no ejecutar clean/reset/stash automático.

Comprobar y enumerar COM4 sin abrirlo (COM14 es opcional):

    py -3 tools/alpha13/h_f137_com4.py --selftest
    py -3 tools/alpha13/h_f137_com4.py --check --expected-head $head
    py -3 tools/alpha13/h_f137_com4.py --inventory --expected-head $head

Compilar una vez con Arduino CLI 1.5.1 y toolchain del package ya instalado, sin tocar hardware:

    $out = Join-Path $env:TEMP ("jwplc_f137_com4_" + (Get-Date -Format "yyyyMMdd_HHmmss"))
    py -3 tools/alpha13/h_f137_com4_build.py --prepare --expected-head $head --output $out

Resultado esperado: H_F137_COM4_BUILD=PASS_ARCHIVES_BINARIES_SEALED y H_F137_COM4_UPLOAD=NOT_RUN. Guardar la ruta $out y su MANIFEST.json. Un REVIEW_STOP impide continuar.

## Subida COM4: autorización INDEPENDIENTE

El operador aprobará por separado **subir solo COM4**, después de verificar su identidad física, binarios y banco aislado. No ejecutar --upload-com4 antes de esa aprobación. El modo posterior de upload requiere además confirmación interactiva textual, vuelve a verificar HEAD/manifiesto, usa exactamente los binarios sellados con arduino-cli upload --input-dir y no solicita borrar toda la flash. COM14 nunca se abre para subir firmware.

Tras upload autorizado, verificar UART 115200 (BOOT/HB estables sin TG0/TG1/panic), PHY/ETH/TCP, DHCP, FC03, UDP y progreso RTC, RTU, SD, FRAM, IO/TCA y TFT. RJ45 requiere autorización separada. No repetir R1/R4 largos por inercia.

H_F137_BACKUP=WAIVED_DEV; H_F137_BUILD=CI_PENDING; H_F137_UPLOAD=WAIT_EXPLICIT_APPROVAL; COM14=UNTOUCHED; PR125=DRAFT; RELEASE=BLOCKED.
