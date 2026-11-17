/*
  JWPLC Basic - Runtime_Intermitente

  El ejemplo más sencillo del modo Runtime: solo salidas y dos TON, sin
  entradas. Sirve para mostrar el Runtime sin cables ni entradas simuladas.

  Programa «Intermitente» (se carga al arrancar si la FRAM tiene otro):
    B01 TON 1 s  entrada = M1 negada  -> Q1
    B02 TON 1 s  entrada = B01        -> M1 (apaga a B01 y el ciclo vuelve a empezar)
    Q1 = B01 y Q2 = B01 negada: Q1 y Q2 se alternan cada segundo.

  Los dos bloques y las salidas caben en una pantalla del mapa: en
  Programa > Monitor (en vivo) se ve el ciclo completo sin moverse. Los tiempos se cambian en Parámetros.
  loadDemo() reemplaza el programa de la FRAM si es otro (por ejemplo la
  «Demo feria»); si ya es el «Intermitente», lo deja como está.

  Qué mirar: Q0.0 y Q0.1 se encienden por turnos cada 1 s desde el arranque.
*/

#include <JWPLC_Runtime.h>

void setup()
{
    JWPLC_Runtime.loadDemo(RUNTIME_DEMO_INTERMITENTE);
    JWPLC_Runtime.begin();
    // Arranca en RUN sin pasar por MENU > INICIAR (quitarlo para respetar
    // el modo guardado: Configuracion > Arranque).
    JWPLC_Runtime.run();
}

void loop()
{
}
