/*
  JWPLC Basic - Runtime_Escalonado

  Modo Runtime con una entrada, dos TON y dos salidas: muestra cómo una
  entrada digital dispara temporizadores.

  Programa «Escalonado» (se carga al arrancar si la FRAM tiene otro):
    B01 TON 1 s  entrada = I1  -> Q1
    B02 TON 2 s  entrada = I1  -> Q2
  Con I1 en 1, Q1 y Q2 se encienden en cascada a 1 y 2 s.
  Al soltar I1 se apagan las dos de inmediato.

  Qué conectar: un pulsador o interruptor en I0.0 (en JWPLC Sim, clic en I0.0).
  Qué mirar: en Programa > Monitor (en vivo) se ve I1, los dos TON contando
  y Q1/Q2 en una sola pantalla. Los tiempos se cambian en Parámetros.
  loadDemo() reemplaza el programa de la FRAM si es otro; si ya es el
  «Escalonado», lo deja como está.
*/

#include <JWPLC_Runtime.h>

void setup()
{
    JWPLC_Runtime.loadDemo(RUNTIME_DEMO_ESCALONADO);
    JWPLC_Runtime.begin();
    // Arranca en RUN sin pasar por MENU > INICIAR (quitarlo para respetar
    // el modo guardado: Configuracion > Arranque).
    JWPLC_Runtime.run();
}

void loop()
{
}
