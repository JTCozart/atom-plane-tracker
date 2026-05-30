#pragma once
#include "Aircraft.h"
#include "Config.h"

// Forward declaration to avoid circular includes
class Display;

class Notifier {
public:
    // Send a push notification for the given aircraft (no-op if ntfy not configured).
    void send(const Ac& ac, const Config& cfg);

    // Send a test notification; shows feedback via display.showMessage().
    // Returns the HTTP response code.
    int sendTest(const Config& cfg, Display& display);
};
