# Prueba física — Runtime UI Home v0.1

## Estado

```text
DISEÑO VISUAL INICIAL: VALIDADO EN TFT
DIRTY RENDERING: IMPLEMENTADO
RECOMPILACIÓN Y REVALIDACIÓN SIN PARPADEO: PENDIENTES
```

La primera visualización física confirmó correctamente:

- encabezado `JWPLC LOGIC`;
- insignia `READY`;
- panel de estado del programa;
- botones `PROGRAMA`, `BLOQUES`, `MEMORIA` y `DIAGNOSTICO`;
- selector visible;
- pie de navegación.

Durante esta primera visualización se detectó parpadeo periódico porque los campos dinámicos se borraban y reescribían aunque su valor permaneciera estable.

La implementación fue refactorizada para aplicar las reglas de:

```text
estructura estática una sola vez
+ caché de valores dinámicos
+ actualización por diferencia
+ scan visual cada 1000 ms
+ redibujado solo de selección anterior y actual
```

## Ejemplo

```text
JWPLC_LogicRuntime_UI_Home
```

## Seguridad

- inicializa la E/S base mediante `runtime.begin()`;
- no carga ningún programa lógico;
- no contiene bloques `DigitalOutput`;
- no escribe ni formatea la FRAM;
- `storage().begin()` solo inspecciona el mapa existente;
- Q0 debe permanecer apagado.

## Flujo esperado

1. El sketch inicia en la pantalla `IDLE` normal.
2. `RUN` debe quedar apagado porque el runtime está en `READY`.
3. `ERR` debe quedar apagado: una FRAM sin formato no es una falla crítica.
4. El monitor de entradas y salidas de `IDLE` debe continuar funcionando.
5. Al pulsar cualquier botón se entra en la vista `USER`.
6. Debe mostrarse el encabezado `JWPLC LOGIC` y el estado `READY`.
7. Sin programa persistente debe mostrarse:

```text
Programa: SIN PROGRAMA
ID / Gen: SIN IDENTIDAD
Bloques: 0
```

8. Las flechas deben mover el selector entre:

```text
PROGRAMA
BLOQUES
MEMORIA
DIAGNOSTICO
```

9. Al mover el selector, únicamente deben redibujarse el botón anterior y el nuevo.
10. `OK` debe mostrar durante aproximadamente 1.5 segundos la sección seleccionada.
11. `ESC` debe regresar a `IDLE`.
12. Una segunda entrada a `USER` debe reconstruir correctamente la vista.

## Validación específica de parpadeo

Mantener la pantalla en `USER` sin pulsar botones durante al menos 20 segundos.

Debe observarse:

```text
Título y marcos completamente estables
Programa e identidad completamente estables
Estado de storage y retentivos completamente estable
Botones completamente estables
Insignia READY completamente estable
```

El campo de scan se evalúa una vez por segundo, pero no debe escribirse en la TFT cuando promedio y máximo sigan sin cambios.

También se debe comprobar:

1. mover el selector repetidamente no provoca destello en los otros dos botones;
2. pulsar `OK` solo modifica el pie de pantalla;
3. al desaparecer el mensaje de `OK`, solo se restaura el pie;
4. entrar nuevamente a `USER` produce una única reconstrucción, no dos destellos consecutivos.

## Monitor serial esperado

```text
JWPLC Logic Runtime UI - pantalla principal USER
IDLE conserva monitor I/O y LEDs del package.
Pulsa cualquier boton para entrar a USER.

Storage: OK
Runtime: OK
Runtime UI: OK

En USER: flechas mueven el selector, OK confirma y ESC vuelve a IDLE.
JWPLC_Display inicializado
```

El orden exacto de `JWPLC_Display inicializado` puede variar porque el display pertenece al autoload normal del package.

## Datos que deben registrarse

```text
Flash utilizada
RAM global utilizada
RAM restante
fotografía de IDLE
fotografía de USER
resultado de cada botón
retorno USER → IDLE
parpadeo con pantalla estable durante 20 s: SI/NO
parpadeo al mover selector: SI/NO
parpadeo al mostrar/restaurar mensaje de OK: SI/NO
segunda entrada USER con un solo redibujado: SI/NO
```

## Criterio de aprobación

```text
COMPILA
IDLE SIN REGRESIÓN
USER VISIBLE
BOTONERA OPERATIVA
ESC RETORNA A IDLE
SIN PARPADEO PERIÓDICO
SOLO SE ACTUALIZAN CAMPOS MODIFICADOS
FRAM SIN ESCRITURAS
Q0 APAGADAS
```

## Documento normativo

Las reglas que deben seguir todas las pantallas posteriores están en:

```text
docs/USER_UI_RENDERING_RULES.md
```
