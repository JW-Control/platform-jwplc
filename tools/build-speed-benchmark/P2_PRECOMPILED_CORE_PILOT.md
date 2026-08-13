# P2 - piloto de core precompilado

Fecha de preparacion: 2026-08-09

## Objetivo

Reducir el cold build del JWPLC Basic sin retirar perifericos del autoload y sin cambiar APIs.

El baseline P1 limpio aun ejecuta aproximadamente 97 compilaciones. De ellas, 64 corresponden al core `jwcontrol`.

## Decision de seguridad

No se reutiliza un unico core.a entre JWPLC Basic y JWPLC Basic Core.

Aunque ambos usan ESP32 y el mismo directorio fuente `cores/jwcontrol`, actualmente el core se compila con propiedades de placa distintas, entre ellas:

- `ARDUINO_BOARD`;
- `ARDUINO_FQBN`;
- `JWPLC_HAS_FRAM`;
- `JWPLC_HAS_SD`;
- `JWPLC_HAS_ETHERNET`;
- `JWPLC_FRAM_SIZE_BYTES`.

Ademas, el core incluye unidades propias JWPLC como `jwplc_peripherals.cpp` y `peripherals_init.cpp`.

Por estabilidad, P2 genera un `core.a` por perfil:

```text
precompiled/core/JWPLCBASIC/core.a
precompiled/core/JWPLCBASICCORE/core.a
```

## Mecanismo piloto

1. `Build-JWPLCPrecompiledCore.ps1` desactiva cualquier overlay P2 previo.
2. Compila el target con `build.core=jwcontrol` y `--clean`.
3. Copia el `build/core/core.a` resultante al area `precompiled/core/<BUILD_BOARD>/`.
4. Genera un bloque controlado en `boards.local.txt` para cambiar el target seleccionado a:

```text
build.core=jwcontrol_p2
```

5. `cores/jwcontrol_p2` contiene solamente un stub C pequeno.
6. `platform.local.txt` mantiene `cores/jwcontrol` en el include path para conservar los headers publicos normales.
7. El hook `recipe.hooks.core.postbuild.2` sustituye el archive del stub por el `core.a` precompilado del perfil.
8. Se ejecuta un segundo build limpio y se comparan tiempos, numero de compilaciones, bytes y SHA-256 del app `.bin`.

## Criterio esperado

Para Basic se espera aproximadamente:

```text
P1 fuente: ~97 compilaciones
P2:        ~34 compilaciones
```

porque las 64 unidades del core fuente deben quedar sustituidas por una unica compilacion del stub.

El tiempo real es el dato que debe decidir si P2 merece entrar a Alpha4.

## Rollback

Ejecutar:

```powershell
.\Remove-JWPLCPrecompiledCore.ps1
```

Esto elimina solo el bloque P2 de `boards.local.txt` y restaura el `build.core=jwcontrol` definido en `boards.txt`.

Los archives se conservan inactivos por defecto. Para borrarlos tambien:

```powershell
.\Remove-JWPLCPrecompiledCore.ps1 -DeleteArchives
```

## Reglas antes de aceptar P2

- validar `01_empty`;
- validar `03_autoload_contract`;
- validar JWPLC Basic fisico;
- generar y validar archive separado para Basic Core;
- comprobar tamanos/mapa;
- decidir si los binarios precompilados se publican dentro del package;
- documentar procedimiento reproducible para regenerarlos;
- no tocar FlashFreq ni declarar bootloader definitivo como parte de P2.
