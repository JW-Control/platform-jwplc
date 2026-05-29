#include "peripherals_init.h"
#include "peripheral-tca6424a.h"
#include "pins_arduino.h"
#include "jwplc_peripherals.h"

// Asegúrate de usar la ruta correcta

void initPeripherals(void)
{
#ifdef JWPLC_BASIC
    // Iniciar el EN_IO en LOW
    pinMode(EN_IO, OUTPUT);
    digitalWrite(EN_IO, LOW);

    // Inicializa el bus I2C y el TCA6424A
    WireC_begin();
    TCA6424A_init(TCA6424A_DEFAULT_ADDRESS);

    pinMode(I0_0, INPUT);
    pinMode(I0_1, INPUT);
    pinMode(I0_2, INPUT);
    pinMode(I0_3, INPUT);
    pinMode(I0_4, INPUT);
    pinMode(I0_5, INPUT);

    pinMode(Q0_0, OUTPUT);
    pinMode(Q0_1, OUTPUT);
    pinMode(Q0_2, OUTPUT);
    pinMode(Q0_3, OUTPUT);
    pinMode(Q0_4, OUTPUT);
    pinMode(Q0_5, OUTPUT);
    pinMode(Q0_6, OUTPUT);
    pinMode(Q0_7, OUTPUT);
    pinMode(Q0_8, OUTPUT);
    pinMode(Q0_9, OUTPUT);

    digitalWrite(Q0_0, LOW);
    digitalWrite(Q0_1, LOW);
    digitalWrite(Q0_2, LOW);
    digitalWrite(Q0_3, LOW);
    digitalWrite(Q0_4, LOW);
    digitalWrite(Q0_5, LOW);
    digitalWrite(Q0_6, LOW);
    digitalWrite(Q0_7, LOW);
    digitalWrite(Q0_8, LOW);
    digitalWrite(Q0_9, LOW);

    // Finalizar el EN_IO en HIGH
    digitalWrite(EN_IO, HIGH);

    // Aquí también puedes inicializar las salidas PWM y analógicas de pines nativos a 0,
    // por ejemplo, llamar a analogWrite(pin, 0, freq) para cada pin que se quiera inicializar.
#endif
    // Si en el futuro agregas otros chips, los inicializas aquí.
}