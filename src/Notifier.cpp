#include "Notifier.h"
#include "Display.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static const char* classFilterTag(AircraftClass cls) {
    switch (cls) {
        case AircraftClass::Military:   return "MIL";
        case AircraftClass::Medevac:    return "MEDVAC";
        case AircraftClass::Commercial: return "COMM";
        default:                        return "PRIV";
    }
}

void Notifier::notifyDetection(const Aircraft& aircraft, const Config& config) {
    if (strlen(config.notifyToken) == 0 || strlen(config.notifyTopic) == 0) return;

    // If notifyClassFilter is set, only notify for listed classes
    if (strlen(config.notifyClassFilter) > 0 &&
        strstr(config.notifyClassFilter, classFilterTag(aircraft.classification)) == nullptr) return;

    const char* priority;
    const char* tags;
    switch (aircraft.classification) {
        case AircraftClass::Military:   priority = "urgent";  tags = "rotating_light"; break;
        case AircraftClass::Medevac:    priority = "high";    tags = "ambulance";      break;
        case AircraftClass::Commercial: priority = "default"; tags = "airplane";       break;
        default:                        priority = "low";     tags = "small_airplane"; break;
    }

    String cs = aircraft.callsign.length() ? aircraft.callsign : (aircraft.type.length() ? aircraft.type : "Unknown");
    char body[128];
    snprintf(body, sizeof(body), "%s (%s) at %.0f ft",
             cs.c_str(),
             aircraft.type.length() ? aircraft.type.c_str() : "???",
             aircraft.altitude);

    // FlightRadar24 link by ICAO hex
    String fr24Url = "https://www.flightradar24.com/" + aircraft.icao;

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, String("https://ntfy.sh/") + config.notifyTopic);
    http.addHeader("Authorization", String("Bearer ") + config.notifyToken);
    http.addHeader("Content-Type",  "text/plain");
    http.addHeader("Title",    String(aircraftClassName(aircraft.classification)) + " Aircraft Detected");
    http.addHeader("Priority", priority);
    http.addHeader("Tags",     tags);
    http.addHeader("Action",   "view, FlightRadar24, " + fr24Url + ", clear=true");
    http.POST(body);
    http.end();
}

int Notifier::sendTestNotification(const Config& config, Display& display) {
    if (strlen(config.notifyToken) == 0 || strlen(config.notifyTopic) == 0) {
        display.showMessage("ntfy not configured");
        delay(1500);
        return 0;
    }

    display.showMessage("Sending test...");

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, String("https://ntfy.sh/") + config.notifyTopic);
    http.addHeader("Authorization", String("Bearer ") + config.notifyToken);
    http.addHeader("Content-Type",  "text/plain");
    http.addHeader("Title",    "atom-plane-tracker test");
    http.addHeader("Priority", "default");
    http.addHeader("Tags",     "white_check_mark");
    int code = http.POST("Test notification from atom-plane-tracker");
    http.end();

    char buf[16];
    snprintf(buf, sizeof(buf), "HTTP %d", code);
    display.showMessage("Sending test...", String(buf));
    delay(1500);
    return code;
}
