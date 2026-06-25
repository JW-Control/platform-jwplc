# Alpha31 - rescate limpio desde PR #27

Este documento registra la decisión de cerrar el PR #27 sin merge directo y reemplazarlo por un PR limpio desde `main`.

## Motivo

El PR #27 (`develop/alpha31-release-readiness -> main`) no era mergeable desde la web y mezclaba una rama antigua de release con `main` actual.

## Alcance rescatado

Se rescatan solamente archivos de texto revisables:

- `JW_MatrixButtons` 1.0.5 dentro del snapshot del package.
- Documentación Alpha31 en Markdown.

## Alcance omitido

Se omite el archivo binario:

```txt
docs/jwplc_alpha31_release_docs.zip
```

Motivo: mantener el PR revisable y evitar binarios duplicados dentro de la rama principal.

## Nota de revisión

El PR limpio debe revisarse como incorporación selectiva de contenido Alpha31, no como merge completo de la rama `develop/alpha31-release-readiness`.
