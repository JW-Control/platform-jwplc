# Notas legales y comerciales - OpenPLC Editor JWPLC Edition

> Nota: este documento no reemplaza una revisión legal formal. Sirve como criterio técnico/comercial para preparar contratos y entregables.

## Contexto

JW Control planea mantener un fork/build propio de OpenPLC Editor para aceptar paquetes `.vpp` firmados por JW Control mediante una llave pública `jwcontrol-2026`.

## Firma OpenPLC existente

La llave `openplc-2026` es una llave pública confiable incluida en OpenPLC Editor stock. No se debe solicitar, copiar ni intentar usar su llave privada.

Usar OpenPLC Editor oficial con paquetes firmados por OpenPLC no implica que JW Control posea la firma `openplc-2026`.

## Firma JW Control

La llave `jwcontrol-2026` debe representar únicamente confianza sobre paquetes mantenidos por JW Control.

Reglas:

- la llave privada no se sube al repositorio;
- se guarda en almacenamiento seguro;
- se rota si hay sospecha de filtración;
- se documenta qué paquetes fueron firmados con esa llave.

## Contratos con clientes

En contratos, propuestas y manuales evitar frases como:

```txt
OpenPLC oficial certificado por JW Control
OpenPLC aprobado oficialmente por OpenPLC para JWPLC
Firma oficial OpenPLC para JWPLC
```

Usar frases como:

```txt
Build de OpenPLC Editor adaptado por JW Control para JWPLC Basic.
Fork/build mantenido por JW Control basado en OpenPLC Editor.
Paquetes VPP JWPLC firmados por JW Control.
OpenPLC es un proyecto de terceros; JW Control mantiene la integración específica con JWPLC Basic.
```

## Implicancia comercial recomendada

Para clientes finales, entregar claramente:

- versión de OpenPLC Editor usada como base;
- commit/tag upstream usado;
- lista de cambios JW Control;
- licencia upstream aplicable;
- link al código fuente del fork si la licencia lo exige;
- manual de instalación;
- alcance de soporte: JW Control soporta el build JWPLC y la integración con JWPLC Basic, no necesariamente todo OpenPLC upstream.

## Riesgos

- Confusión de marca si no se diferencia de OpenPLC oficial.
- Obligaciones de licencia si se distribuye un binario modificado.
- Riesgo supply-chain si se filtra la llave privada JW Control.
- Necesidad de mantener el fork actualizado con parches de seguridad upstream.

## Decisión recomendada

Usar el nombre:

```txt
OpenPLC Editor - JWPLC Edition
```

Y acompañarlo siempre con la aclaración:

```txt
Build mantenido por JW Control, basado en OpenPLC Editor, para integración con JWPLC Basic.
```
