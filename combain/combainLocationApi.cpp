#include "legato.h"
#include "interfaces.h"
#include <list>
#include <stdexcept>
#include <memory>
#include <algorithm>
#include "CombainLocationRequest.h"


struct RequestRecord
{
    ma_combainLocation_LocReqHandleRef_t handle;
    le_msg_SessionRef_t clientSession;
    CombainLocationRequest request;
};

// Just use a list for all of the requests because it's very unlikely that there will be more than
// one or two active at a time.
static std::list<RequestRecord> Requests;

static ma_combainLocation_LocReqHandleRef_t GenerateHandle(void);
static RequestRecord* GetRequestRecordFromHandle(ma_combainLocation_LocReqHandleRef_t handle);


ma_combainLocation_LocReqHandleRef_t ma_combainLocation_CreateLocationRequest
(
    void
)
{
    Requests.emplace_back();
    auto& r = Requests.back();
    r.handle = GenerateHandle();
    r.clientSession = ma_combainLocation_GetClientSessionRef();

    return r.handle;
}

le_result_t ma_combainLocation_AppendWifiAccessPoint
(
    ma_combainLocation_LocReqHandleRef_t handle,
    const uint8_t *bssid,
    size_t bssidLen,
    const uint8_t *ssid,
    size_t ssidLen,
    int16_t signalStrength
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    std::unique_ptr<WifiApScanItem> ap;
    try {
        ap.reset(new WifiApScanItem(bssid, bssidLen, ssid, ssidLen, signalStrength));
    }
    catch (std::runtime_error& e)
    {
        LE_ERROR("Failed to append AP info: %s", e.what());
        return LE_BAD_PARAMETER;
    }
    requestRecord->request.appendWifiAccessPoint(*ap);

    return LE_OK;
}

le_result_t ma_combainLocation_SubmitLocationRequest
(
    ma_combainLocation_LocReqHandleRef_t handle,
    ma_combainLocation_LocationResponseHandlerFunc_t responseHandler,
    void *context
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    return LE_OK;
}

void ma_combainLocation_DestroyLocationRequest
(
    ma_combainLocation_LocReqHandleRef_t handle
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle);
    if (!requestRecord)
    {
        return;
    }

    // TODO
}

le_result_t ma_combainLocation_GetSuccessResponse
(
    ma_combainLocation_LocReqHandleRef_t  handle,
    double *latitude,
    double *longitude,
    double *accuracyInMeters
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    const SuccessResponse *r;
    le_result_t res = requestRecord->request.getSuccessResponse(&r);
    if (res != LE_OK)
    {
        return LE_UNAVAILABLE;
    }

    *latitude = r->latitude;
    *longitude = r->longitude;
    *accuracyInMeters = r->accuracyInMeters;

    return LE_OK;
}

le_result_t ma_combainLocation_GetErrorResponse
(
    ma_combainLocation_LocReqHandleRef_t  handle,
    char *firstDomain,
    size_t firstDomainLen,
    char *firstReason,
    size_t firstReasonLen,
    char *firstMessage,
    size_t firstMessageLen,
    uint16_t *code,
    char *message,
    size_t messageLen
)
{
    RequestRecord *requestRecord = GetRequestRecordFromHandle(handle);
    if (!requestRecord)
    {
        return LE_BAD_PARAMETER;
    }

    const ErrorResponse *r;
    le_result_t res = requestRecord->request.getErrorResponse(&r);
    if (res != LE_OK)
    {
        return LE_UNAVAILABLE;
    }

    *code = r->code;
    strncpy(message, r->message.c_str(), messageLen - 1);
    message[messageLen - 1] = '\0';

    auto it = r->errors.begin();
    if (it != r->errors.end())
    {
        strncpy(firstDomain, it->domain.c_str(), firstDomainLen - 1);
        firstDomain[firstDomainLen - 1] = '\0';

        strncpy(firstReason, it->reason.c_str(), firstReasonLen - 1);
        firstReason[firstReasonLen - 1] = '\0';

        strncpy(firstMessage, it->message.c_str(), firstMessageLen - 1);
        firstMessage[firstMessageLen - 1] = '\0';
    }

    return LE_OK;
}

//--------------------------------------------------------------------------------------------------
/**
 * A handler for client disconnects which frees all resources associated with the client.
 */
//--------------------------------------------------------------------------------------------------
static void ClientSessionClosedHandler
(
    le_msg_SessionRef_t clientSession,
    void* context
)
{
    // TODO: iterate over Requests list matching clientSession
}

static ma_combainLocation_LocReqHandleRef_t GenerateHandle(void)
{
    static uint32_t next = 1;

    ma_combainLocation_LocReqHandleRef_t h = (ma_combainLocation_LocReqHandleRef_t)next;
    next += 2;
    return h;
}

static RequestRecord* GetRequestRecordFromHandle(ma_combainLocation_LocReqHandleRef_t handle)
{
    auto it = std::find_if(
        Requests.begin(),
        Requests.end(),
        [handle] (const RequestRecord& r) -> bool {
            return (
                r.handle == handle &&
                r.clientSession == ma_combainLocation_GetClientSessionRef());
        });
    return (it == Requests.end()) ? NULL : &(*it);
}

COMPONENT_INIT
{
    // Register a handler to be notified when clients disconnect
    le_msg_AddServiceCloseHandler(
        ma_combainLocation_GetServiceRef(), ClientSessionClosedHandler, NULL);
}
