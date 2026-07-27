#define JWPLC_UNIFIED_STRONG_IMPLEMENTATION
#include "RuntimeUIFBDMapUnifiedU4Internal.h"

using namespace JWPLCUnifiedU4;

extern "C" void jwplcUnifiedU4ResetForAttach(
    const RuntimeUIFBDMapUnified *owner,
    RuntimeUIV2ReadModel *model)
{
  resetU4(owner, model);
}
