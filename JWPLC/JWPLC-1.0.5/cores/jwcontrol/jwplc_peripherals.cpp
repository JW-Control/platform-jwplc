#include "jwplc_peripherals.h"
#include "esp32-hal-gpio.h"   // Para __funciones nativas originales

// Incluir los headers de tus drivers en C (asegúrate de que tengan los guards y, si es necesario, extern "C")
#ifdef __cplusplus
extern "C" {
#endif
#include "peripheral-tca6424a.h"  // Driver basado en C para el TCA6424A
#include "WireC.h"              // Librería I2C en C (WireC)
#ifdef __cplusplus
}
#endif

#include <stdio.h>

// Declaraciones adelantadas de las funciones nativas.
extern "C" void ARDUINO_ISR_ATTR __pinMode(uint8_t pin, uint8_t mode);
extern "C" void ARDUINO_ISR_ATTR __digitalWrite(uint8_t pin, uint8_t val);
extern "C" int ARDUINO_ISR_ATTR __digitalRead(uint8_t pin);

// Para analogWrite, usaremos la API LEDC de ESP32:
#include "driver/ledc.h"

// Macro para identificar pines del expansor.
// Se asume que los pines virtuales tienen la parte alta igual a 0x2200.
#define IS_EXPANDER_PIN(pin) (((pin) & 0xFF00) == 0x2200)

// Función auxiliar para determinar si un pin es del expansor.
static inline bool esExpanderPin(uint16_t pin) {
  return IS_EXPANDER_PIN(pin);
}

// Buffer estático para almacenar mensajes de depuración.
// Nota: Se sobrescribe en llamadas sucesivas.
static char msgBuffer[64];

/* Función de depuración que configura el pin y retorna un mensaje.
   Para pines virtuales se invoca TCA6424A_setPinDirection() para configurar el modo,
   mapeando INPUT a TCA6424A_INPUT y OUTPUT a TCA6424A_OUTPUT.
   Para pines nativos se llama a la función original __pinMode().
*/
const char* debugJwplc_pinMode(uint16_t pin, uint8_t mode) {
  Serial.printf("➡️ debugJwplc_pinMode(pin=0x%04X, mode=%s)\n", pin, (mode == INPUT ? "INPUT" : "OUTPUT"));

  if (esExpanderPin(pin)) {
    Serial.println("🔍 Es pin de expansor");
    uint8_t channel = pin & 0xFF;  // Extraemos canal 0-23
    Serial.printf("📍 Canal del expansor: %d\n", channel);

    uint8_t tcaMode;
    if (mode == INPUT) {
      tcaMode = TCA6424A_INPUT;
    } else if (mode == OUTPUT) {
      tcaMode = TCA6424A_OUTPUT;
    } else {
      tcaMode = TCA6424A_INPUT;
    }
    Serial.printf("⚙️ Modo a configurar en expansor: %s\n", (tcaMode == TCA6424A_INPUT ? "INPUT" : "OUTPUT"));

    bool ok = TCA6424A_setPinDirection(TCA6424A_DEFAULT_ADDRESS, channel, tcaMode);
    Serial.println(ok ? "✅ Dirección configurada con éxito" : "❌ Error al configurar dirección");

    snprintf(msgBuffer, sizeof(msgBuffer), "Expansor: Pin %d configurado como %s", channel,
             (tcaMode == TCA6424A_INPUT) ? "INPUT" : "OUTPUT");
    return msgBuffer;
  } else {
    Serial.println("🔍 Es pin nativo");
    __pinMode((uint8_t)pin, mode);
    snprintf(msgBuffer, sizeof(msgBuffer), "Nativo configurado: Pin %d", pin);
    return msgBuffer;
  }
}

