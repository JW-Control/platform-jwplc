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
