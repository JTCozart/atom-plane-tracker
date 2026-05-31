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

static const char* kReleasesApi    =
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

    Serial.println("[OTA] check() started");
    Serial.printf ("[OTA] current version: %s\n", currentVersion());
    Serial.printf ("[OTA] GET %s\n", kReleasesApi);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, kReleasesApi);
    http.addHeader("User-Agent", "atom-plane-tracker");
    int code = http.GET();
    Serial.printf("[OTA] releases API response: %d\n", code);

    if (code != 200) {
        http.end();
        _status = "Check failed (HTTP " + String(code) + ")";
        Serial.printf("[OTA] check failed: %s\n", _status.c_str());
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
    if (err) {
        _status = "Parse error";
        Serial.printf("[OTA] JSON parse error: %s\n", err.c_str());
        return false;
    }

    _latestVersion = doc["tag_name"] | "";
    _releaseUrl    = doc["html_url"] | "";
    _downloadUrl   = "";

    Serial.printf("[OTA] latest tag: %s\n", _latestVersion.c_str());
    Serial.printf("[OTA] release url: %s\n", _releaseUrl.c_str());

    for (JsonObject asset : doc["assets"].as<JsonArray>()) {
        String name = asset["name"] | "";
        String url  = asset["browser_download_url"] | "";
        Serial.printf("[OTA] asset: %s\n", name.c_str());
        if (name.endsWith(".bin") && !name.endsWith("-initial-flash.bin")) {
            _downloadUrl = url;
            Serial.printf("[OTA] selected download url: %s\n", _downloadUrl.c_str());
            break;
        }
    }

    if (_downloadUrl.isEmpty()) {
        Serial.println("[OTA] no suitable .bin asset found");
    }

    if (_latestVersion.isEmpty()) {
        _status = "No release found";
        Serial.println("[OTA] no release found");
        return false;
    }

    String cur = currentVersion();
    _hasUpdate = !_downloadUrl.isEmpty() &&
                 (cur == "dev" || (_latestVersion != cur && _latestVersion > cur));
    _status    = _hasUpdate ? "Update available" : "Up to date";

    Serial.printf("[OTA] hasUpdate: %s  status: %s\n",
                  _hasUpdate ? "true" : "false", _status.c_str());
    return _hasUpdate;
}

bool OtaUpdater::apply() {
    Serial.println("[OTA] apply() started");
    Serial.printf ("[OTA] download url: %s\n", _downloadUrl.c_str());

    if (_downloadUrl.isEmpty()) {
        _status = "No download URL";
        Serial.println("[OTA] apply() aborted: no download URL");
        return false;
    }

    auto& disp = M5.Display;
    uint16_t black = disp.color565(0,   0,   0);
    uint16_t white = disp.color565(255, 255, 255);
    uint16_t green = disp.color565(0,   200, 0);
    uint16_t red   = disp.color565(255, 0,   0);

    auto showError = [&](const String& msg) {
        _status = msg;
        Serial.printf("[OTA] ERROR: %s\n", msg.c_str());
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
    client.setTimeout(60);   // WiFiClientSecure timeout in seconds
    HTTPClient http;
    http.begin(client, _downloadUrl);
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
    http.setTimeout(60000);  // HTTPClient timeout in milliseconds (fits uint16_t)

    Serial.println("[OTA] starting GET request...");
    int code = http.GET();
    Serial.printf("[OTA] download response: %d\n", code);

    if (code != 200) {
        http.end();
        showError("HTTP " + String(code));
        return false;
    }

    int total = http.getSize();
    Serial.printf("[OTA] content-length: %d bytes\n", total);

    Serial.printf("[OTA] free sketch space: %d bytes\n", ESP.getFreeSketchSpace());
    Serial.printf("[OTA] Update.begin(%d)...\n", total > 0 ? total : -1);

    if (!Update.begin(total > 0 ? total : UPDATE_SIZE_UNKNOWN)) {
        http.end();
        showError("Begin failed: " + String(Update.getError()));
        return false;
    }
    Serial.println("[OTA] Update.begin() OK");

    int lastLogPct = -1;
    Update.onProgress([&lastLogPct, &disp, black, white, green](size_t done, size_t size) {
        if (size == 0) return;
        int pct = (int)(done * 100 / size);
        disp.setTextColor(white, black);
        disp.setCursor(2, 34);
        disp.printf("%d%%  ", pct);
        disp.fillRect(2, 48, pct * 124 / 100, 8, green);
        if (pct / 10 != lastLogPct / 10) {
            lastLogPct = pct;
            Serial.printf("[OTA] progress: %d%% (%d / %d bytes)\n", pct, (int)done, (int)size);
        }
    });

    Serial.println("[OTA] calling Update.writeStream()...");
    size_t written = Update.writeStream(*http.getStreamPtr());
    http.end();
    Serial.printf("[OTA] writeStream() returned %d bytes written\n", (int)written);
    Serial.printf("[OTA] Update.progress(): %d\n", (int)Update.progress());
    Serial.printf("[OTA] Update.isFinished(): %s\n", Update.isFinished() ? "true" : "false");
    Serial.printf("[OTA] Update.hasError(): %s  error code: %d\n",
                  Update.hasError() ? "true" : "false", Update.getError());

    if (total > 0 && (int)written != total) {
        showError("Incomplete: " + String(written) + "/" + String(total));
        Update.abort();
        return false;
    }

    if (Update.hasError()) {
        showError("Write err: " + String(Update.getError()));
        return false;
    }

    if (!Update.isFinished()) {
        showError("Not finished");
        Update.abort();
        return false;
    }

    Serial.println("[OTA] calling Update.end()...");
    bool ended = Update.end();
    Serial.printf("[OTA] Update.end() returned: %s  error code: %d\n",
                  ended ? "true" : "false", Update.getError());
    Serial.printf("[OTA] Update.isFinished() after end: %s\n", Update.isFinished() ? "true" : "false");

    if (!ended) {
        showError("End err: " + String(Update.getError()));
        return false;
    }

    _status = "Update complete";
    Serial.println("[OTA] apply() SUCCESS - rebooting");
    return true;
}

