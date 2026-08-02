#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;
using namespace JWPLCUnifiedU4;

extern "C" void jwplcUnifiedU4HeaderAnchor() {}

RuntimeUIFBDMapUnified::HeaderModel
RuntimeUIFBDMapUnified::buildHeaderModel() const
{
  ensureU4(this, _model);
  HeaderModel model{};

  if (_view == View::Map && gU4.addSelected)
  {
    std::snprintf(model.title, sizeof(model.title), "MAPA FBD");
    std::snprintf(model.line1, sizeof(model.line1), "B%02u NUEVO",
                  static_cast<unsigned>(_model != nullptr ? _model->blockCount() : 0U));
    std::snprintf(model.line2, sizeof(model.line2), "OK CONFIG");
  }
  else if (_view == View::AddType)
  {
    std::snprintf(model.title, sizeof(model.title), "NUEVO");
    std::snprintf(model.line1, sizeof(model.line1), "B%02u %s",
                  static_cast<unsigned>(_model != nullptr ? _model->blockCount() : 0U),
                  typeShort(gU4.type));
    std::snprintf(model.line2, sizeof(model.line2), "TIPO");
  }
  else if (_view == View::Configure ||
           _view == View::SourceList ||
           _view == View::SourceEdit ||
           _view == View::ParameterList ||
           _view == View::ParameterEdit)
  {
    const char *title = "CONFIGURAR";
    const char *stage = mainFocusName(gU4.mainFocus);
    if (_view == View::SourceList)
    {
      title = "FUENTES";
      stage = sourceRole(gU4.type, gU4.sourceInput);
    }
    else if (_view == View::SourceEdit)
    {
      title = "EDITAR IN";
      stage = sourceRole(gU4.type, gU4.sourceInput);
    }
    else if (_view == View::ParameterList)
    {
      title = "PARAMETROS";
      stage = parameterName(gU4.type);
    }
    else if (_view == View::ParameterEdit)
    {
      title = gU4.type == U4Type::Ton ? "EDITAR T" : "EDITAR P";
      stage = parameterName(gU4.type);
    }

    std::snprintf(model.title, sizeof(model.title), "%s", title);
    std::snprintf(model.line1, sizeof(model.line1), "B%02u %s",
                  static_cast<unsigned>(_model != nullptr ? _model->blockCount() : 0U),
                  typeShort(gU4.type));
    std::snprintf(model.line2, sizeof(model.line2), "%s", stage);
  }
  else
  {
    const char *title = "DETALLE";
    if (_view == View::Map)
      title = "MAPA FBD";
    else if (_view == View::EditInput)
      title = "EDITAR IN";
    else if (_view == View::EditTon)
      title = "EDITAR T";
    std::snprintf(model.title, sizeof(model.title), "%s", title);

    const LogicV2BlockRecord *definition =
        _model != nullptr ? _model->block(_selectedIndex) : nullptr;
    if (definition == nullptr)
    {
      std::snprintf(model.line1, sizeof(model.line1), "SIN PROGRAMA");
      model.line2[0] = '\0';
    }
    else
    {
      std::snprintf(model.line1,
                    sizeof(model.line1),
                    "B%02u %s",
                    static_cast<unsigned>(_selectedIndex),
                    _model->typeShort(definition->type));
      if (_view == View::Map)
      {
        std::snprintf(model.line2,
                      sizeof(model.line2),
                      "%s",
                      _model->blockValue(_selectedIndex) ? "ON" : "OFF");
      }
      else if ((_view == View::EditTon ||
                _detailFocus == DetailFocus::Parameters) &&
               selectedBlockHasParameter())
      {
        std::snprintf(model.line2, sizeof(model.line2), "PARAM T");
      }
      else if (definition->inputCount > 0)
      {
        const char *role = _model->inputRole(definition->type, _detailInputIndex);
        if (role != nullptr && role[0] != '\0')
          std::snprintf(model.line2, sizeof(model.line2), "%s", role);
        else
          std::snprintf(model.line2,
                        sizeof(model.line2),
                        "IN%u/%u",
                        static_cast<unsigned>(_detailInputIndex + 1U),
                        static_cast<unsigned>(definition->inputCount));
      }
      else
      {
        model.line2[0] = '\0';
      }
    }
  }

  std::snprintf(model.state, sizeof(model.state), "%s", stateText());
  model.stateColor = stateColor();
  return model;
}
