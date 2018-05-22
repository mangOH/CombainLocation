#ifndef COMBAIN_LOCATION_REQUEST_H
#define COMBAIN_LOCATION_REQUEST_H

#include "legato.h"
#include "interfaces.h"
#include <string>
#include <list>

struct WifiApScanItem
{
    WifiApScanItem(
        const uint8_t *bssid,
        size_t bssidLen,
        const uint8_t *ssid,
        size_t ssidLen,
        int16_t signalStrength);
    uint8_t bssid[6];
    uint8_t ssid[32];
    size_t ssidLen;
    int16_t signalStrength;
};

struct SuccessResponse
{
    double latitude;
    double longitude;
    double accuracyInMeters;
};

struct CombainError
{
    std::string domain;
    std::string reason;
    std::string message;
};

struct ErrorResponse
{
    std::list<CombainError> errors;
    uint16_t code;
    std::string message;
};

enum class Response
{
    NOT_SET,
    SUCCESS,
    ERROR
};

class CombainLocationRequest
{
public:
    CombainLocationRequest(void);
    CombainLocationRequest(CombainLocationRequest&& from) = default;
    virtual ~CombainLocationRequest(void);
    void appendWifiAccessPoint(const WifiApScanItem& ap);
    le_result_t submit(ma_combainLocation_LocationResponseHandlerFunc_t responseHandler, void *context);
    le_result_t getSuccessResponse(const SuccessResponse **response);
    le_result_t getErrorResponse(const ErrorResponse **response);

private:
    bool isValid(void) const;


    std::list<WifiApScanItem> wifiAps;
    ma_combainLocation_LocationResponseHandlerFunc_t responseHandler;
    void *responseHandlerContext;
    Response responseType;
    ErrorResponse errorResponse;
    SuccessResponse successResponse;
};

#endif // COMBAIN_LOCATION_REQUEST_H
