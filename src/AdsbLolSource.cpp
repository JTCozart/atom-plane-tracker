#include "AdsbLolSource.h"
#include "AnalyticsReporter.h"
#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

bool AdsbLolSource::fetch(const Config& cfg, JsonDocument& out) {
    char url[128];
    snprintf(url, sizeof(url), kUrlFormat, cfg.latitude, cfg.longitude, (double)cfg.radius);

    WiFiClientSecure client;
    client.setInsecure();  // cert validation not available on embedded
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(kHttpTimeoutMs);

    Serial.printf("[SCAN] GET %s\n", url);
    int httpCode = http.GET();
    _lastResponseCode = httpCode;
    Serial.printf("[SCAN] HTTP %d\n", httpCode);

    if (httpCode != 200) {
        http.end();
        _consecutiveFailures++;
        Serial.printf("[SCAN] fail - consecutive failures: %d\n", _consecutiveFailures);
        char msg[16];
        snprintf(msg, sizeof(msg), "HTTP %d", httpCode);
        AnalyticsReporter::reportError("adsb_fetch", msg);
        return false;
    }

    DeserializationError err = deserializeJson(out, http.getStream());
    http.end();

    if (err) {
        _consecutiveFailures++;
        Serial.println("[SCAN] JSON parse error");
        AnalyticsReporter::reportError("adsb_parse", err.c_str());
        return false;
    }

    _consecutiveFailures = 0;
    return true;
}
