#include "CombainLocationRequest.h"
#include <stdexcept>

WifiApScanItem::WifiApScanItem(
    const uint8_t *bssid,
    size_t bssidLen,
    const uint8_t *ssid,
    size_t ssidLen,
    int16_t signalStrength)
{
    if (bssidLen != 6)
    {
        throw std::runtime_error("BSSID length must be 6");
    }
    std::copy(&bssid[0], &bssid[3], &this->bssid[0]);

    if (ssidLen > sizeof(this->ssid))
    {
        throw std::runtime_error("SSID is too long");
    }
    std::copy(&ssid[0], &ssid[ssidLen], &this->ssid[0]);
    this->ssidLen = ssidLen;

    if (signalStrength >= 0)
    {
        throw std::runtime_error("Signal strength should be negative");
    }
    this->signalStrength = signalStrength;
}

CombainLocationRequest::CombainLocationRequest(void)
    : responseHandler(NULL),
      responseHandlerContext(NULL)
{
}

CombainLocationRequest::~CombainLocationRequest(void)
{
}

void CombainLocationRequest::appendWifiAccessPoint(const WifiApScanItem& ap)
{
    this->wifiAps.push_back(ap);
}

le_result_t CombainLocationRequest::submit(
    ma_combainLocation_LocationResponseHandlerFunc_t responseHandler, void *context)
{
    if (this->responseHandler)
    {
        // Must have been submitted previously
        return LE_BUSY;
    }

    if (!this->isValid())
    {
        return LE_BAD_PARAMETER;
    }

    this->responseHandler = responseHandler;
    this->responseHandlerContext = context;

    // TODO: Perform HTTP request to combain API

    return LE_OK;
}

le_result_t CombainLocationRequest::getSuccessResponse(const SuccessResponse **response)
{
    le_result_t res = LE_OK;

    if (this->responseType == Response::SUCCESS)
    {
        *response = &this->successResponse;
    }
    else
    {
        res = LE_NOT_FOUND;
    }

    return res;
}

le_result_t CombainLocationRequest::getErrorResponse(const ErrorResponse **response)
{
    le_result_t res = LE_OK;
    if (this->responseType == Response::ERROR)
    {
        *response = &this->errorResponse;
    }
    else
    {
        res = LE_NOT_FOUND;
    }

    return res;
}

//---------------- PRIVATE

bool CombainLocationRequest::isValid(void) const
{
    // TODO: Add more sophisticated validity checking
    return !this->wifiAps.empty();
}