/* Función que reemplaza la API estándar de pinMode.
   Se llama automáticamente gracias a la macro definida en el header.
*/
void jwplc_pinMode(uint16_t pin, uint8_t mode) {
  if (esExpanderPin(pin)) {
    uint8_t channel = pin & 0xFF;
    uint8_t tcaMode;
    if (mode == INPUT) {
      tcaMode = TCA6424A_INPUT;
    } else if (mode == OUTPUT) {
      tcaMode = TCA6424A_OUTPUT;
    } else {
      tcaMode = TCA6424A_INPUT;
    }
    TCA6424A_setPinDirection(TCA6424A_DEFAULT_ADDRESS, channel, tcaMode);
    yield();  // Se cede el control para alimentar el watchdog.
  } else {
    __pinMode((uint8_t)pin, mode);
  }
}

/* Función sobrescrita de digitalWrite que se usará en producción.
   Se verifica si el pin es virtual: si es así, se usa el driver TCA6424A_writePin;
   si es un pin nativo, se llama a la función nativa __digitalWrite.
*/
void jwplc_digitalWrite(uint16_t pin, uint16_t val) {
  if (esExpanderPin(pin)) {
    uint8_t channel = pin & 0xFF;
    // Se asume que TCA6424A_writePin retorna 0 en caso de éxito (similar a WireC_writeBit).
    // Convertimos el valor a booleano: HIGH (cualquier valor distinto de 0) es true.
    TCA6424A_writePin(TCA6424A_DEFAULT_ADDRESS, channel, (val != 0));
    yield();
  } else {
    __digitalWrite((uint8_t)pin, (uint8_t)val);
  }
}

/* Función sobrescrita de digitalRead que se usará en producción.
   Si el pin es virtual, se delega a TCA6424A_readPin; si es nativo, se llama a __digitalRead.
*/
int jwplc_digitalRead(uint16_t pin) {
  if (esExpanderPin(pin)) {
    uint8_t channel = pin & 0xFF;
    uint8_t state = 0;
    if (TCA6424A_readPin(TCA6424A_DEFAULT_ADDRESS, channel, &state)) {
      return state;
    } else {
      return -1;  // Error al leer.
    }
  } else {
    return __digitalRead((uint8_t)pin);
  }
}

// Función sobrescrita de analogWrite para pines nativos.
// Ahora solo recibe (pin, duty), asumiendo que la frecuencia y resolución ya fueron configuradas
// previamente mediante jwplc_analogConfig().
void jwplc_analogWrite(uint16_t pin, uint16_t duty) {
  if (esExpanderPin(pin)) {
    // Para pines virtuales (expansor), por ahora no se soporta PWM.
    snprintf(msgBuffer, sizeof(msgBuffer), "PWM no soportado en pin virtual %d", pin & 0xFF);
    Serial.println(msgBuffer);
    return;
  } else {
    // Para pines nativos, simplemente escribe el duty usando la API LEDC.
    // Se asume que el pin ya fue configurado previamente mediante jwplc_analogConfig.
    // Normalmente, ledcWrite() actualiza el duty en el canal asociado.
    __asm__ __volatile__("");  // Dummy statement para evitar optimización (opcional)
    ledcWrite((uint8_t)pin, duty);
  }
}

// Esta función configurará el pin PWM en pines nativos utilizando ledcAttach.
// En este ejemplo, para pines nativos, se llama a ledcAttach; para pines virtuales, se imprime un mensaje (o se deja sin soporte).
void jwplc_analogConfig(uint16_t pin, uint32_t freq, uint8_t resolution) {
  if (esExpanderPin(pin)) {
    // Por ahora, para pines virtuales no soportamos PWM.
    Serial.print("PWM no soportado en pin virtual ");
    Serial.println(pin & 0xFF);
    return;
  } else {
    // Para pines nativos: usaremos ledcAttach.
    // ledcAttach() se define en esp32-hal-ledc.h; normalmente retorna un bool.
    // Aquí usamos directamente la función para adjuntar el pin al LEDC con la frecuencia y resolución dadas.
    if (!ledcAttach((uint8_t)pin, freq, resolution)) {
      Serial.print("Error al configurar PWM en pin ");
      Serial.println(pin);
    }
  }
}

