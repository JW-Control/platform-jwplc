# Alpha14 — auditoría TFT_eSPI externa — 2026-09-30

## Resultado

Los logs que muestran simultáneamente `JWPLC_TFT` y `TFT_eSPI 2.5.43` son
coherentes con el modo source-first de desarrollo actual. No indican doble
backend ni un archive TFT stale.

```text
JWPLC_TFT=SOURCE_FIRST_DEVELOPMENT
TFT_ESPI_ROLE=PRIVATE_SOURCE_BACKEND
TFT_ESPI_VERSION_OBSERVED=2.5.43
TFT_ESPI_SOURCE_OBJECT_COMPILED=YES
TFT_ESPI_OBJECT_LINKED=YES
TFT_RUNTIME_FAILURE=NO
TFT_ARCHITECTURE_CHANGE_IN_THIS_GATE=NO
```

## Por qué Arduino selecciona TFT_eSPI

`JWPLC_TFT/src/JWPLC_TFT.cpp` contiene:

```cpp
#include <TFT_eSPI.h>
```

Como `JWPLC_TFT` está sin `precompiled=full` durante desarrollo, Arduino
inspecciona y compila su source. La resolución de ese include selecciona la
instalación externa ubicada en:

```text
C:\Users\jeykc\Documentos\Programacion\Arduino\libraries\TFT_eSPI
```

La versión observada por Arduino es `2.5.43`.

El sketch no incluye TFT_eSPI directamente y la API pública de `JWPLC_TFT` no
expone tipos de ese backend. La dependencia sigue siendo privada desde el punto
de vista de la API JWPLC, aunque es una dependencia de compilación del modo
source-first.

## Objetos realmente compilados

En `compile_master.log` y `compile_slave.log` existe una orden de compilación
para:

```text
TFT_eSPI/TFT_eSPI.cpp -> TFT_eSPI.cpp.o
```

El objeto también aparece en el comando de link de ambos firmware. Por tanto:

```text
MASTER_TFT_ESPI_COMPILE_COMMAND_COUNT=1
SLAVE_TFT_ESPI_COMPILE_COMMAND_COUNT=1
MASTER_TFT_ESPI_LINKED=YES
SLAVE_TFT_ESPI_LINKED=YES
```

Esto confirma que no es sólo una línea informativa de resolución: se compila y
se consume código de la instalación externa.

## Instalación limpia

El árbol actual de `JWPLC/2.1.0/libraries` no contiene una librería
`TFT_eSPI`, y `JWPLC_TFT/library.properties` declara solamente:

```text
depends=SPI
```

Por ello, una instalación limpia del checkout **en el modo source-first de
desarrollo actual** no compilaría igual sin proporcionar TFT_eSPI 2.5.43.

Eso no invalida el modelo de release ya cualificado:

- el archive histórico `src/esp32/libJWPLC_TFT.a` incorpora el wrapper y el
  backend TFT_eSPI 2.5.43;
- al restaurar `precompiled=full` para una distribución release, el usuario no
  necesita la librería externa;
- durante source-first, el entorno de desarrollo sí debe fijar explícitamente
  TFT_eSPI 2.5.43 para que el build sea reproducible.

## Clasificación

```text
TFT_EXTERNAL_SELECTION=EXPECTED_FOR_CURRENT_SOURCE_MODE
TFT_SOURCE_FIRST_DEPENDS_ON_EXTERNAL_TFT_ESPI=YES
TFT_CLEAN_SOURCE_BUILD_WITHOUT_TFT_ESPI=NO
TFT_PRECOMPILED_RELEASE_SELF_CONTAINED=YES
TFT_H3ER_PRODUCT_RESULT=PASS
TFT_RELEASE_PRECONDITION=RESTORE_PRECOMPILED_POLICY_OR_PACKAGE_PINNED_BACKEND
```

La falta de una dependencia declarada/pinneada es una precondición pendiente
de reproducibilidad para desarrollo o promoción release; no fue un fallo del
producto, hardware ni entorno en H3E-R post-P4.1, porque el entorno de esa
corrida sí tenía 2.5.43 y el runtime TFT físico pasó en Master y Slave.
