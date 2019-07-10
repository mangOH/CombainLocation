#include <curl/curl.h>
#include "CombainHttp.h"
#include "legato.h"
#include "interfaces.h"

static ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string, std::string>> *RequestJson;
static ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string>> *ResponseJson;
static le_event_Id_t ResponseAvailableEvent;

static struct ReceiveBuffer
{
    size_t used;
    uint8_t data[1024];
} HttpReceiveBuffer;

void CombainHttpInit(
    ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string, std::string>> *requestJson,
    ThreadSafeQueue<std::tuple<ma_combainLocation_LocReqHandleRef_t, std::string>> *responseJson,
    le_event_Id_t responseAvailableEvent)
{
    RequestJson = requestJson;
    ResponseJson = responseJson;
    ResponseAvailableEvent = responseAvailableEvent;
    CURLcode res = curl_global_init(CURL_GLOBAL_ALL);
    LE_ASSERT(res == 0);
}

void CombainHttpDeinit(void)
{
    curl_global_cleanup();
}


static size_t WriteMemCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    auto b = reinterpret_cast<ReceiveBuffer *>(userp);
    const size_t numBytes = nmemb * size;

    const size_t available = sizeof(b->data) - (b->used + 1);
    const size_t numToCopy = std::min(available, numBytes);
    memcpy(&b->data[b->used], contents, numToCopy);
    b->used += numToCopy;
    b->data[b->used] = 0;
    return numToCopy;
}

void *CombainHttpThreadFunc(void *context)
{
    do {
        auto t = RequestJson->dequeue();
        ma_combainLocation_LocReqHandleRef_t handle = std::get<0>(t);
        std::string &combainApiKey = std::get<1>(t);
        std::string &requestBody = std::get<2>(t);

        char combainUrl[128] = "https://cps.combain.com?key=";
        strncat(combainUrl, combainApiKey.c_str(), sizeof(combainUrl) - (1 + strlen(combainUrl)));

        CURL* curl = curl_easy_init();
        LE_ASSERT(curl);

        LE_ASSERT(curl_easy_setopt(curl, CURLOPT_URL, combainUrl) == CURLE_OK);

        struct curl_slist *httpHeaders = NULL;
        httpHeaders = curl_slist_append(httpHeaders, "Content-Type:application/json");
        LE_ASSERT(httpHeaders != NULL);
        LE_ASSERT(curl_easy_setopt(curl, CURLOPT_HTTPHEADER, httpHeaders) == CURLE_OK);

        LE_ASSERT(curl_easy_setopt(curl, CURLOPT_COPYPOSTFIELDS, requestBody.c_str()) == CURLE_OK);

        LE_ASSERT(curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemCallback) == CURLE_OK);
        LE_ASSERT(curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&HttpReceiveBuffer) == CURLE_OK);

        const CURLcode res = curl_easy_perform(curl);
        if (res != CURLE_OK)
        {
            LE_ERROR("libcurl returned error (%d): %s", res, curl_easy_strerror(res));
            // TODO: better way to encode CURL errors?
            ResponseJson->enqueue(std::make_tuple(handle, ""));
        }

        std::string json((char*)HttpReceiveBuffer.data, HttpReceiveBuffer.used);
        ResponseJson->enqueue(std::make_tuple(handle, json));
        le_event_Report(ResponseAvailableEvent, NULL, 0);

        curl_easy_cleanup(curl);

        curl_slist_free_all(httpHeaders);

    } while (true);
}
