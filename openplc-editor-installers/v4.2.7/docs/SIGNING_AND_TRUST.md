# Firma y confianza de paquetes VPP

## Resumen

OpenPLC Editor 4.2.7 valida paquetes `.vpp` con una firma Ed25519 incluida en `signature.json`.

La firma protege:

- lista exacta de archivos;
- hash SHA-256 de cada archivo;
- `packageId`;
- version;
- `keyId`;
- fecha de firma.

Si el paquete no tiene firma, tiene archivos extra, le faltan archivos, tiene hashes distintos o usa una llave no confiable, el editor rechaza la importación.

## Qué podemos hacer en JW Control

### 1. Crear llave de desarrollo

```powershell
node .\tools\generate-dev-keypair.mjs .\keys
```

Esto genera:

```txt
keys/jw-control-dev-public.pem
keys/jw-control-dev-private.pem
```

La llave privada no debe subirse al repositorio.

### 2. Firmar el directorio VPP

```powershell
node .\tools\sign-vpp-package.mjs .\vpp jw-control-dev .\keys\jw-control-dev-private.pem
```

Esto genera:

```txt
vpp/signature.json
```

### 3. Empaquetar

```powershell
powershell -ExecutionPolicy Bypass -File .\tools\build-unsigned-vpp.ps1
```

El nombre del script conserva `unsigned` porque el empaquetado solo comprime el contenido. Si `vpp/signature.json` existe, también se incluirá en el `.vpp`.

## Por qué no basta con firmar localmente

OpenPLC Editor stock solo acepta llaves públicas incluidas en su lista de confianza. Si firmamos con una llave JW Control local, el paquete solo será aceptado por un build de OpenPLC que tenga esa llave pública agregada.

## Caminos para distribución pública

### Opción A: firma oficial OpenPLC

Enviar el paquete a OpenPLC/Autonomy para que sea firmado por su pipeline oficial o publicado en su catálogo.

Ventaja:

- compatible con OpenPLC Editor stock.

Desventaja:

- depende de aceptación externa.

### Opción B: llave pública JW Control en OpenPLC Editor

Enviar PR o solicitud para agregar una llave pública JW Control al trust store del editor.

Ventaja:

- JW Control puede firmar sus propios VPP.

Desventaja:

- requiere aprobación upstream.

### Opción C: build propio JW Control

Mantener un OpenPLC Editor 4.2.7 recompilado con la llave pública JW Control.

Ventaja:

- control total para talleres y clientes internos.

Desventaja:

- no es OpenPLC Editor stock.

## Decisión recomendada alpha3

Para `v2.1.0-alpha.3`:

- dejar estructura VPP lista;
- dejar scripts de firma;
- validar con build propio o documentar bloqueo de firma;
- no prometer importación en OpenPLC stock hasta tener firma confiable.
