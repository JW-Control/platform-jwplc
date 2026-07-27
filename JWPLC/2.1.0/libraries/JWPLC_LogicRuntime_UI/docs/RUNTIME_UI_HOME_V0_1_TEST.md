# Resultado físico — Runtime UI Home v0.1

## Estado

```text
DISEÑO VISUAL: VALIDADO EN TFT
DIRTY RENDERING: VALIDADO EN TFT
PARPADEO PERIÓDICO: ELIMINADO
BOTONERA Y RETORNO A IDLE: VALIDADOS
```

## Compilación validada

```text
Flash:       428465 bytes / 3145728 bytes (13 %)
RAM global:   32724 bytes / 327680 bytes (9 %)
RAM restante: 294956 bytes
```

## Ejemplo

```text
JWPLC_LogicRuntime_UI_Home
```

## Seguridad confirmada

- inicializa la E/S base mediante `runtime.begin()`;
- no carga ningún programa lógico;
- no contiene bloques `DigitalOutput`;
- no escribe ni formatea la FRAM;
- `storage().begin()` solo inspecciona el mapa existente;
- Q0 permanece apagado.

## Elementos visuales confirmados

- encabezado `JWPLC LOGIC`;
- insignia `READY`;
- panel de estado del programa;
- botones `PROGRAMA`, `BLOQUES`, `MEMORIA` y `DIAGNOSTICO`;
- selector visible;
- pie de navegación;
- retorno `USER -> IDLE`;
- reconstrucción correcta al entrar nuevamente en USER.

## Renderizado incremental validado

La primera versión visible presentaba parpadeo porque los campos dinámicos se borraban y reescribían aunque su valor permaneciera estable.

La versión corregida fue validada con:

```text
estructura estática una sola vez
+ caché de valores dinámicos
+ actualización por diferencia
+ scan visual cada 1000 ms
+ redibujado solo de selección anterior y actual
```

Resultado observado:

```text
Título y marcos: estables
Programa e identidad: estables
Storage y retentivos: estables
Botones: estables
Insignia READY: estable
Movimiento del selector: sin parpadeo global
Mensaje de OK: solo modifica el pie
Segunda entrada a USER: una sola reconstrucción
```

## Decisión

El patrón de dirty rendering queda aprobado como base obligatoria para las pantallas posteriores del Runtime.

Documento normativo:

```text
docs/USER_UI_RENDERING_RULES.md
```

## Evolución posterior

Después de esta validación:

1. la paleta cian inicial fue reemplazada por tonos verdes, blancos y negros de JW Control;
2. se inició `RuntimeUIProgram` como primera sección funcional;
3. la nueva etapa requiere recompilación y validación independiente.