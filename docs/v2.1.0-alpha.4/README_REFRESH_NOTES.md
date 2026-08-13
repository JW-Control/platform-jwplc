# Notas del refresh documental final de Alpha4

Este archivo registra el criterio usado para actualizar la documentación general después de publicar y validar `v2.1.0-alpha.4`.

## Objetivo

Evitar que el `README.md` principal continúe mezclando:

- estado estable `v2.0.0`;
- estado de desarrollo Alpha3;
- benchmarks históricos de alpha30;
- configuración `huge_app` ya reemplazada en JWPLC Basic;
- detalles de integración que cuentan con documentación específica propia.

## Criterio aplicado

El README principal se mantiene como documento de entrada al repositorio y resume el estado vigente.

Se conservaron o actualizaron:

- canal estable público `v2.0.0`;
- canal dev `v2.1.0-alpha.4`;
- instalación por Boards Manager / Arduino CLI;
- placas y FQBN;
- configuración final de JWPLC Basic para Alpha4;
- Max App de 4 MB;
- rendimiento formal de compilación;
- validación standalone posterior a publicación;
- dependencias de Boards Manager;
- librerías bundled/precompiladas y versiones observadas;
- periféricos y APIs principales;
- coexistencia SPI;
- Display/botonera;
- Ethernet;
- RS-485 / Modbus RTU;
- decisiones de app-only, bootloader y OTA;
- estado externo/opcional de OpenPLC;
- gates físicos de Alpha4.

El detalle histórico sigue disponible en:

```txt
tools/build-speed-benchmark/
docs/alpha32_openplc_integration/
docs/v2.1.0-alpha.4/
JWPLC/Test_Codes/
```

## Razón de la consolidación

El README anterior superponía información de distintas etapas y todavía indicaba, entre otros puntos:

- `v2.1.0-alpha.3` como estado de desarrollo actual;
- `huge_app` como partición de JWPLC Basic;
- máximo de aplicación de `3145728 bytes` para JWPLC Basic;
- tiempos preliminares anteriores al resultado P8;
- checklist de instalación que seguía esperando `huge_app`.

Esos valores ya no representan el estado de Alpha4.

La consolidación busca que el README responda primero a:

1. qué versión es estable;
2. cuál es la pre-release técnica vigente;
3. cómo instalarla;
4. cuál es la configuración real del board;
5. qué optimización de compilación se logró;
6. qué periféricos y dependencias siguen integrados;
7. qué fue validado físicamente.
