# Mapa de E/S del proyecto

Proyecto: ______________________ · Tablero: ____________ · Revisión: ____ · Fecha: __________

Mantener igual que `B_Tags.h`. Si un borne cambia en obra, se corrige aquí y en `B_Tags.h`.

## Entradas digitales

| Borne | Bit | Tag (B_Tags.h) | Descripción | Contacto | Observación |
|---|---|---|---|---|---|
| I0_0 | 0 | `pbMarcha` | Pulsador marcha | NA | ejemplo |
| I0_1 | 1 | `pbParo` | Pulsador paro | NC | ejemplo |
| I0_2 | 2 | `emergencia` | Parada de emergencia | NC | ejemplo |
| I0_3 | 3 | `fallaTermico` | Relé térmico del motor | NC | ejemplo |
| I0_4 | 4 | | reserva | | |
| I0_5 | 5 | | reserva | | |
| I0_6 | 6 | | reserva | | |
| I0_7 | 7 | | reserva | | |

## Salidas a relé

| Borne | Bit | Tag (B_Tags.h) | Descripción | Carga | Observación |
|---|---|---|---|---|---|
| Q0_0 | 0 | `motor` | Contactor del motor | bobina 220 VAC | ejemplo |
| Q0_1 | 1 | `lamparaFalla` | Piloto de falla | | ejemplo |
| Q0_2 | 2 | | reserva | | |
| Q0_3 | 3 | | reserva | | |
| Q0_4 | 4 | | reserva | | |
| Q0_5 | 5 | | reserva | | |
| Q0_6 | 6 | | reserva | | |
| Q0_7 | 7 | | reserva | | |

## Alarmas

| Bit | Código ERR | Causa | Acción del operador |
|---|---|---|---|
| 0 | EMG | Parada de emergencia abierta | Liberar la seta, ESC para reconocer |
| 1 | TER | Térmico disparado | Rearmar el térmico, ESC para reconocer |

## Mapa Modbus (RTU y TCP)

Holding registers, dirección = índice (40001 = 0). ID RTU: ___ · Baudios: _____ · Unit ID TCP: ___ · IP: _______________

| Registro | Dirección | Contenido | Acceso |
|---|---|---|---|
| 0 | 40001 | Estado de la máquina (0 parado, 1 arrancando, 2 marcha, 3 falla) | L |
| 1 | 40002 | Alarmas activas (bit = alarma) | L |
| 2 | 40003 | Alarmas enclavadas | L |
| 3 | 40004 | Entradas I0_0..I0_7 | L |
| 4 | 40005 | Salidas Q0_0..Q0_7 | L |
| 5 | 40006 | Órdenes: bit 0 = reconocer alarmas | E |
| 6..15 | 40007..40016 | reserva | |

## FRAM

| Dirección | Contenido | Versión |
|---|---|---|
| 0 | `struct Persistentes` (arranques, horómetro) | `FRAM_VERSION_DATOS` = 1 |
