#ifndef JWPLC_GLOBAL_PERIPHERALS_AUTO_H
#define JWPLC_GLOBAL_PERIPHERALS_AUTO_H

#ifndef JWPLC_LIBRARY_DISCOVERY_PHASE
#define JWPLC_LIBRARY_DISCOVERY_PHASE 0
#endif

// This header belongs to JWPLC_GlobalPeripherals so Arduino Builder can
// discover the library without expanding its full public header repeatedly.
//
// During library discovery, unique JWPLC bundled markers are imported first.
// This pins the validated copies shipped with the JWPLC platform before
// homonymous libraries from the user's sketchbook can be selected.
//
// During the normal build the complete public API remains unchanged.

#if JWPLC_LIBRARY_DISCOVERY_PHASE

#include <JWPLC_Bundled_JWPLC_Ethernet.h>

#endif

#if !JWPLC_LIBRARY_DISCOVERY_PHASE
#include <JWPLC_GlobalPeripherals.h>
#endif

#endif // JWPLC_GLOBAL_PERIPHERALS_AUTO_H
