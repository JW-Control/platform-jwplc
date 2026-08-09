# P3 - JWPLC_Display precompilado

## Objetivo

Reducir el costo cold restante despues de D1 + P1 + P2, atacando especialmente las pasadas repetidas de preprocesamiento observadas sobre `JWPLC_Display.cpp`.

Referencia P2 validada en PC principal:

- cold P2: 104.223 s
- compilaciones: 34
- core fuente: 0
- core stub: 1
- preprocesados `g++ -E`: 51
- de ellos, 17 pasadas sobre `JWPLC_Display.cpp`

## Estrategia

`JWPLC_Display` declara `precompiled=full` y usa un archive compatible con Arduino:

`JWPLC/2.1.0/libraries/JWPLC_Display/src/esp32/libJWPLC_Display.a`

Durante el piloto, `Build-JWPLCPrecompiledDisplay.ps1` no recompila Display para fabricar el archive. Reutiliza los dos objetos ya producidos por un build P2 validado:

- `JWPLC_Display.cpp.o`
- `JWPLC_IdleScreen.cpp.o`

Luego crea `libJWPLC_Display.a` con el mismo `xtensa-esp32-elf-gcc-ar` del toolchain y ejecuta un cold build limpio P2 + P3.

## Modelo previsto para Alpha4 final

Los `.o` son artefactos intermedios del proceso de release, no artefactos que deban regenerarse en cada compilacion del usuario.

El package final debe contener los archives preconstruidos ya validados:

- `core.a` por perfil de placa cuando sea necesario.
- `lib*.a` para librerias JWPLC precompiladas aprobadas.

En cada compilacion/subida normal Arduino enlaza esos archives; no vuelve a generar los `.o` correspondientes.

Los archives se regeneran solamente cuando cambia una entrada que pueda afectar el binario, por ejemplo:

- fuente de la libreria/core;
- toolchain ESP32;
- flags de compilacion relevantes;
- configuracion de placa/perfil que quede horneada en el binario;
- ABI/dependencias que obliguen a reconstruir.

Los fuentes se conservan como referencia y fallback. Con `precompiled=full`, si falta el archive compatible Arduino vuelve a compilar desde fuentes.

## Alcance y cautela Basic/Core

P3 se valida primero con JWPLC Basic. El formato estandar `src/esp32/libJWPLC_Display.a` selecciona por MCU, no por `build.board`.

Antes de declarar este archive comun tambien para Basic Core se debe comprobar que los objetos de `JWPLC_Display` sean equivalentes entre ambos perfiles o, si no lo son, separar la parte dependiente de perfil o aplicar un mecanismo por perfil similar al P2 del core.

No se asumira compatibilidad Basic/Core solo porque ambos usan ESP32.

## Criterios de aprobacion P3

- `JWPLC_Display.cpp` y `JWPLC_IdleScreen.cpp` no se compilan desde fuente en el cold de verificacion.
- El core P2 sigue usando exactamente un stub y cero fuentes `jwcontrol`.
- El tamano de la aplicacion se mantiene.
- El payload fuera de metadatos/hash permanece equivalente al build P2 de referencia.
- Se registra el numero total de compilaciones y pasadas `g++ -E`.
- Se cuantifica especificamente cuantas pasadas `g++ -E` quedan sobre `JWPLC_Display.cpp`.

## Rollback

Ejecutar:

`Remove-JWPLCPrecompiledDisplay.ps1`

Esto elimina solamente `libJWPLC_Display.a`. `precompiled=full` permanece y Arduino vuelve automaticamente al fuente.
