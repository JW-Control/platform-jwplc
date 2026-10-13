# Índice de Paquete UI: `JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/`

Este directorio contiene la propuesta aprobada de rediseño de interfaz de usuario (UI/UX) para el inspector de medidores y barras (`BAR`) del **JWPLC HMI Designer**.

---

## 1. Contenido del Paquete

| Archivo | Tipo | Tamaño / Líneas | Propósito |
| :--- | :---: | :---: | :--- |
| [`JWPLC_HMI_Designer_Inspector_BAR_Propuesta_2_Especificacion_UI.md`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/JWPLC_HMI_Designer_Inspector_BAR_Propuesta_2_Especificacion_UI.md) | Markdown | 540 líneas (~34 KB) | Especificación técnica y de diseño completa para la implementación del inspector BAR por pestañas y subpestañas. |
| [`JWPLC_HMI_Designer_BAR_Propuesta_2_Referencia.png`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/JWPLC_HMI_Designer_BAR_Propuesta_2_Paquete_UI/JWPLC_HMI_Designer_BAR_Propuesta_2_Referencia.png) | Imagen PNG | 2.4 MB | Mockup visual de alta resolución seleccionado como referencia de composición, lenguaje visual y densidad. |

---

## 2. Resumen de la Especificación de UI (Propuesta 2)

El rediseño reemplaza el scroll vertical único y excesivamente largo del inspector de barras por una organización jerárquica:

### Pestañas Principales:
1. **Diseño:** Configuración visual y geométrica del medidor.
   - *Subpestaña Forma:* Orientación (horizontal, vertical, circular), esquinas redondeadas, grosor de pista y borde.
   - *Subpestaña Contenido:* Rango mínimo/máximo, unidades, etiquetas y texto central.
   - *Subpestaña Apariencia:* Colores de pista, relleno, degradados y bordes.
   - *Subpestaña Ubicación:* Coordenadas X/Y, ancho, alto y alineación.
2. **Datos PLC:** Configuración de la variable de proceso en C++ y control deslizante de **simulación interactiva local**.
3. **Técnico:** Vista previa del bloque de código C++ generado para `JWPLC_UIBarField` y botón de copia rápida.

---

## 3. Estado de Implementación en Código
La implementación en código de esta propuesta vive en [`poc/designer-bar.js`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/designer-bar.js). Consultar [`poc/INDEX.md`](file:///c:/Users/Andre/Documents/GitHub/platform-jwplc/tools/jwplc-hmi-designer/poc/INDEX.md) para el detalle de funciones y parches asociados.
