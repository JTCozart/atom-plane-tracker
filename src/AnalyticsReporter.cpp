#include "AnalyticsReporter.h"
#include "version.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static const char* kEndpoint = "https://app.posthog.com/capture/";
static const char* kApiKey   = "phc_u3KvbsAKsLL7jghUPb9i59NpcW5uHrS7wNn9TELwCTrN";

void AnalyticsReporter::send(const char* event) {
    char body[256];
    snprintf(body, sizeof(body),
        "{\"api_key\":\"%s\","
        "\"event\":\"%s\","
        "\"distinct_id\":\"%s\","
        "\"properties\":{"
        "\"firmware_version\":\"%s\"}}",
        kApiKey,
        event,
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

void AnalyticsReporter::reportBoot()      { send("device_boot"); }
void AnalyticsReporter::reportHeartbeat() { send("device_heartbeat"); }
