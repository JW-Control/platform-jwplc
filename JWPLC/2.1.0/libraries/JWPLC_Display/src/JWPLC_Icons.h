// Biblioteca nativa de iconos JWPLC (JWPLC_Display)
// GENERADO AUTOMÁTICAMENTE por tools/jwplc-hmi-designer/icons/export-cpp-library.cjs
// No editar manualmente. Para actualizar, edita icons/library.cjs y regenera.

#ifndef JWPLC_ICONS_H
#define JWPLC_ICONS_H

#include <Arduino.h>
#include <JWPLC_UI.h>

class JWPLC_IconsClass
{
public:
    // --- Motores ---
    static const JWPLC_UIIcon* motor(uint8_t size = 24); // Motor trifásico [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* motorreductor(uint8_t size = 24); // Motorreductor [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* motorDosSentidos(uint8_t size = 24); // Motor dos sentidos [24, 32, 48, 64 px]

    // --- Bombas y ventiladores ---
    static const JWPLC_UIIcon* bomba(uint8_t size = 24); // Bomba [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* ventilador(uint8_t size = 24); // Ventilador [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* compresor(uint8_t size = 24); // Compresor [24, 32, 48 px]

    // --- Pilotos y señales ---
    static const JWPLC_UIIcon* piloto(uint8_t size = 24); // Piloto luminoso [24, 32, 48 px]
    static const JWPLC_UIIcon* baliza(uint8_t size = 24); // Torre de señalización [24, 32, 48 px]

    // --- Válvulas ---
    static const JWPLC_UIIcon* valvula(uint8_t size = 24); // Válvula [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* valvulaMotorizada(uint8_t size = 24); // Válvula motorizada [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* valvulaSolenoide(uint8_t size = 24); // Electroválvula [24, 32, 48 px]

    // --- Tanques y equipos ---
    static const JWPLC_UIIcon* piston(uint8_t size = 24); // Pistón neumático [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* pistonVertical(uint8_t size = 24); // Pistón vertical [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* tanque(uint8_t size = 24); // Tanque (nivel) [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* silo(uint8_t size = 24); // Silo / tolva [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* resistencia(uint8_t size = 24); // Resistencia / calefactor [24, 32, 48 px]
    static const JWPLC_UIIcon* agitador(uint8_t size = 24); // Agitador / mezclador [24, 32, 48, 64 px]
    static const JWPLC_UIIcon* transportador(uint8_t size = 24); // Cinta transportadora [24, 32, 48, 64 px]

    // --- Sensores ---
    static const JWPLC_UIIcon* proximidad(uint8_t size = 24); // Sensor de proximidad [24, 32, 48 px]
    static const JWPLC_UIIcon* termometro(uint8_t size = 24); // Temperatura [24, 32, 48 px]
    static const JWPLC_UIIcon* manometro(uint8_t size = 24); // Presión [24, 32, 48 px]

    // --- Alarmas ---
    static const JWPLC_UIIcon* alarma(uint8_t size = 24); // Alarma [24, 32, 48 px]
    static const JWPLC_UIIcon* campana(uint8_t size = 24); // Campana / bocina [24, 32, 48 px]
    static const JWPLC_UIIcon* sirena(uint8_t size = 24); // Baliza giratoria [24, 32, 48 px]

    // --- Botones y selectores ---
    static const JWPLC_UIIcon* pulsador(uint8_t size = 24); // Pulsador [24, 32, 48 px]
    static const JWPLC_UIIcon* paroEmergencia(uint8_t size = 24); // Paro de emergencia [24, 32, 48 px]
    static const JWPLC_UIIcon* selector(uint8_t size = 24); // Selector 0-1 [24, 32, 48 px]
    static const JWPLC_UIIcon* contacto(uint8_t size = 24); // Contacto / interruptor [24, 32, 48 px]

    // Búsqueda genérica por identificador del catálogo ("motores/motor", "motor", etc.)
    static const JWPLC_UIIcon* get(const char *iconId, uint8_t size = 24);
};

extern JWPLC_IconsClass JWPLC_Icons;

#endif // JWPLC_ICONS_H
