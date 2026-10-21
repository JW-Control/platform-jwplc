## Actualización H-F137 — build sellado COM4 ya acreditado (21/10/2026)

[CI #39551136908](https://github.com/JW-Control/platform-jwplc/actions/runs/39551136908), SHA `bbb1af42269b66f17c79804d4ef140496882f484`, **PASS Windows+Ubuntu** en compilación de un único sketch Master, SHA archives, enlace -L/-l y cero cpp.o de ambos precompilados; sellado ELF/bin/bootloader/particiones. El sketch emite `VER=G10_PROMOTED_PRODUCT_F137_V1` al arrancar, distinguiéndose del firmware PH0 anterior. El runner es `tools/alpha13/h_f137_com4_build.py`; no hay backup obligatorio para las dos placas de prueba. **COM4 físico, identidad local y upload aún no ejecutados ni autorizados.** Cualquier futuro upload requiere consentimiento explícito y binarios locales sellados. COM14 no se programa. Base de alpha sin nuevos archives, PR #125 Draft.

---

# Alpha13 F137 — H física mínima, placas de desarrollo sin backup

**Estado (21/10/2026):** N2/benchmark/cross-OS PASS, N3 PASS 13/13 en Ubuntu y Windows ([CI #39538155047](https://github.com/JW-Control/platform-jwplc/actions/runs/39538155047)). La adopción de los dos archives Ethernet/ModbusTCP existe únicamente en el PR #125 Draft. H física aún no ejecutada.

## Decisión vinculante: firmware anterior prescindible

El propietario estableció el **17/10/2026** que las placas **COM4 Master (microSD presente)** y **COM14 Slave (sin microSD)** son equipos de desarrollo propios, sin necesidad de respaldar el sketch ni la flash completa antes de sobreescribirlos. Se acepta que el firmware previo podría no ser recuperable. **No volver a exigir respaldo ni nueva renuncia por cada alpha** sobre estas unidades. Fuente: `A13_G10_R_PREH_B4_BANCO_RESPALDOS_SEGURIDAD_20261017.md`, sección H0, excepción para placas de desarrollo. Esta exención NO aplica a un PLC de cliente o en servicio.

## Secuencia

1. **N0 local sin acceso COM:** usar `tools/alpha13/h_f137_com4.py --selftest`, `--check --expected-head` y `--inventory --expected-head`, tras `git pull --ff-only` en la rama del PR #125. El inventario USB requiere CH340 `1A86:7523` en COM4, y si COM14 está conectado comprueba identidad USB distinta, sin abrir COM14. Confirmar presencialmente que COM4 es el Master; un VID/PID no prueba rol.
2. **Seguridad de banco:** router LAN aislado, salidas Q sin cargas ni tensión, fuente estable, monitor serie cerrado antes de cualquier upload. No usar GPIO4 ni cambiar pines/periféricos; no cambiar microSD ni contenido de COM14.
3. **Build y sellado futuro:** preparar binarios del sketch Master contra el package real del PR #125, exigir fuentes-archive-ELF, SHA de binarios, FQBN correcto y HEAD/árbol limpio. No usar ejecutor antiguo G10-PH0 que exige flashear COM14 antes.
4. **Upload COM4:** requiere aprobación explícita **de COM4**, recogida de manera independiente tras identidad y binarios sellados. Se permite sobreescribir firmware previo sin respaldo por la decisión vigente, sin `erase-all` ni cambios en COM14. Ninguna aprobación de N0 equivale a autorización de carga.
5. **Observación H acotada tras upload aprobado:** comprobar BOOT/HB estables y sin TG0/TG1/panic, PHY/ETH/TCP y DHCP en LAN aislada, FC03 read-only y UDP eco, progreso RS-485/RTU, TFT, RTC, I/O/TCA, SD y FRAM. Se propone ventana dirigida de 90–120 s, sin repetir R1 605 s/R2 3 ciclos/R4 1805 s ya acreditados. Una maniobra RJ45 sobre COM4 requiere aprobación separada.
6. **Basic Core:** conservar N2/ELF y política weak Basic/strong Core deshabilitada; no inferir hardware probado para Basic Core sin reflasheo específico y consentimiento independiente.

## Aborto y límites

Si cambian HEAD, SHA archives, identidad/rol USB, aparece reset inesperado, I/O anómala, fallas RTU, SPI/W5500 o no existe evidencia suficiente, detenerse en `REVIEW_STOP` y conservar logs sanitizados. E1/E2, G10 R3 interno, R4 largo y casos DNS/DHCP específicos siguen como limitaciones registradas; no reclasificarlos como PASS.

```text
H_F137_N0_CI=PASS_39545618484_EVIDENCE_OLD_RUNNER
H_F137_NO_BACKUP_DEV_BOARDS=DECISION_20261017
H_F137_INVENTORY_REVISED_CI=PENDING
H_F137_COM4_PHYSICAL=NOT_RUN
H_F137_COM4_UPLOAD=SEPARATE_EXPLICIT_AUTH_REQUIRED
H_F137_COM14=NO_UPLOAD
PR125=DRAFT_NO_MERGE
RELEASE=BLOCKED
NEXT_GATE=H_F137_COM4_IDENTITY_THEN_BUILD_SEAL_AND_UPLOAD_APPROVAL
```
