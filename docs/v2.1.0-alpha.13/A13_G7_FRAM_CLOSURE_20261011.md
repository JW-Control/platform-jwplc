# Alpha13-G7 — Cierre FRAM (2026-10-11)

**G7=PASS físico, compilado y archive; Alpha13 permanece en curso.**

- RUN_ID_CIERRE=g7_close_20261011_173351_a4c16676
- RUN_ID_FISICO=g7_20261011_164228_e5e86ddc
- REUTILIZA_UPLOAD_FISICO=True
- PRUEBA_FISICA_LOG_SHA256=019d5a50ba1dbea9dd15a4e12cd9379aca8b9043f1250f1dfdb17c4851bb4a26
- PRUEBA_FISICA_PROOF_SHA256=0d28d37d1d6dd506df60fc3fee54ee9375076c0ff6c007f3959424543c93280e
- CLI=C:\Program Files\Arduino CLI\arduino-cli.exe
- CLI_SHA256=1017de89179c3167e6b8a38ca6cc4091fa69a3cf97aafa7810f01423003f5571
- Firmware SHA256=30cb8c596d212b6209e3fb179c1b9f216588c2fc83225834dfb2bce7088c663a; COM=COM4
- FRAM: escritura en 0x1F00..0x1F7F, respaldo NVS, restauración verificada.
- NULL y bounds, writeBlock/readBlock, checksum y preservación de salida: PASS.
- Archivo objeto recompilado SHA256=52b4b3f5154e7b0b45f6db5098830167c30db79fa03e12667d1f36fa5ad0501e
- Archivo .a SHA256=477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd
- Paridad objeto↔miembro archive: PASS
- Precompiled=full, enlace candidato y enlace normal: PASS
- Regresiones candidato: ['GLOBAL_FRAM', 'CONFIG_BLOCK']
- Regresiones producto: ['GLOBAL_FRAM', 'CONFIG_BLOCK']
- SHA finales: {'JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.cpp': 'cc068312c53814c9d3034616b38c926e26641f8075079b3f134b38c238fb7936', 'JWPLC/2.1.0/libraries/JW_FRAM/src/JW_FRAM.h': '1cf226930e0c1be9ce202a1227ab93f7c603c30ea39cec642b1229f3288f5f76', 'JWPLC/2.1.0/libraries/JW_FRAM/src/esp32/libJW_FRAM.a': '477ba0ccc3b20e638eccd7dda7d22b209d1ce73003df34917dc9ea7d476d83dd'}
- core.a G3 y TFT.a G4: intactos.

**Límite:** el informe canónico A13-009/A13-013 no está presente
íntegro; no se atribuye su cierre semántico hasta contrastarlo.
F120 (READY_TIMEOUT post-upload y TFT congelada) desapareció tras
reconectar alimentación: causa raíz histórica desconocida.
Se pasó físicamente FRAM sin reflashear, y la restauración se verificó.
No se tocó OpenPLC ni HMI Designer.
No PR ni publicación de alpha.

NEXT_GATE=A13-G8_SERIAL_A13_010_A13_011.

## Nota posterior — conciliación del alcance con la auditoría canónica

**El PASS técnico de G7 y los commits anteriores siguen válidos.**
Sin embargo, el informe canónico atribuye A13-009 a
`JW_FRAM::readString()` y A13-013 a constructores/
`debugPrint` privados: ninguna de esas correcciones fue
parte del candidato G7 cerrado. En el HEAD posterior al cierre,
ambas situaciones siguen visibles en fuentes.

Por tanto, **A13-009 y A13-013 continúan pendientes**
de un subhito de cumplimiento canónico (G7C).
Los guards de puntero y la protección checksum de
`readBlock()` desarrollados en G7 son correctos y
probados, pero **no sustituyen** dichos hallazgos.
Mantener F120 como causa raíz desconocida.

Documento de trazabilidad:
`A13_G7_CANONICAL_RECONCILIATION_AND_G8_SCOPE_20261011.md`.
