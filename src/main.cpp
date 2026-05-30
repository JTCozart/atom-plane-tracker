#include <M5Unified.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "secrets.h"

// airplanes.live — free, no key, ADSBExchange v2 format
// radius is in nautical miles
static const char* API_URL_FMT =
    "https://api.adsb.one/v2/point/%.6f/%.6f/%.1f";

// Fallback: adsb.lol (same format, same endpoint structure)
// "https://api.adsb.lol/v2/lat/%.6f/lon/%.6f/dist/%.1f"

static const uint32_t POLL_INTERVAL_MS = 10000;

// ── helpers ──────────────────────────────────────────────────────────────────

static void connectWifi() {
    M5.Display.println("Connecting WiFi...");
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    uint8_t attempts = 0;
    while (WiFi.status() != WL_CONNECTED && attempts < 30) {
        delay(500);
        attempts++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        M5.Display.println("WiFi OK");
    } else {
        M5.Display.println("WiFi FAILED");
    }
}

static void fetchAndDisplay() {
    if (WiFi.status() != WL_CONNECTED) {
        connectWifi();
        return;
    }

    char url[128];
    snprintf(url, sizeof(url), API_URL_FMT,
             (double)QUERY_LAT, (double)QUERY_LON, (double)QUERY_RADIUS_NM);

    HTTPClient http;
    http.begin(url);
    http.setTimeout(8000);
    int code = http.GET();

    if (code != 200) {
        M5.Display.setCursor(0, 0);
        M5.Display.printf("HTTP %d\n", code);
        http.end();
        return;
    }

    // Stream parse — keeps RAM low on ESP32
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream());
    http.end();

    if (err) {
        M5.Display.setCursor(0, 0);
        M5.Display.println("JSON err");
        return;
    }

    JsonArray ac = doc["ac"].as<JsonArray>();
    int count = ac.size();

    M5.Display.fillScreen(BLACK);
    M5.Display.setCursor(0, 0);
    M5.Display.setTextSize(1);
    M5.Display.printf("Aircraft: %d\n", count);

    // Show up to the first few aircraft that fit on screen
    int row = 1;
    for (JsonObject plane : ac) {
        if (row > 6) break;  // 128px / ~18px per row
        const char* flight   = plane["flight"] | plane["r"] | "???";
        float       alt_ft   = plane["alt_baro"] | 0.0f;
        float       gs       = plane["gs"]       | 0.0f;

        M5.Display.printf("%.8s %5.0fft\n", flight, alt_ft);
        row++;
    }

    if (count == 0) {
        M5.Display.println("No traffic");
    }
}

// ── Arduino entry points ──────────────────────────────────────────────────────

void setup() {
    auto cfg = M5.config();
    M5.begin(cfg);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(WHITE, BLACK);
    M5.Display.fillScreen(BLACK);
    M5.Display.println("atom-plane-tracker");

    connectWifi();
    fetchAndDisplay();
}

void loop() {
    M5.update();
    static uint32_t lastPoll = 0;
    if (millis() - lastPoll >= POLL_INTERVAL_MS) {
        lastPoll = millis();
        fetchAndDisplay();
    }
}
