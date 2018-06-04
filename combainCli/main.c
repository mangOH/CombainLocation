#include "legato.h"
#include "interfaces.h"

#include <stdio.h>

static struct
{
    bool helpRequested;
    bool useWifi;
    bool useCellular;
} CliArgs;

static struct
{
    ma_combainLocation_LocReqHandleRef_t combainHandle;
    bool waitingForWifiResults;
    bool waitingForCellularResults;
    le_wifiClient_NewEventHandlerRef_t wifiHandler;
} State;

static void Usage(FILE *stream)
{
    fprintf(stream, "Usage: ");
    fprintf(stream, le_arg_GetProgramName());
    fprintf(stream, "[-h|--help] [-w|--wifi] [-c|--cellular]\n");
}

static void LocationResultHandler(
    ma_combainLocation_LocReqHandleRef_t handle, ma_combainLocation_Result_t result, void *context)
{
    switch (result)
    {
    case MA_COMBAINLOCATION_RESULT_SUCCESS:
    {
        double latitude;
        double longitude;
        double accuracyInMeters;

        const le_result_t res = ma_combainLocation_GetSuccessResponse(
            handle, &latitude, &longitude, &accuracyInMeters);
        if (res != LE_OK)
        {
            fprintf(
                stderr,
                "Received result notification of type success response, but couldn't fetch the "
                "result.\n");
            exit(1);
        }
        else
        {
            printf("Location\n");
            printf(
                "  latitude=%f, longitude=%f, accuracy=%f meters\n",
                latitude,
                longitude,
                accuracyInMeters);
            exit(0);
        }
        break;
    }

    case MA_COMBAINLOCATION_RESULT_ERROR:
    {
        char firstDomain[65];
        char firstReason[65];
        char firstMessage[129];
        uint16_t code;
        char message[129];

        const le_result_t res = ma_combainLocation_GetErrorResponse(
            handle,
            firstDomain,
            sizeof(firstDomain) - 1,
            firstReason,
            sizeof(firstReason) - 1,
            firstMessage,
            sizeof(firstMessage) - 1,
            &code,
            message,
            sizeof(message) - 1);
        if (res != LE_OK)
        {
            fprintf(
                stderr,
                "Received result notification of type error response, but couldn't fetch the "
                "result.\n");
            exit(1);
        }
        else
        {
            fprintf(stderr, "Received an error response.\n");
            fprintf(stderr, "  firstDomain: %s\n", firstDomain);
            fprintf(stderr, "  firstReason: %s\n", firstReason);
            fprintf(stderr, "  firstMessage: %s\n", firstMessage);
            fprintf(stderr, "  code: %d\n", code);
            fprintf(stderr, "  message: %s\n", message);
            exit(1);
        }

        break;
    }

    case MA_COMBAINLOCATION_RESULT_RESPONSE_PARSE_FAILURE:
    {
        char rawResult[257];
        const le_result_t res = ma_combainLocation_GetParseFailureResult(
            handle, rawResult, sizeof(rawResult) - 1);
        if (res != LE_OK)
        {
            fprintf(
                stderr,
                "Received result notification of type response parse failure, but couldn't fetch "
                "the result.\n");
            exit(1);
        }
        else
        {
            fprintf(stderr, "Received a result which couldn't be parsed \"%s\"\n", rawResult);
            exit(1);
        }
        break;
    }

    case MA_COMBAINLOCATION_RESULT_COMMUNICATION_FAILURE:
        fprintf(stderr, "Couldn't communicate with Combain server\n");
        exit(1);
        break;

    default:
        fprintf(stderr, "Received unhandled result type (%d)\n", result);
        exit(1);
    }
}

static bool TrySubmitRequest(void)
{
    if (!State.waitingForWifiResults && !State.waitingForCellularResults)
    {
        LE_INFO("Attempting to submit location request");
        const le_result_t res = ma_combainLocation_SubmitLocationRequest(
            State.combainHandle, LocationResultHandler, NULL);
        if (res != LE_OK)
        {
            fprintf(stderr, "Failed to submit location request\n");
            exit(1);
        }
        return true;
    }

    LE_INFO("Not submitting location request yet");
    return false;
}

