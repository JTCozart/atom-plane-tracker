#include "Display.h"
#include <M5Unified.h>
#include <cmath>
#include <freertos/task.h>

// ── Color initialisation ─────────────────────────────────────────────────────

void Display::initColors() {
    _colorBlack = M5.Display.color565(0,   0,   0);
    _colorWhite = M5.Display.color565(255, 255, 255);
    _colorRed   = M5.Display.color565(255, 0,   0);

    _backgroundColors[toIndex(AircraftClass::Military)]   = M5.Display.color565(255, 0,   0);
    _backgroundColors[toIndex(AircraftClass::Medevac)]    = M5.Display.color565(0,   0,   255);
    _backgroundColors[toIndex(AircraftClass::Commercial)] = M5.Display.color565(0,   255, 0);
    _backgroundColors[toIndex(AircraftClass::Private)]    = M5.Display.color565(255, 255, 0);

    _foregroundColors[toIndex(AircraftClass::Military)]   = _colorBlack;
    _foregroundColors[toIndex(AircraftClass::Medevac)]    = _colorWhite;
    _foregroundColors[toIndex(AircraftClass::Commercial)] = _colorBlack;
    _foregroundColors[toIndex(AircraftClass::Private)]    = _colorBlack;
}

void Display::begin() {
    initColors();
    // Off-screen canvas for the scanning animation — pushSprite() sends the complete
    // frame in one DMA transfer, eliminating per-draw-call flicker.
    _scanCanvas = new M5Canvas(&M5.Display);
    _scanCanvas->setColorDepth(16);
    _scanCanvas->createSprite(128, 128);
}

// ── Splash screen ────────────────────────────────────────────────────────────

void Display::showSplash(int frame) {
    const int   centerX = 64, centerY = 38;
    const int   outerRadius = 28, innerRadius = 13;
    const float kRadiansPerFrame = 2.0f * (float)M_PI / 36;

    uint16_t brightGreen = _scanCanvas->color565(0, 255,  0);
    uint16_t midGreen    = _scanCanvas->color565(0, 130,  0);
    uint16_t dimGreen    = _scanCanvas->color565(0,  50,  0);

    _scanCanvas->fillScreen(_colorBlack);

    _scanCanvas->drawCircle(centerX, centerY, outerRadius, brightGreen);
    _scanCanvas->drawCircle(centerX, centerY, innerRadius, dimGreen);
    _scanCanvas->drawPixel(centerX, centerY, brightGreen);

    uint16_t trailColors[4] = { dimGreen, midGreen, midGreen, brightGreen };
    for (int t = 0; t < 4; t++) {
        float angle = (frame - (3 - t)) * kRadiansPerFrame;
        int   x2    = centerX + (int)(outerRadius * sinf(angle));
        int   y2    = centerY - (int)(outerRadius * cosf(angle));
        _scanCanvas->drawLine(centerX, centerY, x2, y2, trailColors[t]);
    }

    _scanCanvas->setTextColor(brightGreen, _colorBlack);
    _scanCanvas->setTextSize(2);
    _scanCanvas->setCursor(34, 74);
    _scanCanvas->print("PLANE");
    _scanCanvas->setCursor(22, 92);
    _scanCanvas->print("TRACKER");

    _scanCanvas->setTextSize(1);
    int    dots  = (frame / 5) % 4;
    String label = "Connecting";
    for (int i = 0; i < dots; i++) label += '.';
    int labelX = (128 - (int)label.length() * 6) / 2;
    _scanCanvas->setCursor(max(0, labelX), 114);
    _scanCanvas->print(label);

    _scanCanvas->pushSprite(0, 0);
}

// ── Scan / AP screens ─────────────────────────────────────────────────────────

