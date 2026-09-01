# Alpha7 — VPP Alpha18: preservación, firma y reproducibilidad

Fecha de cierre: 2026-09-01

## Estado

```text
ALPHA18_VPP_STATUS=CLOSED
VPP_VERSION=2.1.0-alpha.18
SIGNED_PAYLOAD_PHYSICAL=9/9
FRESH_CHECKOUT_SIGNED_PAYLOAD=9/9
ALPHA18_PUBLICATION=PASS
```

Este documento registra el cierre de preservación del VPP utilizado por OpenPLC para JWPLC Basic durante Alpha18.

El objetivo de este gate fue conservar exactamente el paquete validado en banco y garantizar que un checkout nuevo pueda reconstruir los mismos bytes que `signature.json` declara, sin depender de la configuración local `core.autocrlf` del desarrollador.

No se redefine aquí el formato de firma para versiones futuras. Alpha18 conserva el payload que fue realmente validado.

## 1. Repositorio y commit de cierre

```text
repository: JW-Control/platform-jwplc
branch: v2.1.0-alpha.7/feature/openplc-backplane-validation
commit: 26160ab5f7f58fd25aea1ef846b9b85d64f55d6b
subject: feat(alpha7): consolidar VPP alpha18 y preservar payload firmado
```

Después de la publicación:

```text
PLATFORM_DIVERGENCE=0/0
FORCE_PUSH_USED=NO
```

El commit OpenPLC asociado al runtime Alpha18 es:

```text
repository: JW-Control/openplc-editor
branch: develop/alpha7-openplc-remote-io-rtu
commit: 2adff7733d0aa33801514cf58954c2350e0b7143
subject: feat(alpha7): integrar servicio hardware cooperativo alpha18
```

La documentación funcional del runtime y del debugger queda en:

```text
openplc-editor/docs/alpha7/ALPHA7_ALPHA18_CLOSURE.md
```

## 2. Archivos principales del VPP Alpha18

```text
openplc-editor-installers/v4.2.7/vpp/
  hal/jwplcbasic.cpp
  manifest.json
  signature.json
  README.md
  assets/jwplcbasic.png
  examples/...
  screens/...
```

El `manifest.json` y `signature.json` publicados declaran:

```text
MANIFEST_VERSION=2.1.0-alpha.18
SIGNATURE_VERSION=2.1.0-alpha.18
VERSION_MATCH_ALPHA18=PASS
```

## 3. Payload firmado declarado

`signature.json` declara nueve archivos dentro de su mapa `files`.

| Archivo | SHA-256 firmado | EOL de checkout requerido |
|---|---|---|
| `README.md` | `f98f113f4095893aca82fbb0a5d750d8a6c383af37e3cbe99691854c96ddb832` | CRLF |
| `assets/jwplcbasic.png` | `64c7c2755436f2224c1aa7f4ed535c95339eba3df43dbc93f1f19bf5f1b6fb68` | binario |
| `examples/JWPLC_Basic_Blink_Q0_0/README.md` | `363bf0ddb193c6d4978e18c3947bf53df755c42704fe383632e8929cad83c52f` | CRLF |
| `examples/JWPLC_Basic_IO_Test_I0_to_Q0/README.md` | `c40d37f6963766cb280de09c43ab56f56b74ce5f533beaf06d35e937f37cb747` | CRLF |
| `hal/jwplcbasic.cpp` | `59877501ade16aa62917c744fb5f52512ad197409174b80894639e6489a6a9d7` | LF |
| `manifest.json` | `8369cd7ad1d490a5ec4db5c17e5ce3e62268c74f9fea3994244523ba9a5eb8e8` | LF |
| `screens/backplane.json` | `415c92f559aba0d1e07c87dd715678c1d12fcecffa70639c75a4e2de578c30ff` | CRLF |
| `screens/jwplc-basic-remote-io.json` | `def2e2e272d1d2b168f498670f7dfb135e07053b447423568916f0a7f9f61638` | CRLF |
| `screens/modbus.json` | `2ba86ffbc65d197eaf94ca6ebc36b6ed0b2cf0d2ea7f0dc6c2676cfd61262ef6` | CRLF |

Resultado del gate:

```text
SIGNED_FILE_COUNT=9
CHECKOUT_HASH_PASS=9
CHECKOUT_HASH_FAIL=0
PHYSICAL_HASH_PASS=9
PHYSICAL_HASH_FAIL=0
```

Por tanto:

