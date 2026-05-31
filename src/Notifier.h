#pragma once
#include "Aircraft.h"
#include "Config.h"

// Forward declaration to avoid circular includes
class Display;

class Notifier {
public:
    // Send a push notification for the given aircraft (no-op if ntfy not configured).
    void notifyDetection(const Aircraft& aircraft, const Config& config);

    // Send a test notification; shows feedback via display.showMessage().
    // Returns the HTTP response code.
    int sendTestNotification(const Config& config, Display& display);

    // Core HTTP send - shared by sendTestNotification() and WebUI.
    // Returns the HTTP response code, or 0 if ntfy is not configured.
    static int sendTestHttp(const Config& config);
};