void Display::showScanning() {
    const int   centerX = 64, centerY = 50;
    const int   outerRadius = 35, innerRadius = 18;
    const int   kFramesPerRev    = 36;
    const float kRadiansPerFrame = 2.0f * (float)M_PI / kFramesPerRev;

    uint16_t dimGreen    = _scanCanvas->color565(0,  50,  0);
    uint16_t midGreen    = _scanCanvas->color565(0, 130,  0);
    uint16_t nearGreen   = _scanCanvas->color565(0, 210,  0);
    uint16_t brightGreen = _scanCanvas->color565(0, 255,  0);

    _scanCanvas->fillScreen(_colorBlack);

    _scanCanvas->drawCircle(centerX, centerY, outerRadius, brightGreen);
    _scanCanvas->drawCircle(centerX, centerY, innerRadius, dimGreen);
    _scanCanvas->drawPixel(centerX, centerY, brightGreen);

    uint16_t trailColors[4] = { dimGreen, midGreen, nearGreen, brightGreen };
    for (int t = 0; t < 4; t++) {
        float angle = (_scanAnimFrame - (3 - t)) * kRadiansPerFrame;
        int   x2    = centerX + (int)(outerRadius * sinf(angle));
        int   y2    = centerY - (int)(outerRadius * cosf(angle));
        _scanCanvas->drawLine(centerX, centerY, x2, y2, trailColors[t]);
    }

    _scanCanvas->setTextColor(brightGreen, _colorBlack);
    _scanCanvas->setTextSize(1);
    int labelX = (128 - 7 * 6) / 2;  // 7 chars * 6px
    _scanCanvas->setCursor(labelX, 100);
    _scanCanvas->print("SCANNING");

    if (_updateAvailable) {
        uint16_t red = _scanCanvas->color565(220, 0, 0);
        _scanCanvas->fillTriangle(120, 2, 114, 11, 126, 11, red);
    }

    _scanCanvas->pushSprite(0, 0);
    _scanAnimFrame++;
}

void Display::showSetupMode() {
    M5.Display.fillScreen(_colorBlack);
    M5.Display.setTextColor(_colorWhite, _colorBlack);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(14, 4);
    M5.Display.print("SETUP");

    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 30); M5.Display.print("SSID: PlaneTracker");
    M5.Display.setCursor(2, 42); M5.Display.print("Pass: PlaneTracker");
    M5.Display.setCursor(2, 60); M5.Display.print("Open browser:");
    M5.Display.setCursor(2, 72); M5.Display.print("192.168.4.1");
}

void Display::showPinDisplay(const String& pin, const char* title) {
    uint16_t bg     = M5.Display.color565(10, 10, 80);
    uint16_t white  = M5.Display.color565(255, 255, 255);
    uint16_t yellow = M5.Display.color565(255, 220, 0);

    M5.Display.fillScreen(bg);
    M5.Display.setTextColor(white, bg);

    // Centre the title horizontally (each char ≈ 6 px at textSize 1)
    M5.Display.setTextSize(1);
    int titleX = max(0, (128 - (int)strlen(title) * 6) / 2);
    M5.Display.setCursor(titleX, 6);
    M5.Display.print(title);

    M5.Display.drawFastHLine(4, 20, 120, white);

    // Large PIN — textSize(3): 18×24 px/char, 4 chars = 72 px wide; center at x=28
    M5.Display.setTextSize(3);
    M5.Display.setTextColor(yellow, bg);
    M5.Display.setCursor(28, 36);
    M5.Display.print(pin.c_str());

    M5.Display.drawFastHLine(4, 70, 120, white);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(white, bg);
    M5.Display.setCursor(20, 78);  M5.Display.print("Enter code in");
    M5.Display.setCursor(20, 90);  M5.Display.print("browser to");
    M5.Display.setCursor(20, 102); M5.Display.print("continue");
}

// ── Aircraft screen ───────────────────────────────────────────────────────────

void Display::showLiveAircraft(const Aircraft& aircraft, int etaSeconds,
                                double queryLat, double queryLon) {
    drawAircraftScreen(aircraft, false, 0, 0, etaSeconds, queryLat, queryLon);
}

