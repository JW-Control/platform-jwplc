# Alpha12 — Topología final release/main

Fecha: 2026-10-05

## Contexto

Las ramas `release/v2.1.x` y `main` conservan historias distintas debido a
squash merges históricos. Por ello Alpha11 ya estableció que la sincronización
final se evalúa por paridad exacta de árbol/contenido.

El PR directo:

```text
#102 release/v2.1.x -> main
```

fue cerrado al resultar conflictivo por divergencia histórica.

## Sincronización dirigida

Se creó un commit sintético sobre `main` apuntando exactamente al tree final de
`release/v2.1.x`:

```text
MAIN_BASE=4fcfde4132967c1ab3f75623e012f4cd5c6a6d14
RELEASE_HEAD_BEFORE_SYNC=30397a09aa07c3837426253d03dc9180de92efd7
RELEASE_TREE=e8d7caf91ef2ac475513f28131255db02ca7c7d2

SYNC_COMMIT=17834103bca24c9c9cb52b23002d0144b0a305ed
SYNC_TREE=e8d7caf91ef2ac475513f28131255db02ca7c7d2
TREE_MATCH=True
```

PR:

```text
#103 sync(alpha12): sincronizar árbol final publicado hacia main
MERGED=YES
MAIN_HEAD_AFTER_SYNC=c40be967ec9b948cc6cc8c497b407d1acf849c8d
```

## Verificación

Después del merge:

```text
MAIN_TREE=e8d7caf91ef2ac475513f28131255db02ca7c7d2
RELEASE_TREE=e8d7caf91ef2ac475513f28131255db02ca7c7d2
TREE_PARITY=True
```

## Conclusión

```text
SYNC_DIRECTION=release/v2.1.x -> main
MAIN_TO_RELEASE_REVERSE_SYNC=NO
GIT_ANCESTRY_PARITY=NOT_REQUIRED_BY_HISTORICAL_SQUASH_MODEL
TREE_PARITY_CRITERION=PASS
ALPHA12_RELEASE_MAIN_TOPOLOGY=PASS
```
