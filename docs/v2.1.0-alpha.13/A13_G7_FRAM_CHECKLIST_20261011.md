## Actualización G7C canónico — CLOSED_PASS (2026-10-11)

- [x] A13-009 corregido en `readString()`: diferencia SPI fallido de vacío válido y comprueba reserva de memoria.
- [x] A13-013: cleanup exclusivo de guards muertos y debug privado, sin romper API pública.
- [x] 11/11 casos C++ nativos PASS y CI tras commit PASS.
- [x] Objeto recompilado, paridad archive y enlace real `-L/-lJW_FRAM` comprobados.
- [x] Adopción local productiva y 2/2 regresiones con `precompiled=full`: PASS.
- [x] Tres archivos productivos exactos versionados y push remoto: `f2a93a8e5cbb736bbaa98ded9c7d2845582b7b32`.
- [x] Cierre: [A13_G7C_FRAM_CLOSURE_20261011.md](A13_G7C_FRAM_CLOSURE_20261011.md).
- [ ] G8, G9, G10 y publicación Alpha13 (hitos independientes).
- [ ] G10: decisión de aceptación/seguimiento F120, causa no determinada, y pruebas runtime adicionales si riesgo lo exige.

Los checkboxes históricos no marcados que aparecen más abajo se conservaron
para explicar la conciliación posterior de G7; **quedan resueltos por G7C**
en la rama Alpha13, no en una PreRelease aún distribuida.

---

# Alpha13-G7 — Checklist FRAM
- [x] CLI oficial 1.5.1 y core local comprobados.
- [x] Fuentes compiladas sin archive.
- [x] Archive .a regenerado y miembro igual al objeto.
- [x] Enlace precompiled=full del candidato verificado.
- [x] FRAM en banco, ventana respaldada en NVS.
- [x] Lectura/escritura, checksum inválido y punteros nulos comprobados.
- [x] FRAM restaurada y verificada.
- [x] Regresiones normales source/archive y hashes finales.
- [x] Commits/push no-force automáticos tras PASS.
- [ ] Contrastar atribución A13-009/A13-013 con auditoría canónica.
- [ ] G8 y gates restantes antes del freeze Alpha13.

## Conciliación posterior con auditoría original

- [ ] A13-009: corregir `readString()` cuando falla leer longitud, y validar cadena vacía legítima/payload/reserva.
- [ ] A13-013: eliminar únicamente guards imposibles y las dos sobrecargas privadas sin consumidores; reconstruir .a y comprobar símbolos.
- [x] Conservar G7 como cierre técnico trazable (checksums y null guards).
