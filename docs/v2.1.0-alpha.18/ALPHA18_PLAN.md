# v2.1.0-alpha.18 — JWPLC Sim: plan

Línea `JWPLC-Sim`, tercera vuelta: **OpenPLC Editor y JWPLC Sim trabajan juntos** y el HMI enlaza todos sus
objetos a variables PLC. Abierta el 2026-11-17, después de publicar 2.1.0-alpha.17.

```text
LINEA=JWPLC-Sim
RAMA=v2.1.0-alpha.18/JWPLC-Sim/main (platform y openplc-editor, desde release/v2.1.x)
  platform-jwplc  @ 29632e4 (2.1.0-alpha.17 + índices)
  openplc-editor  @ f81969c (4.3.3-jwplc.2)
PUBLICA_COMO=2.1.0-alpha.18 / editor 4.3.3-jwplc.3
UPSTREAM=4.3.4 y 4.3.5 de Autonomy quedan para alpha.19 (decisión del 17/11)
```

## Hitos

| Hito | Resultado | Criterio de cierre | Repos |
|---|---|---|---|
| H0 Apertura | Plan y estado de la línea; ramas `…/main` en platform y editor | Este PR fusionado | platform-jwplc |
| H1 Editor ↔ Sim | Botón «Simular en JWPLC Sim» (`--jwsim-open`), `--open-project` desde el Sim y `--jwsim-close` | Checklist manual H1 en PASS (§ Pruebas) | openplc-editor, jwplc-sim |
| H2 HMI completo en OpenPLC | Canales 2/3 del TREND y pisos independientes de la Baliza enlazados; Guardar sin reescribir el `.jwhmi` | Checklist manual H2 en PASS con el Designer de alpha.17 (`3f29398`) | openplc-editor |
| H3 Modbus TCP del JWPLC Basic (firmware OpenPLC) | Enlace tardío del W5500 y peticiones en varios segmentos TCP | Prueba en placa real (§ Pruebas) | openplc-editor |
| H4 Cierre | Editor 4.3.3-jwplc.3; `PLATFORM_DESIGNER_SHA`/`PLATFORM_VPP_SHA` alcanzables desde `…/main`; nota de release | Pre-release del editor y Release de platform | openplc-editor, platform-jwplc |

## Pruebas manuales

**H1** (editor con el Sim instalado):

- [x] Con un proyecto abierto, «Simular en JWPLC Sim» abre el Sim instalado (k23n0, 17/11).
- [ ] Sin el Sim: diálogo «Localizar JWPLC Sim»; la ruta elegida se recuerda.
- [ ] Proyecto sin `project.json`: aviso, no abre nada.
- [x] Sim → editor con el editor cerrado: arranca con el proyecto abierto (e2e `--open-project`, 17/11).
- [x] Sim → editor con el editor abierto: cambia de proyecto por second-instance (e2e, 17/11; sin cambios pendientes).
- [x] Cerrar el Sim (`--jwsim-close`) cierra el editor tras «¿Seguro que quieres salir?» (e2e, 17/11). Preguntas
  abiertas: ¿debe preguntar aun sin cambios? ¿también si el editor se abrió a mano?

**H2** (proyecto JWPLC Basic con HMI):

- [ ] Guardar sin cambios no reescribe `hmi/*.jwhmi`.
- [ ] Guardar con cambios en «Editar diseño…» escribe y recarga la vista de enlaces.
- [ ] Guardar mientras el Designer carga no da «No se pudo guardar el diseño del HMI».
- [ ] TREND con canal 2/3 enlazado: mensaje `HMI: <var> <- <VAR_PLC>` al compilar y curva correcta.
- [ ] Baliza independiente: un enlace por piso; un piso sin enlace no da error.

**H3** (JWPLC Basic con W5500):

- [ ] Cable conectado al arrancar: el 502 responde (regresión).
- [ ] Cable conectado después del arranque: el 502 responde sin reiniciar.
- [ ] Reinicio del W5500 o reconexión: vuelve a responder.
- [ ] Descubrimiento tras enlace tardío.
- [ ] Petición en dos segmentos TCP: se responde.
- [ ] Otra placa sin `MBETH_JWPLC`: sin cambios.

## Reglas que valen aquí

- Lo del editor va en la misma rama de línea en los dos repos; features con PR hacia `…/main`.
- Si cambia el Designer o el VPP, el SHA fijado en el editor se actualiza en el mismo hito.
- Validación sin hardware del editor: `tsc --noEmit`, Prettier, ESLint, Jest y `npm run build`.
