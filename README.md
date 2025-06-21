# JWPLC Platform for Arduino IDE

Repositorio oficial del package personalizado JWPLC para placas basadas en ESP32, optimizado para sistemas industriales.

## 🌟 Características principales

- 🔧 Soporte exclusivo para ESP32 base (no incluye S2, S3, C3, etc.)
- ⚡ Tamaño reducido: de 5.10 GB a 609 MB (~89% de reducción)
- 🧰 Compatible con Arduino IDE y OpenPLC
- 📦 Incluye librerías IDF precompiladas solo para ESP32
- 🧑‍💻 Facilita la programación industrial usando JWPLC Basic

## 🛠 Instalación

En Arduino IDE, ingresa este enlace en la opción de Gestor de URLs adicionales:

https://raw.githubusercontent.com/JW-Control/platform-jwplc/main/package_jwplc_index.json


## 📦 Versiones disponibles

| Versión | Fecha       | Notas                                |
|---------|-------------|---------------------------------------|
| 1.0.5   | 21/06/2025  | Optimizado para ESP32, package liviano |
| 1.0.4   | 10/12/2024  | Versión base con todos los chips       |

## 📁 Estructura del repositorio

- `/JWPLC` → Archivos comunes del core
- `/JWPLC_Basic` → Definiciones específicas para el JWPLC BASIC
- `/packages` → Librerías IDF compiladas solo para ESP32

## 🤝 Créditos

Basado en el core oficial de Espressif y adaptado por JW Control para sistemas de automatización industriales.

# 📦 Cambios por versión

## 🔖 Versión 1.0.5 – 21 de junio de 2025
- ✅ Eliminadas todas las variantes excepto ESP32 base
- ✅ Integración directa del `.zip` personalizado dentro del repositorio
- ✅ Reducción del tamaño del package de 5.10 GB a 609 MB
- ✅ Simplificación de compilación, instalación y mantenimiento
- ✅ Ideal para entornos industriales y dispositivos JWPLC

## 🕰 Versión 1.0.4 – 10 de diciembre de 2024
- Versión completa basada en `idf-release_v5.1`, incluyendo soporte para S2, S3, C3, etc.
