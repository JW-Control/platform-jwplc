# JWPLC Basic Patch Installer for OpenPLC Editor v4

Este paquete instala el patch de integración para habilitar `JWPLC BASIC [2.0.0]` en OpenPLC Editor v4.

No incluye OpenPLC Editor. Primero instala OpenPLC Editor v4.

## Instalación rápida

1. Cierra OpenPLC Editor.
2. Ejecuta `installer/install_openplc_jwplc_patch.bat`.
3. Selecciona la ruta raíz de OpenPLC Editor.
4. Confirma la instalación.
5. Abre OpenPLC Editor.
6. Ve a `Device > Configuration`.
7. Selecciona `JWPLC BASIC [2.0.0]`.

## Seguridad

El instalador crea un backup automático antes de sobrescribir archivos.
No modifica el registro de Windows.
No instala servicios.
No modifica el package Arduino `platform-jwplc`.