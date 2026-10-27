Pre-release de prueba del **JWPLC HMI Designer 2.1.0-alpha.15** (línea OpenPLC + HMI) para Windows x64.

No es la Release oficial: la `v2.1.0-alpha.15` se publica al cerrar la alpha e incluirá estos mismos instaladores
junto con el package. Esta pre-release solo sirve para descargar e instalar el Designer sin compilarlo.

## Descarga

| Archivo | Para qué |
| --- | --- |
| `JWPLC-HMI-Designer-Setup-__VERSION__-x64.exe` | Instalador (recomendado). Se instala para el usuario actual, sin permisos de administrador, y crea los accesos directos. |
| `JWPLC-HMI-Designer-Portable-__VERSION__-x64.exe` | Portable: se ejecuta sin instalar (por ejemplo, desde un USB). |
| `JWPLC-HMI-Designer-__VERSION__-SHA256.txt` | Sumas SHA-256 para comprobar la descarga. |

## Instalación

1. Descargar `JWPLC-HMI-Designer-Setup-__VERSION__-x64.exe` y ejecutarlo.
2. El instalador no está firmado con un certificado de código: si Windows SmartScreen muestra «Windows protegió
   su PC», pulsar **Más información** → **Ejecutar de todas formas**.
3. Abrir **JWPLC HMI Designer** desde el menú Inicio.

Para usarlo con un sketch de Arduino IDE hace falta el core **JWPLC 2.1.0-alpha.14** (o posterior) instalado en el
Arduino IDE. Dentro de OpenPLC Editor no hace falta instalar este Designer: el instalador del editor
(`jwplc-editor-v4.2.8-jwplc.4` en JW-Control/openplc-editor) ya lo trae incluido.

## Qué cambia respecto de 2.1.0-alpha.14

- Versión visible 2.1.0-alpha.15.
- Modo OpenPLC (puente `jwplc-hmi-host/1.1`): «Actualizar HMI», «Variable PLC» con las variables del programa
  (también las del Ladder sin dirección) y aviso al cerrar con cambios sin guardar.

## Comprobar la descarga (opcional)

En PowerShell, dentro de la carpeta de descargas:

```powershell
Get-FileHash .\JWPLC-HMI-Designer-Setup-__VERSION__-x64.exe -Algorithm SHA256
```

El valor debe coincidir con el de `JWPLC-HMI-Designer-__VERSION__-SHA256.txt`.

## Origen

Compilado por GitHub Actions desde el tag `__TAG__` (commit `__SHA__`).
