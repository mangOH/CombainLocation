#ifndef COMBAIN_LOCATION_API_H
#define COMBAIN_LOCATION_API_H

#include "legato.h"
#include "interfaces.h"
#include "ThreadSafeQueue.h"

// A bit gross.. use globals to allow CombainHttp.cpp to access these variables owned by
// combainLocationApi.cpp
extern ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string>> RequestJson;
extern ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string>> ResponseJson;
extern le_event_Id_t ResponseAvailableEvent;

#endif // COMBAIN_LOCATION_API_H
