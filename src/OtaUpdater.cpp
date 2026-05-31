#include "OtaUpdater.h"
#include "version.h"
#include <M5Unified.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Update.h>

#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION "dev"
#endif

static const char* kReleasesApi =
    "https://api.github.com/repos/JTCozart/atom-plane-tracker/releases/latest";

const char* OtaUpdater::currentVersion() { return FIRMWARE_VERSION; }

bool OtaUpdater::isDue() const {
    if (!_everChecked) return millis() >= kFirstCheckMs;
    return (millis() - _lastCheckMs) >= kCheckIntervalMs;
}

bool OtaUpdater::check() {
    _everChecked = true;
    _lastCheckMs = millis();
    _hasUpdate   = false;
    _status      = "Checking...";

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, kReleasesApi);
    http.addHeader("User-Agent", "atom-plane-tracker");
    int code = http.GET();
    if (code != 200) {
        http.end();
        _status = "Check failed (HTTP " + String(code) + ")";
        return false;
    }

    JsonDocument filter;
    filter["tag_name"]                          = true;
    filter["html_url"]                          = true;
    filter["assets"][0]["name"]                 = true;
    filter["assets"][0]["browser_download_url"] = true;

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();
    if (err) { _status = "Parse error"; return false; }

    _latestVersion = doc["tag_name"] | "";
    _releaseUrl    = doc["html_url"] | "";
    _downloadUrl   = "";

    for (JsonObject asset : doc["assets"].as<JsonArray>()) {
        String name = asset["name"] | "";
        if (name.endsWith(".bin")) {
            _downloadUrl = asset["browser_download_url"] | "";
            break;
        }
    }

    if (_latestVersion.isEmpty()) { _status = "No release found"; return false; }

    String cur = currentVersion();
    // dev builds can always install a release; tagged builds update only when newer.
    // Tags are date-based (vYYYYMMDD.HHMM) so lexicographic comparison is correct.
    _hasUpdate = !_downloadUrl.isEmpty() &&
                 (cur == "dev" || (_latestVersion != cur && _latestVersion > cur));
    _status    = _hasUpdate ? "Update available" : "Up to date";
    return _hasUpdate;
}

bool OtaUpdater::apply() {
    if (_downloadUrl.isEmpty()) { _status = "No download URL"; return false; }

    auto& disp = M5.Display;
    uint16_t black = disp.color565(0,   0,   0);
    uint16_t white = disp.color565(255, 255, 255);
    uint16_t green = disp.color565(0,   200, 0);
    uint16_t red   = disp.color565(255, 0,   0);

    auto showError = [&](const String& msg) {
        _status = msg;
        disp.fillScreen(red);
        disp.setTextColor(black, red);
        disp.setTextSize(1);
        disp.setCursor(2, 4);
        disp.print("OTA FAILED");
        disp.setCursor(2, 20);
        disp.print(msg.substring(0, 21));
    };

    disp.fillScreen(black);
    disp.setTextColor(white, black);
    disp.setTextSize(1);
    disp.setCursor(2, 4);  disp.print("UPDATING...");
    disp.setCursor(2, 16); disp.print(_latestVersion);

    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(120000);
    HTTPClient http;
    http.begin(client, _downloadUrl);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(120000);

    int code = http.GET();
    if (code != 200) {
        http.end();
        showError("HTTP " + String(code));
        return false;
    }

    int total = http.getSize(); // -1 if server omits Content-Length

    if (!Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN)) {
        http.end();
        showError("Begin failed: " + String(Update.getError()));
        return false;
    }

    Update.onProgress([&disp, black, white, green](size_t done, size_t size) {
        if (size == 0) return;
        int pct = (int)(done * 100 / size);
        disp.setTextColor(white, black);
        disp.setCursor(2, 34);
        disp.printf("%d%%  ", pct);
        disp.fillRect(2, 48, pct * 124 / 100, 8, green);
    });

    Update.writeStream(*http.getStreamPtr());
    http.end();

    if (Update.hasError()) {
        showError("Write err: " + String(Update.getError()));
        return false;
    }

    if (!Update.end(true)) {
        showError("End err: " + String(Update.getError()));
        return false;
    }

    _status = "Update complete";
    return true;
}
