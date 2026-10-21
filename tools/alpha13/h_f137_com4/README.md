# Alpha13 F137 — H COM4 sin respaldo obligatorio

**Decisión vigente del propietario:** COM4 (Master con microSD) y COM14 (Slave sin microSD) son unidades de desarrollo, su firmware anterior es prescindible y no se exige respaldo de sketch ni full-flash antes de reprogramarlas. Se reutiliza la excepción documentada el 17/10/2026 en A13_G10_R_PREH_B4_BANCO_RESPALDOS_SEGURIDAD_20261017.md; no extenderla a equipos de clientes.

## Hito actual: identidad e integridad, sin flasheo

Runner: tools/alpha13/h_f137_com4.py; PR #125 Draft, rama codex/alpha13-precompile-benchmark-docs-20261020.

- --selftest: contrato sintético positivo/negativo, no hardware.
- --check: HEAD limpio y SHA de los archives de Ethernet y Modbus TCP más precompiled=full.
- --inventory: enumera el USB de Windows (sin abrir COM); COM4 debe ser CH340 VID/PID 1A86:7523; si COM14 está conectado, comprueba VID/PID y que su identidad USB sea distinta. COM14 no es requisito para inventariar COM4 y no se interviene.
- No hay modo --backup-com4, read_flash, borrado, restore ni upload. No se pide instalar esptool. El inventario de Windows no verifica por sí solo el rol físico de la placa; el operador debe confirmar COM4=Master.

En PowerShell, desde un worktree limpio y sin descartar cambios:

    git status --short
    git switch codex/alpha13-precompile-benchmark-docs-20261020
    git pull --ff-only
    $head = (git rev-parse HEAD).Trim()
    py -3 tools/alpha13/h_f137_com4.py --selftest
    py -3 tools/alpha13/h_f137_com4.py --check --expected-head $head
    py -3 tools/alpha13/h_f137_com4.py --inventory --expected-head $head

Si pyserial falta, el inventario da REVIEW_STOP; no modificar estado de la placa para resolver un problema de dependencia. Confirmar banco aislado y salidas Q sin cargas/tensión de red, COM4 Master físicamente identificado y monitor serial cerrado antes de un upload futuro.

## Siguiente gate

El próximo paso es **compilar y sellar los binarios reales desde el package del PR #125**, comprobar el sketch/HEAD/sha y solicitar autorización expresa **para subir únicamente COM4**. Se acepta que el sketch anterior de la placa de pruebas se sobrescribirá sin backup, según la decisión existente. COM14 permanece sin reprogramar y la maniobra RJ45 requiere acuerdo separado.

N2+N3: PASS en CI previa. H_COM4_BACKUP=WAIVED_DEV_BOARD; H_COM4_IDENTITY=WAIT_LOCAL_OPERATOR; H_COM4_UPLOAD=SEPARATE_EXPLICIT_APPROVAL; COM14_NO_UPLOAD=YES; RELEASE=BLOCKED.
