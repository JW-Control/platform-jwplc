# Alpha30 - Datos base y prueba app-only

## Datos recibidos

Con `JWPLC_IO_BlockMirror` y FQBN:

```txt
jwplc:esp32:jwplcbasic
```

se obtuvo:

| Prueba | Resultado |
|---|---:|
| Build limpio sin upload | 02:38.218 |
| Build incremental sin cambios, sin upload | 00:48.444 |
| Build incremental + upload full | 00:44.787 |

Memoria reportada:

| Recurso | Uso |
|---|---:|
| Flash | 414521 bytes / 31% |
| RAM global | 27620 bytes / 8% |

## Lectura de resultados

El upload full no es el cuello principal.

En la prueba de upload full, el programa subió:

| Región | Dirección | Tamaño comprimido / real | Tiempo |
|---|---:|---:|---:|
| Bootloader | 0x1000 | 16018 / 25024 bytes | 0.7 s |
| Partitions | 0x8000 | 146 / 3072 bytes | 0.1 s |
| boot_app0 | 0xe000 | 47 / 8192 bytes | 0.1 s |
| Aplicación | 0x10000 | 235480 / 414672 bytes | 3.9 s |

Conclusión preliminar:

```txt
App-only upload puede ahorrar poco en escritura flash directa,
quizá 1-3 s, pero no va a bajar de 45 s a 15 s por sí solo.
```

El tiempo fuerte está antes del flasheo: compile/check/link/objcopy/hooks.

## Siguiente prueba

Ejecutar:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev\alpha30_compile_then_app_only.ps1 -Port COM14
```

Luego comparar contra:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev\alpha30_measure_build.ps1 -Upload -Port COM14
```

## Prueba app-only aislada

Primero debe existir un build previo en:

```txt
C:\JWPLC_Build\jwplcbasic
```

Luego ejecutar:

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\dev\alpha30_app_only_upload.ps1 -Port COM14
```

Esto mide solo la carga de aplicación, sin compilar.

## Resultado esperado

Si app-only funciona correctamente:

- La placa debe reiniciar y ejecutar el sketch.
- No debe reescribir bootloader.
- No debe reescribir partitions.
- No debe reescribir boot_app0.
- Solo debe escribir en `0x10000`.

Si el tiempo total no mejora casi nada, entonces app-only queda como opción útil pero no prioritaria para acelerar compilación.
