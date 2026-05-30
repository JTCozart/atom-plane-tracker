#pragma once
#include <Arduino.h>
#include <vector>
#include "Aircraft.h"

class Display {
public:
    static constexpr int LINES_VISIBLE = 13;
    static constexpr int CONTENT_Y     = 20;

    // Initialise color tables — must be called after M5.begin()
    void begin();

    void drawScan();
    void drawAPMode();

    // Draw a live or history aircraft screen.
    // etaSecs: pre-computed ETA in seconds, or -1 = unknown (live only; ignored when hist=true).
    // histIdx / histTotal: used for the history bar when hist=true.
    void drawAircraft(const Ac& ac, bool hist, int histIdx, int histTotal, int etaSecs);

    void drawSummary(int mil, int medvac, int comm, int priv);

    void drawDebug(const std::vector<String>& lines, int scroll,
                   int httpCode, int total, const String& ip);

    // Generic 2-line message on a black background (connecting, test-ntfy feedback, etc.)
    void showMessage(const String& line1, const String& line2 = "");

    // Expose colors for WebUI color conversion
    uint16_t bgColor(AcClass c) const { return _bg[c]; }
    uint16_t fgColor(AcClass c) const { return _fg[c]; }

private:
    uint16_t _black{}, _white{}, _red{};
    uint16_t _bg[4]{};
    uint16_t _fg[4]{};

    void initColors();
};
