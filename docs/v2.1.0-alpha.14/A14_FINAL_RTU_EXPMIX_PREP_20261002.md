# Alpha14 — RTU final R3 EXP-MIX — PREP 2026-10-02

## Objetivo

Caracterizar el ceiling RTU FAST con dos workloads representativos de ocho
módulos de expansión. Cada patrón usa exactamente ocho transacciones por scan
y corre 300 s sin TCP.

### Pattern A — 3DI + 3DO + 1AI + 1AO

- 3 × FC02, 8 bits;
- 3 × FC0F, 8 bits;
- 1 × FC04, 4 registros;
- 1 × FC10, 2 registros.

### Pattern B — 2DI + 2DO + 2AI + 2AO

- 2 × FC02, 8 bits;
- 2 × FC0F, 8 bits;
- 2 × FC04, 4 registros;
- 2 × FC10, 2 registros.

## Alcance físico

El gate usa un Slave físico con ocho slots lógicos distribuidos en sus mapas.
Esto reproduce número/tipo/tamaño de ADU y presupuesto del bus. No equivale a
tener ocho dispositivos físicos independientes ni reemplaza la evidencia
multidrop histórica.

## Perfil FAST

- 500000 baud;
- Master FIFO 9;
- Slave FIFO 8;
- RX BULK/BULK;
- TX QUEUED/QUEUED;
- Master GAP;
- Slave STRUCTURAL;
- frame gap 100 us;
- CRC BITWISE;
- TCP OFF.

## Política de verificación

R2 ya validó cada FC con readback. R3 no inserta readbacks adicionales dentro
de la ventana para no alterar el workload real de ocho módulos.

Durante R3:

- FC02 y FC04 verifican cada payload recibido;
- FC0F y FC10 exigen respuesta Modbus limpia;
- al terminar cada patrón se compara el mapa final de outputs esperado por el
  Master contra coils/holding reales del Slave;
- cross-count Master/Slave debe ser exacto;
- CRC, timeout, exception, rejected y verify deben ser 0;
- no puede haber reboot;
- stop quiescente sólo ocurre en frontera de scan completo.

## Métricas principales

- transacciones RTU/s;
- scans completos/s;
- periodo por scan en ms;
- transaction max;
- rate por clase DI/DO/AI/AO.

Si pasa, R4 utilizará estos workloads para medir la frontera TCP x RTU con
full-runtime y targets TCP 100/250/500/750/1000 req/s.