#ifndef JWPLC_DISPLAY_AUTO_H
#define JWPLC_DISPLAY_AUTO_H

// =====================================================
// JWPLC Display autoload marker
// =====================================================
//
// Este header existe para que Arduino Builder detecte y compile
// automaticamente la libreria JWPLC_Display cuando se usa una placa JWPLC.
//
// Durante library discovery se mantiene deliberadamente liviano. Arduino
// preprocesa el sketch varias veces mientras resuelve dependencias; expandir
// aqui todo el ecosistema JWPLC en cada pasada aumenta el tiempo aunque las
// librerias ya esten cacheadas.
//
// La API publica del display permanece visible en todas las fases porque su
// header es liviano. El arbol pesado de perifericos globales se expande solo
// durante el build normal.
//
// Durante la compilacion normal se conserva exactamente el contrato publico:
// - perifericos globales disponibles sin includes manuales;
// - API publica de JWPLC_Display disponible desde Arduino.h;
// - ningun periferico se elimina del autoload real.

#ifndef JWPLC_LIBRARY_DISCOVERY_PHASE
#define JWPLC_LIBRARY_DISCOVERY_PHASE 0
#endif

#include <JWPLC_Display_API.h>

#if !JWPLC_LIBRARY_DISCOVERY_PHASE
#include <JWPLC_GlobalPeripherals.h>
#endif

#endif // JWPLC_DISPLAY_AUTO_H