static bool MacAddrStringToBinary(const char* s, uint8_t *b)
{
    size_t inputOffset = 0;
    const size_t macStrLen = (6 * 2) + (6 - 1);
    while (inputOffset < macStrLen)
    {
        if (inputOffset % 3 == 2)
        {
            if (s[inputOffset] != ':')
            {
                return false;
            }
        }
        else
        {
            uint8_t nibble;
            if (s[inputOffset] >= '0' && s[inputOffset] <= '9')
            {
                nibble = s[inputOffset] - '0';
            }
            else if (s[inputOffset] >= 'a' && s[inputOffset] <= 'f')
            {
                nibble = 10 + s[inputOffset] - 'a';
            }
            else if (s[inputOffset] >= 'A' && s[inputOffset] <= 'F')
            {
                nibble = 10 + s[inputOffset] - 'A';
            }
            else
            {
                return false;
            }

            if (inputOffset % 3 == 0)
            {
                b[inputOffset / 3] = (nibble << 4);
            }
            else
            {
                b[inputOffset / 3] |= (nibble << 0);
            }
        }

        inputOffset++;
    }

    if (s[inputOffset] != '\0')
    {
        return false;
    }

    return true;
}

static uint16_t MccMncStrToInt(const char *s)
{
    size_t offset = 0;
    uint16_t out = 0;
    int mult = 1;
    while (s[offset] != '\0')
    {
        if (s[offset] >= '0' && s[offset] <= '9')
        {
            out += mult * (s[offset] - '0');
        }
        else
        {
            fprintf(stderr, "Bad MCC/MNC string '%s'", s);
            exit(1);
        }
        mult *= 10;
        offset++;
    }

    return out;
}

static void WifiEventHandler(le_wifiClient_Event_t event, void *context)
{
    LE_INFO("Called WifiEventHanler() with event=%d", event);
    switch (event)
    {
    case LE_WIFICLIENT_EVENT_SCAN_DONE:
        if (State.waitingForWifiResults)
        {
            State.waitingForWifiResults = false;

            le_wifiClient_AccessPointRef_t ap = le_wifiClient_GetFirstAccessPoint();
            while (ap != NULL)
            {
                uint8_t ssid[32];
                size_t ssidLen = sizeof(ssid);
                char bssid[(2 * 6) + (6 - 1) + 1]; // "nn:nn:nn:nn:nn:nn\0"
                int16_t signalStrength;
                le_result_t res;
                uint8_t bssidBytes[6];
                res = le_wifiClient_GetSsid(ap, ssid, &ssidLen);
                if (res != LE_OK)
                {
                    fprintf(stderr, "Failed while fetching WiFi SSID\n");
                    exit(1);
                }

                res = le_wifiClient_GetBssid(ap, bssid, sizeof(bssid) - 1);
                if (res != LE_OK)
                {
                    fprintf(stderr, "Failed while fetching WiFi BSSID\n");
                    exit(1);
                }

                // TODO: LE-10254 notes that an incorrect error code of 0xFFFF is mentioned in the
                // documentation. The error code used in the implementation is 0xFFF.
                signalStrength = le_wifiClient_GetSignalStrength(ap);
                if (signalStrength == 0xFFF)
                {
                    fprintf(stderr, "Failed while fetching WiFi signal strength\n");
                    exit(1);
                }

                if (!MacAddrStringToBinary(bssid, bssidBytes))
                {
                    fprintf(stderr, "WiFi scan contained invalid bssid=\"%s\"\n", bssid);
                    exit(1);
                }

                res = ma_combainLocation_AppendWifiAccessPoint(
                    State.combainHandle, bssidBytes, 6, ssid, ssidLen, signalStrength);
                if (res != LE_OK)
                {
                    fprintf(stderr, "Failed to append WiFi scan results to combain request\n");
                    exit(1);
                }

                ap = le_wifiClient_GetNextAccessPoint();
            }

            TrySubmitRequest();
        }
        break;

    case LE_WIFICLIENT_EVENT_SCAN_FAILED:
        fprintf(stderr, "WiFi scan failed\n");
        exit(1);
        break;

    default:
        // Do nothing - don't care about connect/disconnect events
        break;
    }
}

