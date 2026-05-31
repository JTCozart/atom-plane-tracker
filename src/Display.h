#pragma once
#include <M5Unified.h>
#include <Arduino.h>
#include <vector>
#include "Aircraft.h"

class Display {
public:
    static constexpr int kVisibleLineCount = 12;
    static constexpr int kContentStartY    = 30;

    // Initialise color tables - must be called after M5.begin()
    void begin();

    void showSplash(int frame);
    void showScanning();
    void showSetupMode();

    // Draw a live or history aircraft screen.
    // etaSeconds: pre-computed ETA in seconds, or -1 = unknown (live only; ignored when isHistorical=true).
    // historyIndex / historyCount: used for the history bar when isHistorical=true.
    void showAircraft(const Aircraft& aircraft, bool isHistorical, int historyIndex, int historyCount, int etaSeconds);

    void showSummary(int militaryCount, int medevacCount, int commercialCount, int privateCount);

    void showDebug(const std::vector<String>& lines, int scrollOffset,
                   int responseCode, int aircraftCount, const String& ipAddress,
                   uint32_t uptimeSeconds);

    // Generic 2-line message on a black background (connecting, test-ntfy feedback, etc.)
    void showMessage(const String& line1, const String& line2 = "");

    // Red screen shown when the API cannot be reached.
    void showLostConnection(int retryCount);

    // Red error screen shown when poll interval is below the minimum.
    // secondsRemaining: countdown until auto-correction (shown to user).
    void showPollIntervalError(uint32_t currentMs, uint32_t minimumMs, int secondsRemaining);

    // Expose colors for WebUI color conversion
    uint16_t backgroundColorFor(AircraftClass cls) const { return _backgroundColors[toIndex(cls)]; }
    uint16_t foregroundColorFor(AircraftClass cls) const { return _foregroundColors[toIndex(cls)]; }

private:
    uint16_t  _colorBlack{}, _colorWhite{}, _colorRed{};
    uint16_t  _backgroundColors[4]{};
    uint16_t  _foregroundColors[4]{};
    uint8_t   _scanAnimFrame{0};
    M5Canvas* _scanCanvas{nullptr};  // off-screen sprite - eliminates flicker

    void initColors();
};
