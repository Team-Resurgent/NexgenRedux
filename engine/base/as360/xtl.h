#pragma once

// What AngelScript's 360 branch of as_atomic.cpp asks of <xtl.h>: see windows.h beside it.
#include "windows.h"

#ifdef __cplusplus
extern "C" {
#endif
LONG nx360InterlockedIncrement(LONG volatile* value);
LONG nx360InterlockedDecrement(LONG volatile* value);
#ifdef __cplusplus
}
#endif

#define InterlockedIncrement nx360InterlockedIncrement
#define InterlockedDecrement nx360InterlockedDecrement