```text
FRESH_CHECKOUT_SIGNED_PAYLOAD=True
PHYSICAL_SIGNED_PAYLOAD=True
```

## 4. `signature.json`

El propio archivo `signature.json` no aparece dentro de su mapa `files`, por lo que no forma parte de esos nueve hashes declarados.

Su representación validada es LF y su SHA-256 físico durante el gate fue:

```text
3e72e7df94d023ff89c34fb1c7443aaedbab6be0a457602d166cdd9655e7b308
```

La simulación de checkout LF produjo el mismo hash:

```text
SIGNATURE_REPRODUCIBLE=True
```

Metadatos del esquema de firma conservados:

```text
algorithm=ed25519
keyId=jwcontrol-2026
version=2.1.0-alpha.18
```

### Alcance de esta validación

Durante este gate se verificaron:

- consistencia de versión;
- hashes SHA-256 de los nueve archivos declarados;
- igualdad de los bytes físicos con los hashes declarados;
- reconstrucción de los mismos hashes bajo la política EOL explícita;
- reproducibilidad del propio `signature.json`.

No se registró en este gate una verificación criptográfica Ed25519 independiente con la clave pública. Por tanto, no se debe reinterpretar `9/9` como evidencia de una verificación criptográfica adicional que no fue ejecutada.

## 5. Hallazgo: política EOL histórica mixta

Durante la preservación se observó que seis archivos físicos firmados estaban en CRLF mientras que los blobs canónicos existentes en Git estaban en LF.

Ejemplo para `README.md`:

```text
HEAD_BYTES=1002
INDEX_BYTES=1002
WORKTREE_BYTES=1050

HEAD_EOL_LF=48
INDEX_EOL_LF=48
WORKTREE_EOL_CRLF=48

HEAD_EQUALS_INDEX=True
INDEX_EQUALS_WORKTREE=False
GIT_STATUS=CLEAN
```

Los 48 bytes extra del working tree correspondían exactamente a los 48 caracteres `CR` de los finales CRLF.

El mismo patrón se comprobó en los otros cinco archivos históricos.

Causa práctica:

```text
core.autocrlf=true
+ archivos tracked sin política EOL explícita previa
+ firma calculada sobre bytes físicos del working tree
```

Git consideraba los archivos limpios porque normalizaba CRLF → LF al comparar con el índice, aunque el hash SHA-256 de los bytes físicos fuese distinto al hash del blob.

## 6. Decisión Alpha18

No se resignó Alpha18 y no se alteraron los bytes ya validados.

Se agregó una `.gitattributes` específica para el VPP Alpha18 que reproduce exactamente el payload firmado.

Política:

```text
# Historical signed text payload: CRLF.
openplc-editor-installers/v4.2.7/vpp/README.md text eol=crlf
openplc-editor-installers/v4.2.7/vpp/examples/JWPLC_Basic_Blink_Q0_0/README.md text eol=crlf
openplc-editor-installers/v4.2.7/vpp/examples/JWPLC_Basic_IO_Test_I0_to_Q0/README.md text eol=crlf
openplc-editor-installers/v4.2.7/vpp/screens/backplane.json text eol=crlf
openplc-editor-installers/v4.2.7/vpp/screens/jwplc-basic-remote-io.json text eol=crlf
openplc-editor-installers/v4.2.7/vpp/screens/modbus.json text eol=crlf

# Alpha18 generated/signed text payload: LF.
openplc-editor-installers/v4.2.7/vpp/hal/jwplcbasic.cpp text eol=lf
openplc-editor-installers/v4.2.7/vpp/manifest.json text eol=lf
openplc-editor-installers/v4.2.7/vpp/signature.json text eol=lf

# Signed binary payload.
openplc-editor-installers/v4.2.7/vpp/assets/jwplcbasic.png -text
```

Resultado efectivo comprobado:

```text
CRLF_POLICY_PASS_COUNT=6
LF_POLICY_PASS_COUNT=3
BINARY_POLICY_PASS=True
```

Los tres archivos bajo política LF son:

```text
hal/jwplcbasic.cpp
manifest.json
signature.json
```

De ellos, los dos primeros forman parte del mapa firmado de nueve archivos; `signature.json` se preserva aparte.

## 7. Conclusión de reproducibilidad

El gate simuló los bytes que produciría un checkout nuevo aplicando la política `.gitattributes` y volvió a calcular SHA-256 para todos los archivos declarados.

Resultado:

