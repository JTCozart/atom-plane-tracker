#include "Config.h"
#include <Preferences.h>
#include "secrets.h"

void Config::load() {
    Preferences prefs;
    prefs.begin("plantracker", true);
    String storedSsid = prefs.getString("ssid", "");
    if (storedSsid.length() > 0) {
        strncpy(ssid,     storedSsid.c_str(),                                  sizeof(ssid)      - 1);
        strncpy(password, prefs.getString("pass", WIFI_PASSWORD).c_str(),      sizeof(password)  - 1);
        latitude         = prefs.getDouble("lat",    QUERY_LAT);
        longitude        = prefs.getDouble("lon",    QUERY_LON);
        radius           = prefs.getFloat ("radius", QUERY_RADIUS_NM);
        pollIntervalMs   = prefs.getUInt  ("pollMs", POLL_INTERVAL_MS);
    } else {
        strncpy(ssid,     WIFI_SSID,     sizeof(ssid)     - 1);
        strncpy(password, WIFI_PASSWORD, sizeof(password) - 1);
        latitude       = QUERY_LAT;
        longitude      = QUERY_LON;
        radius         = QUERY_RADIUS_NM;
        pollIntervalMs = POLL_INTERVAL_MS;
    }
    // ntfy settings always loaded independently (may be set without WiFi reconfiguration)
    strncpy(notifyToken,       prefs.getString("ntfyToken",   NTFY_TOKEN).c_str(),   sizeof(notifyToken)       - 1);
    strncpy(notifyTopic,       prefs.getString("ntfyTopic",   NTFY_TOPIC).c_str(),   sizeof(notifyTopic)       - 1);
    strncpy(notifyClassFilter, prefs.getString("ntfyClasses", NTFY_CLASSES).c_str(), sizeof(notifyClassFilter) - 1);
    notifyUpdates          = prefs.getBool("ntfyUpdates",    true);
    notifyEmergencySquawk  = prefs.getBool("ntfyEmergSquawk", true);
    notifyPoi              = prefs.getBool("ntfyPoi",         false);
    strncpy(poiTypes, prefs.getString("poiTypes", "").c_str(), sizeof(poiTypes) - 1);
    poiEnabled          = prefs.getBool("poiEnabled",   false);
    strncpy(webPasswordHash, prefs.getString("webPassHash", "").c_str(), sizeof(webPasswordHash) - 1);
    strncpy(webPasswordSalt, prefs.getString("webSalt",     "").c_str(), sizeof(webPasswordSalt) - 1);
    requireWebPassword  = prefs.getBool("reqWebPass",   false);
    prefs.end();
}

void Config::savePollInterval(uint32_t ms) {
    pollIntervalMs = ms;
    Preferences prefs;
    prefs.begin("plantracker", false);
    prefs.putUInt("pollMs", ms);
    prefs.end();
}
