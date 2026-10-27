/*
  I_Persistencia · Datos que sobreviven a un corte de energía (FRAM)

  QUÉ VA AQUÍ:    la struct Persistentes con consignas, contadores y horómetros, y cuándo
                  se guardan. La FRAM no se gasta como una Flash, pero aun así se escribe
                  sólo cuando algo cambió.
  QUÉ NO VA AQUÍ: valores fijos del proyecto (A_Config.h).

  Si agregas o quitas campos de Persistentes, sube FRAM_VERSION_DATOS en A_Config.h:
  al arrancar se detecta el cambio y se cargan los valores por defecto.
*/

struct Persistentes
{
    uint32_t arranques;     // veces que se encendió el equipo
    uint32_t horasMarcha;   // horómetro del motor (h)
    uint32_t segundosMarcha;
};

Persistentes persistentes = {};
static Persistentes persistentesGuardadas = {};
static bool framDisponible = false;

static void persistentesPorDefecto()
{
    persistentes = {};
}

void persistenciaCargar()
{
    framDisponible = JWPLC_FRAM.size() > 0;
    if (!framDisponible || !JWPLC_FRAM.readBlock(FRAM_DIR_PERSISTENTES, persistentes, FRAM_VERSION_DATOS))
    {
        persistentesPorDefecto();
        diagnosticoMensaje("FRAM", framDisponible ? "datos por defecto" : "no disponible");
    }
    persistentes.arranques++;
    persistentesGuardadas = {}; // fuerza el primer guardado
}

void guardarPersistencia()
{
    // Horómetro: suma un segundo por cada segundo con el motor en marcha.
    static uint32_t ultimoSegundoMs = 0;
    if ((uint32_t)(millis() - ultimoSegundoMs) >= 1000)
    {
        ultimoSegundoMs = millis();
        if (estado == ESTADO_MARCHA && ++persistentes.segundosMarcha >= 3600)
        {
            persistentes.segundosMarcha = 0;
            persistentes.horasMarcha++;
        }
    }

    if (!framDisponible)
        return;

    static uint32_t ultimoGuardadoMs = 0;
    if ((uint32_t)(millis() - ultimoGuardadoMs) < T_GUARDADO_MINIMO_MS)
        return;
    if (memcmp(&persistentes, &persistentesGuardadas, sizeof(Persistentes)) == 0)
        return;

    ultimoGuardadoMs = millis();
    if (JWPLC_FRAM.writeBlock(FRAM_DIR_PERSISTENTES, persistentes, FRAM_VERSION_DATOS))
        persistentesGuardadas = persistentes;
    else
        diagnosticoMensaje("FRAM", "error al guardar");
}
