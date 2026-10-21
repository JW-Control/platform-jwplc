## Decisión vigente — placas de desarrollo SIN respaldo previo (21/10/2026)

**Corrección de alcance:** la decisión del propietario del 17/10/2026 sigue vigente: COM4/COM14 son unidades de laboratorio con firmware previo prescindible. No se requiere ni se ofrece respaldo del sketch o full-flash en H-F137, y no se vuelve a solicitar renuncia por cada alpha. Esto no se aplica a equipos de clientes.

H-F137 usa ahora tools/alpha13/h_f137_com4.py para selftest, verificación de SHA del package y enumeración USB COM4; COM14 es opcional para inventario y permanece intacto. No abre puertos, no usa esptool ni reinicia la placa. El paso posterior (upload COM4) conserva aprobación independiente y explícita, identidad física del Master y salidas Q sin cargas. N3 Ubuntu/Windows 13/13 PASS; el resultado físico todavía no se ha ejecutado.

---

# Alpha13 F137 — validación física mínima tras cambio de política Ethernet

**Estado vigente:** `H_F137_N0_OFFLINE=PASS_WINDOWS_UBUNTU`; se aprobó iniciar el gate de identificación y respaldo **COM4 solamente**. [N3 #39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047) pasó 13/13 en Windows y Ubuntu; [N0 H #39545618484](https://github.com/JW-Control/platform-jwplc/actions/runs/39545618484) pasó autopruebas en ambos SO. Procedimiento versionado de identidad/backup: [tools/alpha13/h_f137_com4/README.md](../../tools/alpha13/h_f137_com4/README.md) y runner `tools/alpha13/h_f137_com4.py`. **Aún no hay identidad ni backup físico nuevo observado.** Inventory no abre puertos; `flash_id/read_flash` pueden reiniciar COM4 al entrar al bootloader, requieren consentimiento interactivo y banco aislado. **No están autorizados upload, erase, restauración, maniobra RJ45 ni COM14.**

## Motivo y alcance

La corrección de F137 cambia código de `JWPLC_Ethernet.cpp` de compilación por perfil a guardas runtime: en `jwplcbasic` la política weak debe mantenerse habilitada, y en `jwplcbasiccore` el core presenta override strong deshabilitado. No modifica APIs ni registros de Q, CS W5500, GPIO4, otros archives, `core.a` ni fuentes ModbusTCP. El gate N2 positivo debe mostrar objeto idéntico por fuente de ambos FQBN (build path controlado), símbolo `W` Basic / `T` Core en ELF, y que el `.a` se selecciona sin recompilar fuentes.

## Banco mínimo propuesto (requiere permiso de upload COM4)

1. Recuperar HEAD exacto y SHA binarios desde `platform-jwplc` y resultado N3; usar solo runner/sketch **versionado** (ningún ZIP temporal descargado ni edición manual de variant). COM4 Master, COM14 Slave en router aislado; COM14 y sus contenidos permanecen intactos. Preflight de puerto/identidad y confirmación del operador antes de cualquier upload.
2. Compilar source→archive→ELF con package real del HEAD aprobado, aplicar upload únicamente en COM4 después de aprobación explícita de esa placa; no erase-all, no modificación de flash ajena, conservar archivos de manifiesto/log en ruta local del operador.
3. Observar arranque, `BOOT` estable, `HB` continuo, `PHY=1 ETH=1 TCP=1` y DHCP en la red aislada; exigir ausencia de `TG0/TG1`/panic sin confundir reset DTR/RTS con fallo.
4. Desde PC ejecutar FC03 read-only a COM4, eco UDP y observar progreso RTU Master→COM14 Slave, RTC/SD/FRAM/IO/TFT sin nuevos errores. Reutilizar el criterio reducido de G10 R1/R2 sin relanzar automáticamente la campaña de 605 s. Proponer ventana dirigida de 90–120 s y, solo si el operador autoriza, **una** desconexión/reconexión RJ45 del Master para confirmar PHY/servicio.
5. Basic Core: verificar N2/ELF y prueba negativa de `isEnabled() == false`, callback inactivo y ausencia de ruta SPI/CS/W5500. Cualquier prueba física de Core que requiera reflashear otro equipo se solicita por separado; no se infiere GPIO5 medido por un PASS lógico.

## Criterios de parada

- SHA/procedencia distinto, ELF sin override strong Core, fallo de arranque, reset `TGx`, IO inesperado, CS de W5500 diferente del package, errores de Modbus/RTU nuevos, prueba insuficiente o dispositivo/puerto dudoso → **REVIEW_STOP**. Conservar logs y no reinterpretar como PASS.
- **No repetir** H0-F1 10+10, G10 R1 completo, R2 3/3, R3 interno ni R4 1805 s sin evidencia que invalide las pruebas precedentes. R3 interno, R4 largo y DNS/DHCP handshake específicos siguen riesgos residuales aceptados, E1/E2 siguen PENDING.

## Secuencia de cierre

`N0 → N1_NEGATIVO → N2_OBJETOS/ELF → N3_PACKAGE_FINAL → H_ACOTADO_AUTORIZADO → C_PR125_MERGE_OPERADOR → RELEASE_DISPATCH_OPERADOR → ZIP_PUBLICADO_IDE_CLI → CLOSED_PUBLISHED`.

Nada en este documento autoriza merge de PR #125, ejecución del workflow Release ni cierre automático de Alpha13.
