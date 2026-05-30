#include "Config.h"
#include <Preferences.h>
#include "secrets.h"

void Config::load() {
    Preferences prefs;
    prefs.begin("plantracker", true);
    String storedSsid = prefs.getString("ssid", "");
    if (storedSsid.length() > 0) {
        strncpy(ssid, storedSsid.c_str(),                             sizeof(ssid) - 1);
        strncpy(pass, prefs.getString("pass", WIFI_PASSWORD).c_str(), sizeof(pass) - 1);
        lat    = prefs.getDouble("lat",    QUERY_LAT);
        lon    = prefs.getDouble("lon",    QUERY_LON);
        radius = prefs.getFloat ("radius", QUERY_RADIUS_NM);
        pollMs = prefs.getUInt  ("pollMs", POLL_INTERVAL_MS);
    } else {
        strncpy(ssid, WIFI_SSID,     sizeof(ssid) - 1);
        strncpy(pass, WIFI_PASSWORD, sizeof(pass) - 1);
        lat    = QUERY_LAT;
        lon    = QUERY_LON;
        radius = QUERY_RADIUS_NM;
        pollMs = POLL_INTERVAL_MS;
    }
    // ntfy settings always loaded independently (may be set without WiFi reconfiguration)
    strncpy(ntfyToken,   prefs.getString("ntfyToken",   NTFY_TOKEN).c_str(),   sizeof(ntfyToken)   - 1);
    strncpy(ntfyTopic,   prefs.getString("ntfyTopic",   NTFY_TOPIC).c_str(),   sizeof(ntfyTopic)   - 1);
    strncpy(ntfyClasses, prefs.getString("ntfyClasses", NTFY_CLASSES).c_str(), sizeof(ntfyClasses) - 1);
    prefs.end();
}
