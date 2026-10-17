# Alpha13 → Alpha15 — matriz de compatibilidad PRE-transferencia (read-only)

**Fecha:** 16/10/2026 (Perú) · **Repositorio:** `JW-Control/platform-jwplc`.

**Origen producto:** `v2.1.0-alpha.13/feature/cleanup-robustness` (fuentes de producto, **NO** overlays de laboratorio).  
**Destino:** `v2.1.0-alpha.15-HmiDesigner`, HEAD observado `97eec38320c6bcf55f73e741a44fd1beb090d591`.  
**Origen experimental:** PR [#108](https://github.com/JW-Control/platform-jwplc/pull/108) = G10-R **Draft**, `A13-006=ADOPTION_BLOCKED`.

## 1. Resultado de la comparación

GitHub Compare informó ramas **`diverged`** entre la rama producto Alpha13 y Alpha15; **no** son un simple fast-forward. La comparación arroja cientos de cambios cruzados, por lo que la decisión **no** se fundamenta en un merge general o en el contador total de archivos. Se contrastaron los **SHA de blobs exactos** de rutas críticas:

| Ruta en `JWPLC/2.1.0/` | SHA igual | Decisión preliminar |
| --- | :---: | --- |
| `platform.txt` | Sí | No requiere traslado por este archivo |
| `libraries/JWPLC_Ethernet/src/JWPLC_Ethernet.cpp` | Sí | Coincidente en ambos HEAD consultados |
| `libraries/JWPLC_Ethernet/src/DhcpAsyncControl.cpp` | Sí | Coincidente en ambos HEAD consultados |
| `libraries/JWPLC_Ethernet/src/Dns.cpp` | **No** | **Revisión semántica obligatoria**; Alpha15 ya contiene cambios |
| `libraries/JWPLC_Ethernet/src/EthernetClient.cpp` | **No** | **Revisión semántica obligatoria**; posible colisión con G10 |
| `libraries/JWPLC_Ethernet/src/EthernetUdp.cpp` | **No** | **Revisión semántica obligatoria**; posible colisión con G10 |
| `libraries/JWPLC_Display/src/JWPLC_Display_API.h` | Sí | API de display coincidente para este archivo |
| `libraries/JWPLC_Display/src/JWPLC_Display.cpp` | Sí | Coincidencia exacta |
| `libraries/JWPLC_ModbusRTU/src/JWPLC_ModbusRTU.h` | Sí | Header de RTU coincidente |
| `libraries/JWPLC_TFT/src/JWPLC_TFT.h` | **No** | **No sobrescribir** TFT de Alpha15 |
| `libraries/JWPLC_TFT/src/esp32/libJWPLC_TFT.a` | **No** | Archive compilado distinto; NO sustituir sin reconstrucción/aprobación |

**Alcance de esta tabla:** igualdad de SHA significa **igualdad exacta del blob comparado**, no compatibilidad general, test de ABI ni que otros archivos también coincidan. No se hizo una comparación semántica completa de los tres `.cpp` Ethernet distintos. El archive TFT se comparó por SHA de GitHub, sin inferir contenido ABI.

## 2. Reglas de transferencia

1. **No transferir** `tools/alpha13/`, `LabSocketSnapshot`, `LabEvent`, `labReadEvents()`, protocolos `G10R_*`, diagnósticos COM4/COM14 ni PR #105–#108 en bloque. El código G10 sigue sin adopción.
2. Para componentes G1–G9 **ya productivos**, levantar un inventario de commits y blobs aprobado, contrastar destino por archivo y crear un PR de integración **separado**, con revisión del responsable de Alpha15. No duplicar lo que ya coincide.
3. Cuando G10-R pase H/R5 y se apruebe `A13-006`, extraer un **patch productivo sin instrumentación**, sobre el HEAD actual de Alpha15. Los conflictos Ethernet/DNS/UDP/Cliente se resolverán semánticamente con tests de compatibilidad, no reemplazando archivos a ciegas.
4. **Mantener** el TFT y los archives de Alpha15 en su línea de desarrollo. Toda modificación de archive requiere procedencia compilada y gate Arduino IDE + enlace ABI.
5. Ejecutar una sola regresión integrada de **Arduino package + HMI Designer + TFT + Ethernet/Modbus** después de resolver las diferencias, y documentar SHA origen/destino/resultado. No asumir que el PASS host de G10-R valida el package instalado.

## 3. Estado / próxima acción

```text
A13_TO_A15_COMPAT=READ_ONLY_PARTIAL_DIFF_RECORDED
COMMON_BLOBS=PLATFORM_DISPLAY_API_DISPLAY_CPP_DHCP_ASYNC_MODBUSRTU_HEADER
DIVERGED_BLOBS=DNS_ETHERNETCLIENT_ETHERNETUDP_TFT_HEADER_TFT_ARCHIVE
A13_G10_PRODUCT_ADOPTION=BLOCKED
A15_BRANCH_MODIFIED=NO
NEXT=SEMANTIC_ETHERNET_DIFF_AND_APPROVED_PRODUCT_PATCH_ONLY_AFTER_G10_H
```

**Conclusión:** Alpha15 puede avanzar independientemente. El punto crítico de integración futura no es Display sino **DNS/EthernetClient/EthernetUdp** y la integridad del TFT 0.2.0 de Alpha15. Este documento solo autoriza preparación y comparación read-only, **no merge**.
