#include "JWControl.h"
#include <assert.h>

#define RETURN_PINMODE(pin, mode)             \
    ret = pinMode(pin, mode);                 \
    if (ret != 0)                             \
    {                                         \
        IS_initDefaultIOPins_line = __LINE__; \
        return ret;                           \
    }
#define RETURN_DIGITALWRITE(pin, state)       \
    ret = digitalWrite(pin, state);           \
    if (ret != 0)                             \
    {                                         \
        IS_initDefaultIOPins_line = __LINE__; \
        return ret;                           \
    }

unsigned int IS_initDefaultIOPins_line = 0;
////////////////////////////////////////////////////////////////////////////////////////////////////
int IS_initDefaultIOPins(void)
{
    int ret;
#if defined(JWPLC_BASIC)
    RETURN_PINMODE(EN_IO, OUTPUT);
    RETURN_DIGITALWRITE(EN_IO, LOW);
    RETURN_PINMODE(PIN_I0_0, INPUT);
    RETURN_PINMODE(PIN_I0_1, INPUT);
    RETURN_PINMODE(PIN_I0_2, INPUT);
    RETURN_PINMODE(PIN_I0_3, INPUT);
    RETURN_PINMODE(PIN_I0_4, INPUT);
    RETURN_PINMODE(PIN_I0_5, INPUT);

    RETURN_PINMODE(PIN_Q0_0, OUTPUT);
    RETURN_PINMODE(PIN_Q0_1, OUTPUT);
    RETURN_PINMODE(PIN_Q0_2, OUTPUT);
    RETURN_PINMODE(PIN_Q0_3, OUTPUT);
    RETURN_PINMODE(PIN_Q0_4, OUTPUT);
    RETURN_PINMODE(PIN_Q0_5, OUTPUT);
    RETURN_PINMODE(PIN_Q0_6, OUTPUT);
    RETURN_PINMODE(PIN_Q0_7, OUTPUT);
    RETURN_PINMODE(PIN_Q0_8, OUTPUT);
    RETURN_PINMODE(PIN_Q0_9, OUTPUT);

    RETURN_DIGITALWRITE(PIN_Q0_0, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_1, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_2, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_3, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_4, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_5, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_6, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_7, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_8, LOW);
    RETURN_DIGITALWRITE(PIN_Q0_9, LOW);
    RETURN_DIGITALWRITE(EN_IO, HIGH);
#endif

    return 0;
}