void Display::showHistoryAircraft(const Aircraft& aircraft, int historyIndex, int historyCount,
                                   double queryLat, double queryLon) {
    drawAircraftScreen(aircraft, true, historyIndex, historyCount, -1, queryLat, queryLon);
}

void Display::drawAircraftScreen(const Aircraft& aircraft, bool isHistorical,
                                  int historyIndex, int historyCount, int etaSeconds,
                                  double queryLat, double queryLon) {
    AircraftClass cls = aircraft.classification;
    M5.Display.fillScreen(_backgroundColors[toIndex(cls)]);
    M5.Display.setTextColor(_foregroundColors[toIndex(cls)], _backgroundColors[toIndex(cls)]);

    M5.Display.setTextSize(3);
    M5.Display.setCursor(2, 8);
    String callsign = aircraft.callsign.length() ? aircraft.callsign : "N/A";
    if (callsign.length() > 8) callsign = callsign.substring(0, 8);
    M5.Display.print(callsign);

    M5.Display.setTextSize(1.5);
    M5.Display.setCursor(2, 34);
    M5.Display.print("Type: ");
    M5.Display.print(aircraft.type.length() ? aircraft.type : "???");

    M5.Display.setTextSize(1.5);
    M5.Display.setCursor(2, 47);
    if (aircraft.altitude > 0) {
        M5.Display.printf("Alt:  %.0f", aircraft.altitude);
    } else {
        M5.Display.print("Alt: GND");
    }

    if (!isHistorical) {
        M5.Display.setTextSize(1.5);
        M5.Display.setCursor(2, 60);
        int eta = aircraft.adjustedEta(etaSeconds);
        if (eta < 0) {
            M5.Display.print("ETA: --:--");
        } else {
            M5.Display.printf("ETA: %d:%02d", eta / 60, eta % 60);
        }
    }

    if (aircraft.squawk.length() > 0) {
        int squawkY = isHistorical ? 60 : 73;
        M5.Display.setTextSize(1.5);
        if (aircraft.isEmergencySquawk()) {
            bool flashOn = (millis() / 400) % 2 == 0;
            uint16_t squawkBg = flashOn ? _colorRed : _backgroundColors[toIndex(cls)];
            uint16_t squawkFg = flashOn ? _colorWhite : _foregroundColors[toIndex(cls)];
            M5.Display.fillRect(0, squawkY - 1, 128, 13, squawkBg);
            M5.Display.setTextColor(squawkFg, squawkBg);
        }
        M5.Display.setCursor(2, squawkY);
        M5.Display.printf("SQK: %s", aircraft.squawk.c_str());
        M5.Display.setTextColor(_foregroundColors[toIndex(cls)], _backgroundColors[toIndex(cls)]);
    }

    if (queryLat != 0.0 && aircraft.latitude != 0.0f) {
        int distY = isHistorical ? 73 : 86;
        float dist         = aircraft.distanceNm(queryLat, queryLon);
        const char* compass = Aircraft::compassPoint(aircraft.bearingDeg(queryLat, queryLon));
        char dir            = aircraft.isApproaching(queryLat, queryLon) ? '>' : '<';
        M5.Display.setTextSize(1.5);
        M5.Display.setTextColor(_foregroundColors[toIndex(cls)], _backgroundColors[toIndex(cls)]);
        M5.Display.setCursor(2, distY);
        M5.Display.printf("%.1fNM %s %c", dist, compass, dir);
    }

    if (isHistorical) {
        uint16_t tagColor = M5.Display.color565(64, 64, 64);
        M5.Display.fillRect(0, 93, 128, 17, tagColor);
        M5.Display.setTextColor(_colorWhite, tagColor);
        M5.Display.setTextSize(2);
        const char* className = aircraftClassName(aircraft.classification);
        int textWidth = (int)strlen(className) * 12;
        int x = max(0, (128 - textWidth) / 2);
        M5.Display.setCursor(x, 93);
        M5.Display.print(className);

        M5.Display.fillRect(0, 110, 128, 20, _colorRed);
        M5.Display.setTextColor(_colorWhite, _colorRed);
        M5.Display.setTextSize(1);
        char bar[16];
        snprintf(bar, sizeof(bar), "[ %d/%d ]", historyIndex + 1, historyCount);
        int histX = max(0, (128 - (int)strlen(bar) * 6) / 2);
        M5.Display.setCursor(histX, 115);
        M5.Display.print(bar);
    } else {
        uint16_t bannerColor = M5.Display.color565(64, 64, 64);
        M5.Display.fillRect(0, 106, 128, 25, bannerColor);
        M5.Display.setTextColor(_colorWhite, bannerColor);
        M5.Display.setTextSize(2);
        const char* className = aircraftClassName(aircraft.classification);
        int textWidth = (int)strlen(className) * 12;
        int x = max(0, (128 - textWidth) / 2);
        M5.Display.setCursor(x, 111);
        M5.Display.print(className);
    }

    if (_updateAvailable && !isHistorical) {
        M5.Display.fillTriangle(120, 2, 114, 11, 126, 11, _colorRed);
    }
}

