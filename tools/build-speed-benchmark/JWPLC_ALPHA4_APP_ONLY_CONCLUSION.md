# JWPLC v2.1.0-alpha.4 — conclusión sobre app-only

## Objetivo

Cerrar el pendiente de `app-only` para `v2.1.0-alpha.4` sin repetir una prueba de flasheo que ya fue validada sobre hardware en el ciclo anterior, y distinguiendo claramente qué parte del tiempo puede mejorar esta técnica.

## Evidencia histórica validada

Durante la etapa de optimización de `alpha30` se probó la carga **app-only** sobre JWPLC Basic.

Resultados preservados:

| Prueba | Tiempo |
|---|---:|
| Build incremental + upload full | 00:44.787 |
| Compile + app-only total | 00:43.050 |
| Compile dentro de compile + app-only | 00:36.335 |
| App-only upload dentro de esa prueba | 00:06.687 |
| App-only aislado | 00:06.352 |

La prueba confirmó que app-only escribe únicamente la aplicación en `0x10000`, suponiendo que bootloader, tabla de particiones y `boot_app0.bin` ya están correctamente grabados.

La diferencia observada entre `build incremental + upload full` y `compile + app-only` fue de aproximadamente **1.737 s**, equivalente a alrededor de **3.88 %** del tiempo total de aquella prueba. Por tanto, el ahorro existe, pero es pequeño frente al costo de compilación, discovery, link, generación de binarios y hooks.

## Verificación para alpha4

La rama actual de optimización se comparó contra `main`.

Resultado relevante:

- `JWPLC/2.1.0/platform.txt` **no aparece modificado** entre `main` y `v2.1.0-alpha.4/feature/build-speed-cache`.
- Las optimizaciones de alpha4 se concentran en discovery, precompilación de librerías/core, backend Ethernet y `platform.local.txt`.
- No se ha cambiado la receta base de upload full/app-only como parte de esta fase.

Por ello, la conclusión funcional histórica de app-only continúa siendo aplicable: alpha4 no introdujo un cambio en `platform.txt` que justifique repetir el ensayo de flasheo únicamente para volver a demostrar el mismo mecanismo.

## Relación con el resultado actual de compilación

El cold candidato final de P6 para JWPLC Basic es:

```txt
67.322 s
12 compiles
29 invocaciones g++ -E
```

El trabajo de alpha4 logró una reducción aproximada de **50.68 %** frente al baseline oficial histórico de `136.509 s` sin retirar periféricos del autoload normal.

Este resultado confirma la dirección correcta: el cuello principal estaba en el flujo de build, no en escribir nuevamente bootloader/particiones durante cada upload.

## Decisión alpha4

`app-only` queda **CERRADO** con la siguiente decisión:

- **Sí funciona** y es técnicamente útil para iteraciones de desarrollo.
- **No es la solución principal de rendimiento** del package.
- **No se requiere alterar `platform.txt`** en alpha4 para convertirlo en el mecanismo de upload por defecto.
- El upload full debe seguir siendo el camino normal/seguro para Arduino IDE y para primera programación, reinstalación, cambios de particiones, bootloader o configuración de flash.
- App-only puede mantenerse como **herramienta auxiliar de desarrollo** cuando se conoce que bootloader, particiones y `boot_app0.bin` ya son compatibles con la aplicación que se va a cargar.
- No se propone un menú público `UploadMode` en esta alpha; introducirlo ampliaría combinaciones de uso y requiere una validación UX/seguridad separada que no aporta al objetivo actual de cierre.

## Riesgo que evita esta decisión

Convertir app-only en upload normal podría dejar un equipo con una combinación incoherente de:

- aplicación nueva;
- bootloader antiguo;
- tabla de particiones antigua;
- `boot_app0.bin` antiguo;
- parámetros de flash no coincidentes.

Esto es especialmente importante mientras la configuración final de producto y la política de bootloader precompilado todavía se están cerrando explícitamente.

## Conclusión final

```txt
APP-ONLY: VALIDADO COMO HERRAMIENTA DE DESARROLLO.
NO ADOPTAR COMO UPLOAD NORMAL/POR DEFECTO EN v2.1.0-alpha.4.
NO REQUIERE NUEVO COLD NI NUEVA PRUEBA DE FLASH PARA CERRAR ESTE PENDIENTE,
YA QUE platform.txt NO FUE MODIFICADO EN ESTA FASE Y EXISTE EVIDENCIA FÍSICA PREVIA.
```

El siguiente pendiente de cierre recomendado es documentar la conclusión sobre **bootloader precompilado** y su relación con la configuración final de placa.