/*
  B_Tags.h · Nombres de E/S y variables globales

  QUÉ VA AQUÍ:    el nombre de cada entrada y salida (igual que en docs/MAPA_IO.md),
                  las variables que comparten las pestañas, los estados de la máquina
                  y las instancias de temporizadores/flancos.
  QUÉ NO VA AQUÍ: lecturas o escrituras físicas (C_Entradas / F_Salidas) ni lógica.

  Regla: en la lógica se usan SIEMPRE estos nombres, nunca I0_x / Q0_x directamente.
  Así, si un borne cambia en obra, sólo se corrige aquí y en docs/MAPA_IO.md.

  Los nombres de ejemplo (marcha/paro de un motor) se reemplazan por los del proyecto.
*/
#pragma once

// ---------------------------------------------------------------------------
// Entradas digitales: bit del bloque I0_0..I0_7 (bit 0 = I0_0)
// NA = normalmente abierto, NC = normalmente cerrado (cableado a prueba de fallos)
// ---------------------------------------------------------------------------
static constexpr uint8_t BIT_PB_MARCHA  = 0; // I0_0  Pulsador marcha (NA)
static constexpr uint8_t BIT_PB_PARO    = 1; // I0_1  Pulsador paro (NC)
static constexpr uint8_t BIT_EMERGENCIA = 2; // I0_2  Parada de emergencia (NC)
static constexpr uint8_t BIT_TERMICO    = 3; // I0_3  Relé térmico del motor (NC)
//                                              I0_4..I0_7 reserva

// Estado ya filtrado y con lógica positiva (true = condición presente). Lo llena C_Entradas.
bool pbMarcha = false;     // se pulsó marcha
bool pbParo = false;       // se pulsó paro (NC abierto)
bool emergencia = false;   // emergencia activa (NC abierto)
bool fallaTermico = false; // térmico disparado (NC abierto)

// ---------------------------------------------------------------------------
// Salidas digitales: bit del bloque Q0_0..Q0_7 (bit 0 = Q0_0)
// ---------------------------------------------------------------------------
static constexpr uint8_t BIT_MOTOR         = 0; // Q0_0  Contactor del motor
static constexpr uint8_t BIT_LAMPARA_FALLA = 1; // Q0_1  Piloto de falla
//                                                 Q0_2..Q0_7 reserva

// Lo que la lógica pide encender. F_Salidas lo escribe al final del ciclo.
bool motor = false;
bool lamparaFalla = false;

// ---------------------------------------------------------------------------
// Estados de la máquina (D_Logica)
// ---------------------------------------------------------------------------
enum EstadoMaquina : uint8_t
{
    ESTADO_PARADO = 0,
    ESTADO_ARRANCANDO,
    ESTADO_MARCHA,
    ESTADO_FALLA
};
EstadoMaquina estado = ESTADO_PARADO;

// ---------------------------------------------------------------------------
// Alarmas (E_Alarmas): un bit por alarma
// ---------------------------------------------------------------------------
enum CodigoAlarma : uint8_t
{
    ALM_EMERGENCIA = 0,
    ALM_TERMICO,
    ALM_CANTIDAD
};
uint16_t alarmasActivas = 0;     // bit = alarma presente ahora
uint16_t alarmasEnclavadas = 0;  // bit = ocurrió y nadie la reconoció
bool reconocerAlarmas = false;   // la pide G_HMI o H_Comunicaciones

// ---------------------------------------------------------------------------
// Imagen del ciclo (C_Entradas / F_Salidas). No usar fuera de esas pestañas.
// ---------------------------------------------------------------------------
uint8_t imagenEntradas = 0;
uint8_t imagenSalidas = 0;

// ---------------------------------------------------------------------------
// Temporizadores y flancos (K_Utilidades.h): una instancia por uso
// ---------------------------------------------------------------------------
Antirrebote filtroEntrada[8];
TON tArranque;          // tiempo de arranque antes de dar marcha por buena
Flanco flancoMarcha;
