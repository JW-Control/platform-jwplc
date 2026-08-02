#ifndef JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V12_H
#define JWPLC_LOGIC_RUNTIME_UI_FBD_MAP_V12_H

#include "RuntimeUIFBDMapV11.h"

/**
 * @brief Revisión candidata v0.5.9 del editor FBD.
 *
 * El mapa normal deja de ejecutar por completo el renderer de preview compacto
 * heredado de V8. Las listas de parámetros muestran hasta cuatro filas con
 * ventana desplazable e indicador de posición. Cuando un bloque solo tiene un
 * parámetro, PARAMETROS abre directamente su editor y evita una pantalla
 * intermedia sin utilidad.
 */
class RuntimeUIFBDMapV12 : public RuntimeUIFBDMapV11
{
public:
  RuntimeUIFBDMapV12();

  void attach(RuntimeUIV2ReadModel &model);
  void enter();
  void refresh(const JWPLC_IOState *io,
               const JWPLC_RTCState *rtc);
  void exit();
  void forceRedraw();

protected:
  static constexpr uint8_t PARAMETER_ROWS_VISIBLE = 4;
  static constexpr int16_t PARAMETER_POSITION_X = 278;
  static constexpr int16_t PARAMETER_POSITION_Y = 30;
  static constexpr uint8_t PARAMETER_POSITION_COLUMNS = 5;

  bool handleMainInput() override;
  void drawParameterListScreen() override;
  void drawParameterListRow(uint8_t parameterIndex,
                            bool selected) override;
  bool handleParameterListInput() override;
  void returnFromParameterEdit(bool accept) override;

private:
  void resetV12State();
  void syncIdleReturnModeV12();
  void ensureParameterListVisible();
  void drawParameterPosition();

  uint8_t _parameterListTop;
};

#endif
