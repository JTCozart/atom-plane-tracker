#pragma once
#include <Arduino.h>

class OtaUpdater {
public:
    static const char* currentVersion();

    // Hits GitHub releases API. Returns true if a newer release is available.
    bool check();

    // Streams the release binary into flash via the Arduino Update library.
    // Shows progress on the physical display. Device must reboot after success.
    bool apply();


    // True when it is time to run check() - 60 s after boot, then every 24 h.
    bool isDue() const;

    bool          hasUpdate()     const { return _hasUpdate; }
    const String& latestVersion() const { return _latestVersion; }
    const String& statusMessage() const { return _status; }
    const String& releaseUrl()    const { return _releaseUrl; }

private:
    static constexpr uint32_t kFirstCheckMs    =     60000UL; // 60 s after boot
    static constexpr uint32_t kCheckIntervalMs = 86400000UL;  // 24 h

    String   _latestVersion;
    String   _downloadUrl;
    String   _releaseUrl;
    bool     _hasUpdate   = false;
    bool     _everChecked = false;
    uint32_t _lastCheckMs = 0;
    String   _status      = "Not checked yet";
};
