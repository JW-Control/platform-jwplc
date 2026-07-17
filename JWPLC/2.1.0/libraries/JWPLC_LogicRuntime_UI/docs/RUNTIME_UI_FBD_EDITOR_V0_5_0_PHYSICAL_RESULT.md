# Resultado físico — editor FBD v0.5.0

## Estado

```text
ENTRADA A USER: aprobada
MAPA FBD: aprobado
DETALLE GRÁFICO: aprobado
SELECCIÓN DE ENTRADAS CON UP/DOWN: aprobada
MANEJO DEL EDITOR: requiere ajuste
```

## Observaciones del usuario

La prueba física confirmó:

- `OK` abre correctamente el mapa desde la pantalla inicial;
- `OK` sobre `B04 AND` abre correctamente el detalle;
- `UP/DOWN` permite recorrer las entradas del bloque;
- la presentación gráfica del detalle es válida.

Se rechazó el flujo de interacción original:

```text
RIGHT → entrar a EDITAR IN
UP/DOWN → seleccionar campo
LEFT/RIGHT → modificar campo
```

## Decisión para v0.5.1

Adoptar una navegación jerárquica y consistente:

```text
MAPA FBD
  OK  → DETALLE
  ESC → salir de USER

DETALLE
  UP/DOWN → seleccionar entrada
  OK      → EDITAR IN
  ESC     → volver a MAPA FBD

EDITAR IN
  LEFT/RIGHT → seleccionar FUENTE o LÓGICA
  UP/DOWN    → cambiar el valor seleccionado
  OK         → validar y aplicar
  ESC        → cancelar y volver a DETALLE
```

Regla principal:

```text
ESC solo puede abandonar USER cuando el usuario está en MAPA FBD.
En niveles internos, ESC retrocede exactamente un nivel.
```

La compuerta de liberación debe mantenerse en todas las transiciones para impedir que un pulso o repetición pendiente se aplique en la pantalla siguiente.
