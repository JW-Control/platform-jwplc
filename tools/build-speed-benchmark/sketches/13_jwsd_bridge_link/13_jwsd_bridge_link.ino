#include <JW_SD.h>

// Gate Alpha5:
// tomar direcciones de miembros de JW_SD obliga al linker a extraer
// JW_SD.cpp.o del archive precompilado.
//
// No se ejecuta begin(), por lo que este gate es únicamente estructural.
using JWSDBeginFn = bool (JW_SD::*)();
using JWSDPresentFn = bool (JW_SD::*)() const;

static JWSDBeginFn volatile jwSdBeginFn =
    static_cast<JWSDBeginFn>(&JW_SD::begin);

static JWSDPresentFn volatile jwSdPresentFn =
    &JW_SD::isCardPresent;

void setup()
{
    (void)jwSdBeginFn;
    (void)jwSdPresentFn;
}

void loop()
{
}