// ── Summary screen ────────────────────────────────────────────────────────────

void Display::showSummary(int militaryCount, int medevacCount, int commercialCount, int privateCount) {
    M5.Display.fillScreen(_colorBlack);
    M5.Display.setTextColor(_colorWhite, _colorBlack);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(8, 4);
    M5.Display.print("SUMMARY");

    M5.Display.setTextSize(1.5);
    int y = 38;
    M5.Display.setCursor(4, y); M5.Display.printf("MIL:   %3d", militaryCount);   y += 20;
    M5.Display.setCursor(4, y); M5.Display.printf("MED:   %3d", medevacCount);    y += 20;
    M5.Display.setCursor(4, y); M5.Display.printf("COMM:  %3d", commercialCount); y += 20;
    M5.Display.setCursor(4, y); M5.Display.printf("PRIV:  %3d", privateCount);

    M5.Display.setTextSize(1);
    M5.Display.setTextColor(M5.Display.color565(120, 120, 120), _colorBlack);
    M5.Display.setCursor(4, 116);
    M5.Display.print("2x btn: clear");
}

// ── Radar screen ─────────────────────────────────────────────────────────────

void Display::showRadar(const std::map<String, Aircraft>& aircraft,
                         double queryLat, double queryLon, float queryRadius) {
    M5.Display.fillScreen(_colorBlack);

    const int centerX = 64, centerY = 64;
    const int outerRadius = 54, innerRadius = 27;

    uint16_t dimGreen    = M5.Display.color565(0, 80, 0);
    uint16_t brightGreen = M5.Display.color565(0, 210, 0);

    M5.Display.drawCircle(centerX, centerY, outerRadius, dimGreen);
    M5.Display.drawCircle(centerX, centerY, innerRadius, dimGreen);
    M5.Display.drawLine(centerX, centerY - outerRadius, centerX, centerY + outerRadius, dimGreen);
    M5.Display.drawLine(centerX - outerRadius, centerY, centerX + outerRadius, centerY, dimGreen);
    M5.Display.fillCircle(centerX, centerY, 2, brightGreen);

    M5.Display.setTextColor(brightGreen, _colorBlack);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(centerX - 3, centerY - outerRadius - 9); M5.Display.print("N");
    M5.Display.setCursor(centerX - 3, centerY + outerRadius + 2); M5.Display.print("S");
    M5.Display.setCursor(centerX + outerRadius + 2, centerY - 4); M5.Display.print("E");
    M5.Display.setCursor(centerX - outerRadius - 8, centerY - 4); M5.Display.print("W");

    for (const auto& kv : aircraft) {
        const Aircraft& ac = kv.second;
        if (ac.latitude == 0.0f && ac.longitude == 0.0f) continue;

        float dist   = ac.distanceNm(queryLat, queryLon);
        float bearing = ac.bearingDeg(queryLat, queryLon);
        float ratio  = min(dist / queryRadius, 1.0f);
        float pixR   = ratio * (float)outerRadius;

        float bearingRad = bearing * M_PI / 180.0f;
        int blipX = centerX + (int)(pixR * sinf(bearingRad) + 0.5f);
        int blipY = centerY - (int)(pixR * cosf(bearingRad) + 0.5f);

        // Triangle pointing in direction of travel
        const float tipRadius  = 4.5f;
        float trackRadians     = ac.trackDegrees * M_PI / 180.0f;
        int tx = blipX + (int)(tipRadius * sinf(trackRadians) + 0.5f);
        int ty = blipY - (int)(tipRadius * cosf(trackRadians) + 0.5f);
        int lx = blipX + (int)(tipRadius * 0.65f * sinf(trackRadians + 2.3f) + 0.5f);
        int ly = blipY - (int)(tipRadius * 0.65f * cosf(trackRadians + 2.3f) + 0.5f);
        int rx = blipX + (int)(tipRadius * 0.65f * sinf(trackRadians - 2.3f) + 0.5f);
        int ry = blipY - (int)(tipRadius * 0.65f * cosf(trackRadians - 2.3f) + 0.5f);

        uint16_t color = _backgroundColors[toIndex(ac.classification)];
        M5.Display.fillTriangle(tx, ty, lx, ly, rx, ry, color);
        M5.Display.fillCircle(tx, ty, 1, _colorWhite);
    }

    if (!aircraft.empty()) {
        char buf[8];
        snprintf(buf, sizeof(buf), "%d AC", (int)aircraft.size());
        M5.Display.setTextColor(brightGreen, _colorBlack);
        M5.Display.setTextSize(1);
        M5.Display.setCursor(2, 2);
        M5.Display.print(buf);
    }
}

