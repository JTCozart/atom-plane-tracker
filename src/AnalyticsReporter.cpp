#include "AnalyticsReporter.h"
#include "version.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static const char* kEndpoint = "https://app.posthog.com/capture/";
static const char* kApiKey   = "phc_u3KvbsAKsLL7jghUPb9i59NpcW5uHrS7wNn9TELwCTrN";

void AnalyticsReporter::reportBoot() {
    char body[256];
    snprintf(body, sizeof(body),
        "{\"api_key\":\"%s\","
        "\"event\":\"device_boot\","
        "\"distinct_id\":\"%s\","
        "\"properties\":{"
        "\"firmware_version\":\"%s\"}}",
        kApiKey,
        WiFi.macAddress().c_str(),
        FIRMWARE_VERSION);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, kEndpoint);
    http.addHeader("Content-Type", "application/json");
    http.POST(body);
    http.end();
}
