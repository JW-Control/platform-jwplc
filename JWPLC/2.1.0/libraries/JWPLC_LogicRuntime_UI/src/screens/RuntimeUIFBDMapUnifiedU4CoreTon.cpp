#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

#include <cstdio>
#include <cstring>

#include "../widgets/RuntimeUIWidgets.h"

using namespace JWPLCLogicRuntimeUIWidgets;

namespace JWPLCUnifiedU4
{
const char *mainFocusName(U4MainFocus focus)
{
  switch (focus)
  {
  case U4MainFocus::Sources:
    return "FUENTES";
  case U4MainFocus::Parameters:
    return "PARAMETROS";
  case U4MainFocus::Create:
  default:
    return "CREAR";
  }
}

bool mainFocusSelectable(U4MainFocus focus)
{
  if (focus == U4MainFocus::Sources)
    return sourceCountForType(gU4.type) > 0;
  if (focus == U4MainFocus::Parameters)
    return parameterCountForType(gU4.type) > 0;
  return true;
}

void normalizeMainFocus()
{
  if (mainFocusSelectable(U4MainFocus::Sources))
    gU4.mainFocus = U4MainFocus::Sources;
  else if (mainFocusSelectable(U4MainFocus::Parameters))
    gU4.mainFocus = U4MainFocus::Parameters;
  else
    gU4.mainFocus = U4MainFocus::Create;
}

void moveMainFocus(bool forward)
{
  uint8_t current = static_cast<uint8_t>(gU4.mainFocus);
  for (uint8_t step = 0; step < 3; ++step)
  {
    current = forward
                  ? static_cast<uint8_t>((current + 1U) % 3U)
                  : (current == 0 ? 2U : static_cast<uint8_t>(current - 1U));
    const U4MainFocus candidate = static_cast<U4MainFocus>(current);
    if (mainFocusSelectable(candidate))
    {
      gU4.mainFocus = candidate;
      return;
    }
  }
}

uint32_t u4TonMinorMaximum(U4TonBase base)
{
  return base == U4TonBase::Seconds ? 99UL : 59UL;
}

uint32_t tonMilliseconds()
{
  switch (gU4.tonBase)
  {
  case U4TonBase::Minutes:
    return (gU4.tonMajor * 60UL + gU4.tonMinor) * 1000UL;
  case U4TonBase::Hours:
    return (gU4.tonMajor * 60UL + gU4.tonMinor) * 60000UL;
  case U4TonBase::Seconds:
  default:
    return (gU4.tonMajor * 100UL + gU4.tonMinor) * 10UL;
  }
}

const char *tonMajorLabel()
{
  switch (gU4.tonBase)
  {
  case U4TonBase::Minutes:
    return "MIN";
  case U4TonBase::Hours:
    return "HORA";
  case U4TonBase::Seconds:
  default:
    return "SEG";
  }
}

const char *tonMinorLabel()
{
  switch (gU4.tonBase)
  {
  case U4TonBase::Minutes:
    return "SEG";
  case U4TonBase::Hours:
    return "MIN";
  case U4TonBase::Seconds:
  default:
    return "CENT";
  }
}

const char *tonBaseText()
{
  return gU4.tonBase == U4TonBase::Seconds
             ? "<s>"
             : (gU4.tonBase == U4TonBase::Minutes ? "<m>" : "<h>");
}

void changeTonBase(bool forward)
{
  uint8_t base = static_cast<uint8_t>(gU4.tonBase);
  base = forward
             ? static_cast<uint8_t>((base + 1U) % 3U)
             : (base == 0 ? 2U : static_cast<uint8_t>(base - 1U));
  gU4.tonBase = static_cast<U4TonBase>(base);
  const uint32_t maximum = u4TonMinorMaximum(gU4.tonBase);
  if (gU4.tonMinor > maximum)
    gU4.tonMinor = maximum;
}

void formatTonConfigured(char *destination, size_t capacity)
{
  if (destination == nullptr || capacity == 0)
    return;
  const char suffix = gU4.tonBase == U4TonBase::Seconds
                          ? 's'
                          : (gU4.tonBase == U4TonBase::Minutes ? 'm' : 'h');
  std::snprintf(destination,
                capacity,
                "%02lu:%02lu%c",
                static_cast<unsigned long>(gU4.tonMajor),
                static_cast<unsigned long>(gU4.tonMinor),
                suffix);
}

void formatResource(char *destination, size_t capacity)
{
  if (destination == nullptr || capacity == 0)
    return;
  if (gU4.resource == 0xFFU)
  {
    std::snprintf(destination, capacity, "SIN RECURSO");
    return;
  }
  std::snprintf(destination,
                capacity,
                gU4.type == U4Type::DigitalInput ? "I0.%u" : "Q0.%u",
                static_cast<unsigned>(gU4.resource));
}

} // namespace JWPLCUnifiedU4