// ── Debug screen ──────────────────────────────────────────────────────────────

void Display::showDebug(const std::vector<String>& lines, int scrollOffset,
                         const DiagnosticInfo& info) {
    M5.Display.fillScreen(_colorBlack);
    M5.Display.setTextColor(_colorWhite, _colorBlack);
    M5.Display.setTextSize(1);

    M5.Display.setCursor(0, 0);
    M5.Display.printf("DBG HTTP:%d ac:%d", info.responseCode, info.aircraftCount);

    M5.Display.setCursor(0, 8);
    M5.Display.printf("IP: %s", info.ipAddress.c_str());

    M5.Display.setCursor(0, 16);
    uint32_t hours   = info.uptimeSeconds / 3600;
    uint32_t minutes = (info.uptimeSeconds % 3600) / 60;
    uint32_t seconds = info.uptimeSeconds % 60;
    M5.Display.printf("UP: %02u:%02u:%02u", hours, minutes, seconds);

    M5.Display.setCursor(0, 24);
    M5.Display.printf("VER: %s", info.version.c_str());

    int lineTotal = (int)lines.size();
    int maxScroll = max(0, lineTotal - kVisibleLineCount);
    for (int i = 0; i < kVisibleLineCount; i++) {
        int lineIndex = scrollOffset + i;
        M5.Display.setCursor(0, 32 + i * 8);
        if (lineIndex < lineTotal) {
            String line = lines[lineIndex];
            if (line.length() > 21) line = line.substring(0, 21);
            M5.Display.print(line);
        }
    }

    if (maxScroll > 0) {
        M5.Display.setCursor(96, 104);
        M5.Display.printf("%d/%d", scrollOffset, maxScroll);
    }

    auto drawBar = [&](int y, const char* label, float usedRatio, const char* pctStr) {
        uint16_t dimGray = M5.Display.color565(40, 40, 40);
        uint16_t barColor = usedRatio > 0.75f ? _colorRed
                          : usedRatio > 0.50f ? M5.Display.color565(200, 180, 0)
                          :                     M5.Display.color565(0, 180, 0);
        M5.Display.setTextColor(_colorWhite, _colorBlack);
        M5.Display.setCursor(0, y);
        M5.Display.print(label);
        M5.Display.fillRect(20, y + 2, 72, 4, dimGray);
        M5.Display.fillRect(20, y + 2, (int)(usedRatio * 72), 4, barColor);
        M5.Display.setCursor(94, y);
        M5.Display.print(pctStr);
    };

    uint32_t freeHeap  = ESP.getFreeHeap();
    uint32_t totalHeap = ESP.getHeapSize();
    float    ramUsed   = 1.0f - (float)freeHeap / (float)totalHeap;
    char     ramPct[5]; snprintf(ramPct, sizeof(ramPct), "%3d%%", (int)(ramUsed * 100));
    drawBar(112, "RAM", ramUsed, ramPct);

    // Clamp to [0,1] — watermark can exceed kAssumedLoopTaskStackBytes on overflow.
    static constexpr uint32_t kAssumedLoopTaskStackBytes = 24 * 1024;
    uint32_t stackFreeBytes = uxTaskGetStackHighWaterMark(NULL) * 4;
    float    stackUsed      = max(0.0f, 1.0f - (float)stackFreeBytes / (float)kAssumedLoopTaskStackBytes);
    char     stackPct[5]; snprintf(stackPct, sizeof(stackPct), "%3d%%", (int)(stackUsed * 100));
    drawBar(120, "STK", stackUsed, stackPct);
}

