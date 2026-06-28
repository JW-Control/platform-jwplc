# Instalador del patch OpenPLC para JWPLC Basic

Este directorio contiene scripts para aplicar el patch de integración **JWPLC Basic v2.0.0 + OpenPLC Editor v4** en Windows.

## Archivos

| Archivo | Uso |
|---|---|
| `install_openplc_jwplc_patch.bat` | Lanzador recomendado para usuario final. |
| `install_openplc_jwplc_patch.ps1` | Script PowerShell que realiza la instalación. |

## Qué hace el instalador

1. Solicita la ruta raíz de OpenPLC Editor.
2. Verifica que exista la carpeta origen del patch.
3. Crea un backup automático dentro de la carpeta destino.
4. Copia/sobrescribe los archivos modificados.
5. Muestra un resumen final.

## Cómo usar

1. Cerrar OpenPLC Editor.
2. Ejecutar:

```txt
install_openplc_jwplc_patch.bat
```

3. Ingresar la ruta raíz de OpenPLC Editor cuando el script la solicite.

Ejemplo:

```txt
C:\Users\usuario\OpenPLC_Editor_save1
```

4. Confirmar la instalación.
5. Abrir OpenPLC Editor.
6. Seleccionar:

```txt
JWPLC BASIC [2.0.0]
```

## Permisos

El script no requiere permisos de administrador si OpenPLC Editor está instalado en una carpeta del usuario.

Si OpenPLC Editor está instalado en `C:\Program Files`, Windows puede requerir permisos de administrador.

## Seguridad

El instalador es un script auditable. No modifica el registro de Windows y no instala servicios.

Solo copia archivos desde:

```txt
installers/openplc-jwplc-basic-v2.0.0/open-plc-editor/
```

hacia la ruta indicada por el usuario.

## Restaurar backup

Cada instalación crea una carpeta tipo:

```txt
backup_jwplc_openplc_YYYYMMDD_HHMMSS
```

Para restaurar, copiar manualmente su contenido sobre la instalación de OpenPLC Editor.
