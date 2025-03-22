#ifndef JWPLC_PERIPHERALS_H
#define JWPLC_PERIPHERALS_H

#include <Arduino.h>

#ifdef __cplusplus
extern "C" {
#endif

// Prototipo de la función que reemplazará pinMode.
void jwplc_pinMode(uint16_t pin, uint8_t mode);

// Función de depuración que configura el pin y retorna un mensaje.
// Útil para pruebas.
const char* debugJwplc_pinMode(uint16_t pin, uint8_t mode);

// Función que reemplaza la API estándar de digitalWrite.
void jwplc_digitalWrite(uint16_t pin, uint16_t val);

// Función que reemplaza la API estándar de digitalRead.
int jwplc_digitalRead(uint16_t pin);

// Función para configurar PWM: se le pasan el pin, el duty (0-255, por ejemplo) y la frecuencia en Hz.
void jwplc_analogWrite(uint16_t pin, int duty, int freq);

#ifdef __cplusplus
}
#endif

// Redefinir las macros para redirigir a nuestras funciones.
#undef pinMode
#define pinMode(x, y) jwplc_pinMode((uint16_t)(x), (y))

#undef digitalWrite
#define digitalWrite(x, y) jwplc_digitalWrite((uint16_t)(x), (y))

#undef digitalRead
#define digitalRead(x) jwplc_digitalRead((uint16_t)(x))

#endif // JWPLC_PERIPHERALS_H