// ── Generic message ───────────────────────────────────────────────────────────

void Display::showMessage(const String& line1, const String& line2) {
    M5.Display.fillScreen(_colorBlack);
    M5.Display.setTextColor(_colorWhite, _colorBlack);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 56);
    M5.Display.print(line1);
    if (line2.length() > 0) {
        M5.Display.setCursor(2, 70);
        M5.Display.print(line2);
    }
}

void Display::showLostConnection(int retryCount) {
    M5.Display.fillScreen(_colorRed);
    M5.Display.setTextColor(_colorBlack, _colorRed);

    M5.Display.setTextSize(3);
    int x = (128 - 4 * 18) / 2;
    M5.Display.setCursor(x, 12);
    M5.Display.print("LOST");

    M5.Display.setTextSize(2);
    x = (128 - 10 * 12) / 2;
    M5.Display.setCursor(x, 42);
    M5.Display.print("CONNECTION");

    M5.Display.setTextSize(1);
    x = (128 - 8 * 6) / 2;
    M5.Display.setCursor(x, 78);
    M5.Display.print("RETRYING");

    char buf[12];
    snprintf(buf, sizeof(buf), "(%d)", retryCount);
    x = (128 - (int)strlen(buf) * 6) / 2;
    M5.Display.setCursor(x, 90);
    M5.Display.print(buf);
}

String Display::rgb565ToCss(uint16_t c) {
    uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((c >> 5)  & 0x3F) * 255 / 63;
    uint8_t b = (c & 0x1F) * 255 / 31;
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return String(buf);
}

void Display::showPollIntervalError(uint32_t currentMs, uint32_t minimumMs, int secondsRemaining) {
    M5.Display.fillScreen(_colorRed);
    M5.Display.setTextColor(_colorBlack, _colorRed);
    M5.Display.setTextSize(1);

    M5.Display.setCursor(2, 4);
    M5.Display.print("POLL TOO FAST");

    M5.Display.setCursor(2, 18);
    M5.Display.printf("Set: %ums", currentMs);

    M5.Display.setCursor(2, 30);
    M5.Display.printf("Min: %ums", minimumMs);

    M5.Display.setCursor(2, 50);
    M5.Display.print("Press btn to set");
    M5.Display.setCursor(2, 62);
    M5.Display.printf("min + reboot");

    M5.Display.setCursor(2, 82);
    M5.Display.printf("Auto-fix in %ds", secondsRemaining);
}
