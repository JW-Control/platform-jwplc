# JWPLC Basic Desktop Simulator v0.1

## Estado

```text
PROPUESTA FUTURA
NO INICIADA
NO BLOQUEA EL DESARROLLO ACTUAL DEL JWPLC LOGIC RUNTIME
```

Este documento conserva la propuesta para desarrollar más adelante un simulador industrial propio del JWPLC Basic. El trabajo actual debe continuar enfocado en el editor gráfico de bloques y el runtime lógico.

## Motivación

Evitar una dependencia mensual de plataformas externas de simulación y disponer de una herramienta privada, extensible y orientada específicamente al ecosistema JWPLC.

La propuesta no busca emular completamente el ESP32. El objetivo es ejecutar una versión de escritorio de la lógica compatible con las APIs principales del package y visualizar un entorno industrial interactivo.

## Objetivo de v0.1

Crear una aplicación Electron que represente un JWPLC Basic virtual y permita probar programas de automatización básicos con dispositivos industriales gráficos.

Alcance inicial:

- JWPLC Basic virtual;
- 8 entradas digitales;
- 8 salidas digitales;
- panel de estados;
- ejecución, pausa y reset;
- cableado lógico entre dispositivos y E/S;
- guardar y abrir proyectos;
- consola de eventos;
- motor de simulación determinista;
- modo virtual sin hardware físico.

## Dispositivos de v0.1

### Entradas

- pulsador normalmente abierto;
- pulsador normalmente cerrado;
- selector de dos posiciones;
- selector de tres posiciones;
- fin de carrera;
- sensor inductivo;
- sensor fotoeléctrico.

### Salidas

- lámpara piloto;
- zumbador;
- relé auxiliar;
- contactor;
- electroválvula;
- motor.

## Evolución prevista

Después del MVP se podrán agregar:

- banda transportadora;
- cilindro neumático;
- tanque;
- bomba;
- puerta automática;
- contactos auxiliares;
- fallas configurables;
- escenas de máquinas;
- grabación y reproducción de pruebas;
- generación de informes;
- hardware-in-the-loop por USB con un JWPLC Basic real.

## Arquitectura propuesta

```text
apps/
├── desktop-electron/
├── simulator-engine/
├── jwplc-virtual-board/
└── device-library/
```

### desktop-electron

Responsable de:

- interfaz gráfica;
- área de trabajo;
- drag and drop;
- inspector de propiedades;
- animaciones;
- gestión de proyectos;
- consola y diagnóstico.

### simulator-engine

Responsable de:

- reloj de simulación;
- scan determinista;
- señales digitales;
- cola de eventos;
- pausa, reset y velocidad;
- actualización de dispositivos.

El motor no debe depender de Electron ni del renderer.

### jwplc-virtual-board

Responsable de:

- 8 DI;
- 8 DO;
- estados RUN/ERR/BUS/ETH cuando corresponda;
- TFT virtual futura;
- botonera virtual futura;
- FRAM virtual futura;
- RTC virtual futuro;
- SD virtual futura.

### device-library

Cada dispositivo debe exponer una interfaz equivalente a:

```ts
interface SimulatorDevice {
  id: string;
  type: string;
  inputs: SignalPort[];
  outputs: SignalPort[];
  state: Record<string, unknown>;
  parameters: Record<string, unknown>;
  update(deltaMs: number): void;
  serialize(): object;
}
```

## Integración con lógica Arduino

### Primera estrategia

Compilar una versión de escritorio de las APIs necesarias:

```text
digitalRead()
digitalWrite()
millis()
delay()
JW_FRAM
JW_RTC
JWPLC_Display
```

No se promete compatibilidad universal con todas las librerías Arduino durante v0.1.

Las librerías no soportadas deben informarse explícitamente al usuario.

### Estrategia posterior: HIL

```text
Electron Simulator
        ↕ USB/Serial
JWPLC Basic físico
```

En este modo el firmware real corre en el JWPLC Basic y la aplicación virtualiza sensores, pulsadores, actuadores y procesos.

## Persistencia virtual futura

- FRAM: archivo binario con tamaño y offsets compatibles;
- RTC: reloj del sistema o reloj acelerado de simulación;
- microSD: carpeta seleccionada por el usuario;
- escenas y proyectos: archivos JSON versionados.

## Iteraciones estimadas

### MVP funcional

```text
8 a 10 iteraciones
```

Incluye placa virtual, DI/DO, dispositivos básicos, ejecución y proyectos.

### Versión usable para talleres y usuarios

```text
14 a 18 iteraciones acumuladas
```

Incluye cableado más completo, diagnóstico, fallas, FRAM/RTC/SD y mejor editor de propiedades.

### Simulador industrial ampliado

```text
25 a 35 iteraciones acumuladas
```

Incluye procesos animados, escenarios automáticos, HIL, informes y biblioteca industrial extensible.

## Ejercicios objetivo

- Start/Stop;
- inversión de giro;
- semáforo;
- bomba por niveles;
- puerta automática;
- arranque estrella-triángulo;
- banda transportadora con sensores;
- alarmas y enclavamientos.

## Decisiones

1. Usar Electron por experiencia previa con JW Serial, JW Modbus Tool y Digital Twin Laundry.
2. No construir un emulador completo de ESP32.
3. Mantener motor, frontend y biblioteca de dispositivos desacoplados.
4. Implementar primero modo virtual y después HIL.
5. No mezclar este desarrollo con el cierre actual del editor FBD del JWPLC Logic Runtime.

## Punto de reanudación futuro

Antes de programar debe cerrarse un documento de arquitectura detallado que defina:

- formato de proyecto;
- protocolo entre renderer y motor;
- compatibilidad Arduino soportada;
- modelo de scan;
- contrato de dispositivos;
- estrategia de compilación de sketches;
- límites de v0.1;
- criterios de aceptación.
