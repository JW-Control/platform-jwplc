# Alpha13 G10-P0 — Inventario estático source-backed previo a promoción

**Fecha:** 19/10/2026. **Base de producto:** `v2.1.0-alpha.13/feature/cleanup-robustness` HEAD `2b5802720df552e01a0b4f9d4fe1a7bd2de16daf`. **Origen:** lectura directa de `tools/alpha13/gates/a13_g10_integrated.py::compose()`, mapas `b1.FILES`, `s1.TARGETS`, `udp.UDP_PATH` y árbol Git de la base, sin ejecutar el compositor ni modificar fuentes.

**Resultado de inspección estática:** el diccionario `paths` del compositor funcional G10-I incluye **14 rutas destino**, de las que 12 existen actualmente dentro de `JWPLC/2.1.0` y 2 son archivos SafeAccess aportados por `tools/alpha13/candidates/g10_p1g_c/`. `Dns.cpp` y `Dhcp.cpp` se copian literalmente desde el package; por ello figuran en el overlay funcional aunque no se esperan diferencias de contenido. **El inventario de contenido modificado y los hashes dependen de ejecutar `compose()` sobre el HEAD aprobado; no afirmar 12 archivos efectivamente modificados solo por el total de 14 rutas.**

## 1. Inventario de destino

Todos los archivos listados están bajo `JWPLC/2.1.0/libraries/`.

| # | Archivo | Presente en producto | Clasificación inicial |
|---:|---|---|---|
| 1 | `JWPLC_Ethernet/src/JWPLC_W5x00_Ethernet.h` | Sí | Header Ethernet/UDP con campos internos |
| 2 | `JWPLC_Ethernet/src/EthernetClient.cpp` | Sí | Cliente TCP: ownership/lease/deferred |
| 3 | `JWPLC_Ethernet/src/EthernetServer.cpp` | Sí | Servidor TCP y copias de clientes |
| 4 | `JWPLC_Ethernet/src/EthernetUdp.cpp` | Sí | Ciclo UDP, cierre diferido |
| 5 | `JWPLC_Ethernet/src/socket.cpp` | Sí | Propiedad/estado socket y bus SPI |
| 6 | `JWPLC_Ethernet/src/JWPLC_Ethernet.cpp` | Sí | Servicio cooperativo y wrappers SPI |
| 7 | `JWPLC_Ethernet/src/Dns.cpp` | Sí | Identidad de fuente original, sin cambios previstos |
| 8 | `JWPLC_Ethernet/src/Dhcp.cpp` | Sí | Identidad de fuente original, sin cambios previstos |
| 9 | `JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.h` | No | Nuevo candidato de mutex/ownership |
| 10 | `JWPLC_Ethernet/src/JWPLC_W5500_SafeAccess.cpp` | No | Nueva implementación de mutex/ownership |
| 11 | `JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.cpp` | Sí | Modbus TCP servidor |
| 12 | `JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.cpp` | Sí | Modbus TCP cliente |
| 13 | `JWPLC_ModbusTCP/src/JWPLC_ModbusTCP.h` | Sí | Header de Modbus TCP servidor |
| 14 | `JWPLC_ModbusTCP/src/JWPLC_ModbusTCP_Client.h` | Sí | Header de Modbus TCP cliente |

**Fuera del compositor G10-I:** `JWPLC_Ethernet/src/utility/w5100.cpp`, protegido por F1, no debe sobrescribirse. El compositor de laboratorio **G10-R** usa `h0_w5100_stage`, `h0_init_stage` y `h0_link_stage` con checkpoints de RTC y centinelas; esos cambios no son código funcional de producto. No copiar la matriz completa de G10-R como si fuera G10-I.

## 2. Preflight obligatorio antes de promover

1. Resolver formalmente `H0_F1_EXTENDED_20_OF_20_STRICT=REVIEW_EVIDENCE_GAP` con las muestras del operador (sin reescribir logs). Hay 10+10 maniobras reportadas y 6 arranques RJ45 ON adicionales, pero cuatro capturas de la campaña principal no cumplen el criterio de cinco READY visibles.
2. Fijar HEAD de base en rama de trabajo aislada para G10; confirmar estado sin archivos sucios, PR documental H0 separado y no modificar Alpha15.
3. Ejecutar `compose()` **sin escrituras productivas**, en carpeta temporal de resultados; generar manifiesto de SHA-256 por ruta, diff exacto vs `JWPLC/2.1.0` y clasificación `UNCHANGED/MODIFIED/ADDED/LAB_ONLY`. Las 14 rutas actuales son solamente mapa de destino.
4. Asegurar que `w5100.cpp` conserva bytes/hashes del F1 que ya pasó COM4 y que `JWPLC_ETH_CS=5` y guarda GPIO6–11 siguen vigentes; sin cambios en `boards.txt`, `pins_is.h`, core ni variant.
5. Revisar dependencias internas SafeAccess, APIs públicas, mutex SPI no recursivo, lifecycle de sockets, Ethernet TCP/UDP y Modbus TCP antes de copiar fuentes al package.
6. Registrar evidencia reutilizable de N0/N1, Arduino N2/N3 source-first de ambos FQBN y 11 sketches, junto a los cinco archives precompilados; rechazar cambios a autoload, GPIO4 o archivos congelados.
7. Solicitar autorización independiente de R1–R4 físicos G10; la autorización de EN/USB de H0 no es consentimiento de subir o ensayar G10.

## 3. Estado final de este documento

```text
G10_P0_STATIC_SOURCE_INVENTORY=PASS_14_DESTINATIONS
G10_P0_EXISTING_PRODUCT_PATHS=12
G10_P0_CANDIDATE_SAFEACCESS_PATHS=2
G10_P0_DNS_DHCP=SOURCE_UNCHANGED_BY_COMPOSITOR
G10_P0_DYNAMIC_COMPOSE_RUN=NOT_RUN
G10_P0_EXACT_DIFF_SHA256=NOT_GENERATED
G10_P0_PRODUCT_FILES_CHANGED=0
G10_P0_PHYSICAL_R1_R4=NOT_RUN_NOT_AUTHORIZED
H0_F1_EXTENDED_STRICT=REVIEW_EVIDENCE_GAP
NEXT_GATE=H0_ACCEPTANCE_DECISION_THEN_G10_COMPOSE_DIFF_NONPHYSICAL
```