```text
CHECKOUT_HASH_PASS=9
CHECKOUT_HASH_FAIL=0
FRESH_CHECKOUT_SIGNED_PAYLOAD=True

PHYSICAL_HASH_PASS=9
PHYSICAL_HASH_FAIL=0
PHYSICAL_SIGNED_PAYLOAD=True

SIGNATURE_REPRODUCIBLE=True
```

Conclusión:

```text
ALPHA18_SIGNED_BYTES_PRESERVED=PASS
ALPHA18_CHECKOUT_REPRODUCIBILITY=PASS
```

## 8. Rebase y publicación

Durante la preservación, el branch remoto recibió commits paralelos de los gates REV6 / STOP / ETHNEXT.

La integración se hizo mediante rebase porque las rutas no se superponían con el VPP Alpha18.

Se conservaron referencias locales de seguridad antes de cada rebase:

```text
safety/alpha7-platform-pre-rebase-20260901-98a7b64e
safety/alpha7-platform-pre-rebase2-20260901-76aa07f3
```

Estas referencias son locales de seguridad y no forman parte del contrato de publicación del VPP.

Después del segundo rebase:

```text
NEW_ALPHA18_COMMIT=26160ab5
BEHIND=0
AHEAD=7
SIGNED_PAYLOAD=9/9
```

La publicación final fue fast-forward y sin force push:

```text
PLATFORM_PUSH_VERIFIED=True
FORCE_PUSH_USED=NO
PLATFORM_DIVERGENCE=0/0
```

## 9. Relación con Alpha18 funcional

El HAL firmado `hal/jwplcbasic.cpp` contiene la implementación JWPLC asociada al servicio cooperativo Alpha18 utilizado por OpenPLC.

El cierre funcional independiente confirmó:

```text
ALPHA18_DEBUGGER_GATE=PASS
MODBUS_TCP_SERVER_WITH_RTU_BACKPLANE=PASS
OPENPLC_DEBUGGER_TCP_WITH_RTU_BACKPLANE=PASS
RTU_FC02_FAILURES=0
RTU_FC15_FAILURES=0
SLOW_EDGE_CORRELATION=20/20
COUNTER_GAP_GROWTH=0
```

La evidencia detallada de runtime, timing y debugger se conserva en el repositorio OpenPLC.

## 10. Pendiente del proceso de firma

La política `.gitattributes` actual es una decisión de preservación de Alpha18, no la definición deseada del firmador para siempre.

Pendiente posterior:

```text
SIGNER_CANONICAL_TEXT_FORMAT=PENDING
```

Antes de modificar el proceso de firma se debe decidir explícitamente:

1. formato canónico para archivos de texto;
2. momento exacto de normalización;
3. si el hash debe calcularse sobre bytes canónicos del repositorio o sobre bytes de distribución;
4. cómo verificar el resultado en Windows independientemente de `core.autocrlf`;
5. cómo probar la firma criptográfica Ed25519 como gate separado;
6. cómo migrar sin invalidar releases ya publicados.

La opción preferible a evaluar es un formato canónico único LF para todos los archivos de texto futuros, pero esa decisión no se adopta retroactivamente para Alpha18.

## 11. Lo que no debe cambiarse al retomar Alpha7

Hasta que exista una decisión explícita del firmador:

```text
DO_NOT_RESIGN_ALPHA18=YES
DO_NOT_NORMALIZE_ALPHA18_SIGNED_TEXT_BLINDLY=YES
DO_NOT_REMOVE_ALPHA18_GITATTRIBUTES=YES
```

Tampoco se debe interpretar esta preservación como permiso para cambiar APIs o retirar periféricos del autoload normal.

## 12. Siguiente gate

El próximo cambio funcional del Backplane es independiente del cierre VPP:

```text
NEXT_GATE=FC01_OUTPUT_FEEDBACK
```

Antes de editar `jwplcbasic.cpp` se debe inspeccionar la API actual de `JWPLC_ModbusRTU` y la máquina cooperativa existente.

Objetivo conceptual:

```text
FC15 WRITE requested outputs
      ↓
FC01 READ actual coils
      ↓
compare requested vs feedback
      ↓
FC02 READ discrete inputs
      ↓
repeat
```

La implementación debe mantener el runtime cooperativo, no romper APIs ya probadas y no introducir concurrencia adicional.

## 13. Cierre

```text
ALPHA18_VPP=CLOSED
SIGNED_PAYLOAD=9/9
CHECKOUT_REPRODUCIBILITY=PASS
NEXT=FC01_OUTPUT_FEEDBACK
```