COMPONENT_INIT
{
    le_arg_SetFlagVar(&CliArgs.helpRequested, "h", "help");
    le_arg_SetFlagVar(&CliArgs.useWifi, "w", "wifi");
    le_arg_SetFlagVar(&CliArgs.useCellular, "c", "cellular");
    le_arg_Scan();

    if (CliArgs.helpRequested)
    {
        Usage(stdout);
        exit(0);
    }

    if (!CliArgs.useWifi && !CliArgs.useCellular)
    {
        fprintf(stderr, "Error: At least one scan type must be chosen.  See --help\n");
        Usage(stderr);
        exit(1);
    }

    State.combainHandle = ma_combainLocation_CreateLocationRequest();

    State.waitingForWifiResults = CliArgs.useWifi;
    State.waitingForCellularResults = CliArgs.useCellular;

    if (CliArgs.useWifi)
    {
        const le_result_t startRes = le_wifiClient_Start();
        if (startRes != LE_OK && startRes != LE_BUSY)
        {
            fprintf(stderr, "Couldn't start the WiFi service");
            exit(1);
        }
        State.wifiHandler = le_wifiClient_AddNewEventHandler(WifiEventHandler, NULL);
        le_wifiClient_Scan();
    }

    if (CliArgs.useCellular)
    {
        char mcc[LE_MRC_MCC_BYTES] = {0};
        char mnc[LE_MRC_MNC_BYTES] = {0};
        le_mrc_Rat_t rat;
        uint32_t cid;
    	uint32_t lac;
        le_mrc_MetricsRef_t metricsRef;
        int32_t rssi;
        uint32_t ber;
        uint32_t bler;
        int32_t rsrq;
        int32_t rsrp;
        int32_t ecio;
        int32_t rscp;
        int32_t sinr;
        int32_t io;
        uint32_t er;
        ma_combainLocation_CellularTech_t cellTech;

        if (le_mrc_GetCurrentNetworkMccMnc(mcc, sizeof(mcc), mnc, sizeof(mnc)) != LE_OK)
        {
            fprintf(stderr, "Couldn't get cellular MCC, MNC");
            exit(1);
        }

        if (le_mrc_GetRadioAccessTechInUse(&rat) != LE_OK)
        {
            fprintf(stderr, "Couldn't get cellular RAT");
            exit(1);
        }

        cid = le_mrc_GetServingCellId();
        if (cid == UINT32_MAX)
        {
            fprintf(stderr, "Couldn't get cellular cell Id");
            exit(1);
        }

        lac = le_mrc_GetServingCellLocAreaCode();
        if (lac == UINT32_MAX)
        {
            fprintf(stderr, "Couldn't get cellular location area code (LAC)");
            exit(1);
        }

        metricsRef = le_mrc_MeasureSignalMetrics();
        if (metricsRef == NULL)
        {
            fprintf(stderr, "Couldn't measure cellular signal metrics");
            exit(1);
        }

        switch (rat)
        {
        case LE_MRC_RAT_UNKNOWN:
            fprintf(stderr, "Unknown cellular RAT");
            exit(1);
            break;

        case LE_MRC_RAT_GSM:
            cellTech = MA_COMBAINLOCATION_CELL_TECH_GSM;
            if (le_mrc_GetGsmSignalMetrics(metricsRef, &rssi, &ber) != LE_OK)
            {
                fprintf(stderr, "Couldn't get GSM RSSI");
                exit(1);
            }
            break;

        case LE_MRC_RAT_UMTS:
            if (le_mrc_GetUmtsSignalMetrics(metricsRef, &rssi, &bler, &ecio, &rscp, &sinr) != LE_OK)
            {
                fprintf(stderr, "Couldn't get UMTS RSSI");
                exit(1);
            }
            fprintf(stderr, "UMTS is not supported by combain");
            exit(1);
            break;

        case LE_MRC_RAT_TDSCDMA:
            fprintf(stderr, "It isn't possible to get the RSSI for TDSCDMA");
            exit(1);
            break;

        case LE_MRC_RAT_LTE:
            cellTech = MA_COMBAINLOCATION_CELL_TECH_LTE;
            if (le_mrc_GetLteSignalMetrics(metricsRef, &rssi, &bler, &rsrq, &rsrp, &sinr) != LE_OK) 
            {
                fprintf(stderr, "Couldn't get LTE RSSI");
                exit(1);
            }
            break;

        case LE_MRC_RAT_CDMA:
            cellTech = MA_COMBAINLOCATION_CELL_TECH_CDMA;
            if (le_mrc_GetCdmaSignalMetrics(metricsRef, &rssi, &er, &ecio, &sinr, &io) != LE_OK) 
            {
                fprintf(stderr, "Couldn't get CDMA RSSI");
                exit(1);
            }
            break;

        default:
            fprintf(stderr, "Unsupported cellular RAT (%d)", rat);
            exit(1);
            break;
        }

        if (ma_combainLocation_AppendCellTower(
                State.combainHandle,
                cellTech,
                MccMncStrToInt(mcc),
                MccMncStrToInt(mnc),
                lac,
                cid,
                rssi) != LE_OK)
        {
            fprintf(stderr, "Failed to append WiFi scan results to combain request\n");
            exit(1);
        }

        TrySubmitRequest();
    }
}
