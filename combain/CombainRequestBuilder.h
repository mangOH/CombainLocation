#ifndef COMBAIN_REQUEST_BUILDER_H
#define COMBAIN_REQUEST_BUILDER_H

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


class CombainRequestBuilder
{
public:
    void appendWifiAccessPoint(const WifiApScanItem& ap);
    std::string generateRequestBody(void) const;

private:

    std::list<WifiApScanItem> wifiAps;
};

#endif // COMBAIN_REQUEST_BUILDER_H
