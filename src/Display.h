#pragma once
#include <M5Unified.h>
#include <Arduino.h>
#include <vector>
#include <map>
#include "Aircraft.h"

class Display {
public:
    static constexpr int kVisibleLineCount = 10;
    static constexpr int kContentStartY    = 30;

    struct DiagnosticInfo {
        int      responseCode;
        int      aircraftCount;
        String   ipAddress;
        uint32_t uptimeSeconds;
        String   version;
    };

    // Must be called after M5.begin() — initialises colors and the off-screen canvas.
    void begin();

    void showSplash(int frame);
    void showScanning();
    void showSetupMode();

    // Draw a live aircraft screen.
    // etaSeconds: pre-computed ETA in seconds, or -1 if unknown.
    void showLiveAircraft(const Aircraft& aircraft, int etaSeconds,
                          double queryLat, double queryLon);

    // Draw a history aircraft screen with a history counter banner.
    void showHistoryAircraft(const Aircraft& aircraft, int historyIndex, int historyCount,
                             double queryLat, double queryLon);

    void showSummary(int militaryCount, int medevacCount, int commercialCount, int privateCount);

    void showRadar(const std::map<String, Aircraft>& aircraft,
                   double queryLat, double queryLon, float queryRadius);

    void showDebug(const std::vector<String>& lines, int scrollOffset, const DiagnosticInfo& info);

    void showMessage(const String& line1, const String& line2 = "");
    void showLostConnection(int retryCount);

    // secondsRemaining: countdown until the interval is auto-corrected.
    void showPollIntervalError(uint32_t currentMs, uint32_t minimumMs, int secondsRemaining);

    void setUpdateAvailable(bool v) { _updateAvailable = v; }

    uint16_t backgroundColorFor(AircraftClass cls) const { return _backgroundColors[toIndex(cls)]; }
    uint16_t foregroundColorFor(AircraftClass cls) const { return _foregroundColors[toIndex(cls)]; }

    // Converts an RGB565 color (as used by M5 Display) to a CSS hex string.
    static String rgb565ToCss(uint16_t c);

private:
    uint16_t  _colorBlack{}, _colorWhite{}, _colorRed{};
    uint16_t  _backgroundColors[4]{};
    uint16_t  _foregroundColors[4]{};
    uint8_t   _scanAnimFrame{0};
    bool      _updateAvailable{false};
    M5Canvas* _scanCanvas{nullptr};  // off-screen sprite — pushSprite() eliminates per-draw flicker

    void initColors();
    void drawAircraftScreen(const Aircraft& aircraft, bool isHistorical,
                            int historyIndex, int historyCount, int etaSeconds,
                            double queryLat, double queryLon);
};
