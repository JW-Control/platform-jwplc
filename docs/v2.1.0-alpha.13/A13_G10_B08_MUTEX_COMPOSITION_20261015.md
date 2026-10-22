# Alpha13 G10-I B08 — regresión integrada de propiedad SPI

**Estado del candidato:** `B08=N1_N2_PASS_N3_PENDING`. **Producto:** no modificado; no implica adopción.  
**Rama:** `codex/alpha13-g10-b03-nightly`. **PR:** [#105](https://github.com/JW-Control/platform-jwplc/pull/105).  
**Fecha:** 15 de octubre de 2026 (Lima; CI continuada UTC 16/10).

## Defecto source-backed y corrección mínima

El core `jwplc_spi_bus.cpp` usa `xSemaphoreCreateMutex()`, **no recursivo**. La capa experimental `JWPLC_W5500_SafeAccess.cpp` permite nesting **solo** cuando el task ya adquirió el bus a través de su propio `enter()`. Nunca debe heredar una adquisición previa de SD/FRAM/Display en el mismo task.

El gate S1 (`a13_g10_p1g_c2_first_real_spi_slice.py`) ya transformaba `acquireBus/releaseBus` a `jwplc_w5500_candidate::enter/leave` en Ethernet, Modbus TCP Client y Server. Sin embargo, `a13_g10_integrated.py::compose()` releía `JWPLC_Ethernet.cpp` de `ROOT` para insertar el tick B03, **sobrescribiendo los wrappers del runtime Ethernet**. Los wrappers Modbus TCP existentes en S1 no eran la causa. Se corrigió el integrador para usar el texto ya compuesto (`paths[service_rel]`) y conservar todos los cambios previos. No se tocó firmware productivo, GPIO4, autoload ni archives.

## Evidencias no físicas

| Nivel | CI y comprobación | Resultado |
| --- | --- | --- |
| N0 | Candidato en rama aislada, sin cambios productivos; inventario de 5 archives congelados | PASS en integrador |
| N1 Ubuntu 24.04 y Windows | [Run 38842899887](https://github.com/JW-Control/platform-jwplc/actions/runs/38842899887): 3 wrappers `acquireBus`, 3 `releaseBus` **extraídos del source compuesto**, mutex host no recursivo, nesting + READ/CLOSE, owner, timeout, 6 mutaciones negativas | PASS |
| N2 Arduino selectivo | Mismo run: 4 sketches baseline + 5 candidato con bootstrap inmutable, wrappers en plataforma staged, source-first `.d`/objeto/ELF | PASS |
| N3 package Alpha13 completo | Matriz `11 baseline + 11 candidato` y autoload normal en CI; correr **solo sobre HEAD final marcado** | PENDIENTE |
| H físico | Reset W5500, TCP/Modbus real, DNS/DHCP/UDP, fairness con TFT/SD/FRAM/RTC, RS-485/Modbus RTU, botonera y TCA/I/O | NO EJECUTADO |

Las dos primeras iteraciones B08 tuvieron fallo de harness/composición y no se cuentan como PASS: `B08_ACQUIRE_SOURCE_DRIFT` (parche duplicado) y `B08_ACQUIRE_WRAPPER_NOT_COMPOSED` (sobrescritura runtime). El tercer intento pasó con el cambio source-backed acotado. No repetir N1/N2 mientras sus entradas permanezcan idénticas.

## Criterios pendientes y límites

- Un `N3=PASS` demostrará únicamente compilación/enlace del package aislado, procedencia y cobertura representativa de los sketches; **no** comportamiento de red ni ausencia de starvation SPI.
- Contrato B01 legacy escalar continúa optativo/limitado; `BusBusy` no debe tratarse como `CLOSED` en consumidores críticos.
- `A13-006=ADOPTION_BLOCKED`; `G10-H=NOT_STARTED`; `G10-C=NOT_STARTED`. Sin merge, publicación, cambio de GPIO4 ni upload.
- Actualizar `ALPHA13_STATUS.md` de la rama base **al aprobar e integrar** el PR, nunca confundir un candidato aislado con producto adoptado.

**NEXT_GATE:** `G10-I-N3_FULL_PLATFORM_REVIEW`; luego revisión de evidencia/definición de G10-H y autorización física independiente.
