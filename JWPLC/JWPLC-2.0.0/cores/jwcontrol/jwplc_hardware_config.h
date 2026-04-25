#ifndef JWCONTROL_JWPLC_HARDWARE_CONFIG_H
#define JWCONTROL_JWPLC_HARDWARE_CONFIG_H

// =====================================================
// Configuración de hardware JWPLC
// =====================================================
// Estos flags permiten compilar el core para distintas variantes
// físicas del JWPLC Basic.
//
// Por defecto, todos los periféricos opcionales quedan habilitados.
// Luego boards.txt podrá sobrescribir estos valores con -D si una
// variante de placa no incluye algún periférico.
//
// Variante principal:
//   JWPLC Basic
//
// Variante económica/esencial propuesta:
//   JWPLC Basic Core
// =====================================================

#ifndef JWPLC_HAS_RTC
#define JWPLC_HAS_RTC 1
#endif

#ifndef JWPLC_HAS_FRAM
#define JWPLC_HAS_FRAM 1
#endif

#ifndef JWPLC_HAS_SD
#define JWPLC_HAS_SD 1
#endif

#ifndef JWPLC_HAS_ETHERNET
#define JWPLC_HAS_ETHERNET 1
#endif

#endif // JWCONTROL_JWPLC_HARDWARE_CONFIG_H