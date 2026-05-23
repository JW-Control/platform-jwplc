# v2.0.0 - Política de canales Boards Manager

## Objetivo

Evitar que usuarios finales vean todas las alphas históricas en Arduino Boards Manager.

---

## Canal público

Archivo:

```txt
JWPLC/package_jwplc_index.json
```

URL:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index.json
```

Uso:

```txt
Usuarios finales.
```

Contenido recomendado desde `v2.0.0`:

```txt
2.0.0
```

Opcionalmente:

```txt
betas públicas futuras
RCs públicos futuros
releases estables futuras
```

No recomendado:

```txt
todas las alphas históricas
```

---

## Canal dev / interno

Archivo:

```txt
JWPLC/package_jwplc_index_dev.json
```

URL:

```txt
https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/JWPLC/package_jwplc_index_dev.json
```

Uso:

```txt
Validación interna y desarrollo.
```

Contenido recomendado:

```txt
alphas
betas
RCs
releases estables
```

---

## Política de publicación futura

Para próximos ciclos:

```txt
alphas técnicas -> última alpha de verificación -> release estable
```

Beta es opcional:

```txt
Usar beta solo si se requiere validación pública o de terceros.
```

RC es opcional:

```txt
Usar RC solo si se requiere un candidato final adicional antes de estable.
```

---

## Motivo

Durante el ciclo de `v2.0.0`, `alpha31` ya actuó como validación técnica integral. `beta1` fue publicada como package validation, pero para ciclos futuros puede evitarse una beta si la última alpha de verificación cumple el mismo nivel de validación:

- instalación limpia;
- IDE;
- CLI;
- subida;
- otra PC;
- periféricos;
- ejemplos principales;
- index JSON;
- release ZIP.
