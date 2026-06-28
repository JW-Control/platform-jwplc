# Plan: fork OpenPLC Editor - JWPLC Edition

## Objetivo

Mantener un fork propio de OpenPLC Editor para validar e instalar paquetes `.vpp` de JWPLC Basic firmados por JW Control.

## Upstream propuesto

```txt
Autonomy-Logic/openplc-editor
```

Fork propuesto:

```txt
JW-Control/openplc-editor
```

## Motivo

OpenPLC Editor 4.2.6/4.2.7 stock solo confía en la llave pública `openplc-2026`. Los paquetes `.vpp` firmados con `jw-control-dev` o una futura llave `jwcontrol-2026` son rechazados como `Untrusted signing key`.

## Cambio mínimo requerido

Archivo upstream:

```txt
src/backend/shared/utils/vpp/trusted-keys.ts
```

Agregar una llave pública adicional:

```ts
export const TRUSTED_PACKAGE_KEYS: Record<string, string> = {
  'openplc-2026': `-----BEGIN PUBLIC KEY-----
...
-----END PUBLIC KEY-----
`,
  'jwcontrol-2026': `-----BEGIN PUBLIC KEY-----
...LLAVE_PUBLICA_JW_CONTROL...
-----END PUBLIC KEY-----
`,
}
```

## Reglas

- No eliminar `openplc-2026`.
- No subir llaves privadas.
- No desactivar `REQUIRE_SIGNATURE` para builds de clientes.
- Diferenciar claramente el build: `OpenPLC Editor - JWPLC Edition`.
- Documentar que es un fork/build mantenido por JW Control.
- Mantener trazabilidad de cambios frente a upstream.

## Flujo inicial recomendado

```powershell
gh repo fork Autonomy-Logic/openplc-editor --org JW-Control --clone=false
git clone https://github.com/JW-Control/openplc-editor.git
cd openplc-editor
git remote add upstream https://github.com/Autonomy-Logic/openplc-editor.git
git checkout development
git checkout -b develop/jwplc-trusted-key
```

## Generar llave definitiva JW Control

```powershell
mkdir keys
openssl genpkey -algorithm ED25519 -out keys\jwcontrol-2026-private.pem
openssl pkey -in keys\jwcontrol-2026-private.pem -pubout -out keys\jwcontrol-2026-public.pem
```

La llave privada debe quedar fuera del repositorio y en almacenamiento seguro de JW Control.

## Pendientes

- [ ] Crear fork en GitHub.
- [ ] Generar llave definitiva `jwcontrol-2026`.
- [ ] Agregar llave pública al fork.
- [ ] Recompilar editor.
- [ ] Firmar VPP JWPLC con `jwcontrol-2026`.
- [ ] Validar importación del VPP.
- [ ] Validar compilación/subida en JWPLC Basic 2.0.0.
- [ ] Documentar licencia/atribución para entregas comerciales.
