#include "RuntimeUIFBDMapActiveRenderer.h"

void RuntimeUIFBDMapActiveRenderer::drawCompactDetailHeader(bool force)
{
  // La cabecera unificada de RuntimeUIFBDMapActiveRenderer se dibuja una sola vez
  // al final del callback. V14 no debe escribir una posición provisional.
  (void)force;
}
