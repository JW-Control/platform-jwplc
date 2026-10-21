# Alpha13 F137 — H COM4: identidad y respaldo sin upload

**Estado:** preparación de un gate físico. Esta ficha NO autoriza upload, erase, restauración, maniobra RJ45 ni actuación en COM14. COM4 se utilizará solo para comprobaciones y lectura completa de flash después de una frase de consentimiento local. El acceso por esptool puede reiniciar COM4; nunca hacerlo en una máquina en operación.

**Fuente:** PR #125 Draft, rama codex/alpha13-precompile-benchmark-docs-20261020. N2, benchmark y N3 Windows/Ubuntu 13/13 PASARON en CI #39538155047. Los archivos precompilados no están aún en la base Alpha13.

## Precondiciones de seguridad

Banco aislado, sin cargas ni tensión de red en salidas Q, alimentación segura, monitores serie cerrados, PC Windows con Python, pyserial y esptool previamente disponibles. Identificar físicamente COM4 = Master con microSD y COM14 = Slave sin microSD. Que ambos figuren como CH340 VID/PID 1A86:7523 no demuestra por sí solo su correspondencia física. El runner revisa identidad PnP diferenciada y flash 4 MB, pero exige la verificación presencial del operador.

No usar el runner antiguo g10p_ph0_upload.py: exige COM14 primero, fue creado para otra rama y no respalda flash. No mover GPIO4, no cambiar pines, no usar erase-all ni modificar periféricos. La lectura full-flash tampoco respalda la tarjeta microSD, que se debe preservar por separado si contiene datos valiosos.

## Pasos PowerShell en Windows

Primero examinar el estado Git local: si hay cambios tracked, detenerse, sin git reset/clean/stash automático.

    git status --short
    git branch --show-current

Con el worktree limpio, situarse en la rama Draft y recuperar scripts exclusivamente mediante Git:

    git switch codex/alpha13-precompile-benchmark-docs-20261020
    git pull --ff-only
    $head = (git rev-parse HEAD).Trim()
    py -3 tools/alpha13/h_f137_com4.py --selftest
    py -3 tools/alpha13/h_f137_com4.py --check --expected-head $head
    py -3 tools/alpha13/h_f137_com4.py --inventory --expected-head $head

Selftest y check no abren COM; inventory enumera USB sin abrir COM ni provocar reset. Si cualquier etapa devuelve REVIEW_STOP, no avanzar.

Tras verificar realmente COM4 Master, aislamiento y ausencia de cargas, hacer copia única en directorio nuevo y persistente fuera del checkout. La entrada interactiva exige escribir exactamente AUTORIZO LECTURA Y RESPALDO SOLO COM4:

    $backupDir = Join-Path ([Environment]::GetFolderPath('MyDocuments')) ('JWPLC_Backups\F137_COM4_' + (Get-Date -Format 'yyyyMMdd_HHmmss'))
    py -3 tools/alpha13/h_f137_com4.py --backup-com4 --expected-head $head --output $backupDir

La operación realiza únicamente esptool flash_id y read_flash 0x0 0x400000 sobre COM4. Puede resetear COM4 al entrar/salir del bootloader. Exige 4 194 304 bytes y SHA-256. Si se corta, conservar directorio parcial y detenerse; no relanzar ni borrar backup anterior. No restaura firmware ni escribe flash. Guardar el archivo COM4_FULL_FLASH_0x400000.bin y MANIFEST_LOCAL.json en PC y una segunda ubicación privada, nunca GitHub. No publicar MAC, USB PnP, rutas sensibles ni imagen.

Salida positiva: H_F137_COM4_BACKUP=PASS_FULL_FLASH_SHA256, H_F137_FLASH_SIZE_BYTES=4194304, H_F137_COM14=UNTOUCHED, H_F137_UPLOAD=NOT_RUN.

## Puertas posteriores (no autorizadas todavía)

Después de verificar el recibo local de respaldo, preparar compilación COM4 del package real en un stage aislado, verificar SHA de ELF/binarios y evidencia source-archive-ELF; entonces solicitar autorización de upload COM4 por separado. No instalar nada en COM14. La prueba de tráfico físico FC03/UDP, UART y RJ45 exige confirmación del operador para la maniobra correspondiente. No repetir la campaña G10 de 605 s ni R4 de 1805 s sin nuevos motivos.

H_COM4_IDENTITY_BACKUP=WAIT_OPERATOR; H_COM4_UPLOAD=NOT_AUTHORIZED; COM14_UNTOUCHED=YES; ALPHA13_RELEASE=BLOCKED.
