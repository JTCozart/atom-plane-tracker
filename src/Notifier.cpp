#include "Notifier.h"
#include "Display.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>

static const char* classTag(AcClass cls) {
    switch (cls) {
        case CLS_MIL:    return "MIL";
        case CLS_MEDVAC: return "MEDVAC";
        case CLS_COMM:   return "COMM";
        default:         return "PRIV";
    }
}

void Notifier::send(const Ac& ac, const Config& cfg) {
    if (strlen(cfg.ntfyToken) == 0 || strlen(cfg.ntfyTopic) == 0) return;

    // If ntfyClasses is set, only notify for listed classes
    if (strlen(cfg.ntfyClasses) > 0 &&
        strstr(cfg.ntfyClasses, classTag(ac.cls)) == nullptr) return;

    const char* priority;
    const char* tags;
    switch (ac.cls) {
        case CLS_MIL:    priority = "urgent";  tags = "rotating_light"; break;
        case CLS_MEDVAC: priority = "high";    tags = "ambulance";      break;
        case CLS_COMM:   priority = "default"; tags = "airplane";       break;
        default:         priority = "low";     tags = "small_airplane"; break;
    }

    String cs = ac.callsign.length() ? ac.callsign : (ac.type.length() ? ac.type : "Unknown");
    char body[160];
    snprintf(body, sizeof(body), "%s (%s) at %.0f ft\nOwner: %s",
             cs.c_str(),
             ac.type.length() ? ac.type.c_str() : "???",
             ac.alt,
             ac.owner.length() ? ac.owner.c_str() : "Unknown");

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, String("https://ntfy.sh/") + cfg.ntfyTopic);
    http.addHeader("Authorization", String("Bearer ") + cfg.ntfyToken);
    http.addHeader("Content-Type",  "text/plain");
    http.addHeader("Title",    String(AC_LABELS[ac.cls]) + " Aircraft Detected");
    http.addHeader("Priority", priority);
    http.addHeader("Tags",     tags);
    http.POST(body);
    http.end();
}

int Notifier::sendTest(const Config& cfg, Display& display) {
    if (strlen(cfg.ntfyToken) == 0 || strlen(cfg.ntfyTopic) == 0) {
        display.showMessage("ntfy not configured");
        delay(1500);
        return 0;
    }

    display.showMessage("Sending test...");

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, String("https://ntfy.sh/") + cfg.ntfyTopic);
    http.addHeader("Authorization", String("Bearer ") + cfg.ntfyToken);
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
