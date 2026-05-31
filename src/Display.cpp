#include "Display.h"
#include <M5Unified.h>
#include <cmath>

// ── Color initialisation ─────────────────────────────────────────────────────

void Display::initColors() {
    _colorBlack = M5.Display.color565(0,   0,   0);
    _colorWhite = M5.Display.color565(255, 255, 255);
    _colorRed   = M5.Display.color565(255, 0,   0);

    _backgroundColors[toIndex(AircraftClass::Military)]   = M5.Display.color565(255, 0,   0);    // red
    _backgroundColors[toIndex(AircraftClass::Medevac)]    = M5.Display.color565(0,   0,   255);  // blue
    _backgroundColors[toIndex(AircraftClass::Commercial)] = M5.Display.color565(0,   255, 0);    // green
    _backgroundColors[toIndex(AircraftClass::Private)]    = M5.Display.color565(255, 255, 0);    // yellow

    _foregroundColors[toIndex(AircraftClass::Military)]   = _colorBlack;
    _foregroundColors[toIndex(AircraftClass::Medevac)]    = _colorWhite;
    _foregroundColors[toIndex(AircraftClass::Commercial)] = _colorBlack;
    _foregroundColors[toIndex(AircraftClass::Private)]    = _colorBlack;
}

void Display::begin() {
    initColors();
    // Off-screen canvas for the scanning animation - pushSprite() sends the
    // complete frame in one DMA transfer, eliminating per-draw-call flicker.
    _scanCanvas = new M5Canvas(&M5.Display);
    _scanCanvas->setColorDepth(16);
    _scanCanvas->createSprite(128, 128);
}

// ── Splash screen ────────────────────────────────────────────────────────────

void Display::showSplash(int frame) {
    const int   cx = 64, cy = 38;
    const int   outerR = 28, innerR = 13;
    const float kRadPerFrame = 2.0f * (float)M_PI / 36;

    uint16_t brightGreen = _scanCanvas->color565(0, 255,  0);
    uint16_t midGreen    = _scanCanvas->color565(0, 130,  0);
    uint16_t dimGreen    = _scanCanvas->color565(0,  50,  0);

    _scanCanvas->fillScreen(_colorBlack);

    // Radar rings
    _scanCanvas->drawCircle(cx, cy, outerR, brightGreen);
    _scanCanvas->drawCircle(cx, cy, innerR, dimGreen);
    _scanCanvas->drawPixel(cx, cy, brightGreen);

    // Animated sweep with 3-step trailing glow
    uint16_t trailColors[4] = { dimGreen, midGreen, midGreen, brightGreen };
    for (int t = 0; t < 4; t++) {
        float angle = (frame - (3 - t)) * kRadPerFrame;
        int   x2    = cx + (int)(outerR * sinf(angle));
        int   y2    = cy - (int)(outerR * cosf(angle));
        _scanCanvas->drawLine(cx, cy, x2, y2, trailColors[t]);
    }

    // "PLANE" and "TRACKER" centered, large
    _scanCanvas->setTextColor(brightGreen, _colorBlack);
    _scanCanvas->setTextSize(2);
    _scanCanvas->setCursor(34, 74);
    _scanCanvas->print("PLANE");
    _scanCanvas->setCursor(22, 92);
    _scanCanvas->print("TRACKER");

    // Animated "Connecting..." dots
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
    // ── Radar sweep - drawn into off-screen canvas, pushed in one transfer ────
    const int   cx = 64, cy = 50;
    const int   outerR = 35, innerR = 18;
    const int   kFramesPerRev = 36;
    const float kRadPerFrame  = 2.0f * (float)M_PI / kFramesPerRev;

    uint16_t dimGreen  = _scanCanvas->color565(0,  50,  0);
    uint16_t midGreen  = _scanCanvas->color565(0, 130,  0);
    uint16_t nearGreen = _scanCanvas->color565(0, 210,  0);
    uint16_t brightGreen = _scanCanvas->color565(0, 255, 0);

    _scanCanvas->fillScreen(_colorBlack);

    // Range rings
    _scanCanvas->drawCircle(cx, cy, outerR, brightGreen);
    _scanCanvas->drawCircle(cx, cy, innerR, dimGreen);
    _scanCanvas->drawPixel(cx, cy, brightGreen);

    // Sweep line + 3-step trailing glow (dim → mid → near → bright green)
    uint16_t trailColors[4] = { dimGreen, midGreen, nearGreen, brightGreen };
    for (int t = 0; t < 4; t++) {
        float angle = (_scanAnimFrame - (3 - t)) * kRadPerFrame;
        int   x2    = cx + (int)(outerR * sinf(angle));
        int   y2    = cy - (int)(outerR * cosf(angle));
        _scanCanvas->drawLine(cx, cy, x2, y2, trailColors[t]);
    }

    // "SCANNING" label
    _scanCanvas->setTextColor(brightGreen, _colorBlack);
    _scanCanvas->setTextSize(1);
    int labelX = (128 - 7 * 6) / 2;  // 7 chars * 6px
    _scanCanvas->setCursor(labelX, 100);
    _scanCanvas->print("SCANNING");

    // Update-available indicator: small red up-arrow in top-right corner
    if (_updateAvailable) {
        uint16_t red = _scanCanvas->color565(220, 0, 0);
        _scanCanvas->fillTriangle(120, 2, 114, 11, 126, 11, red);
    }

    // Single atomic transfer to the physical display - no partial-frame flicker
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

// ── Aircraft screen ───────────────────────────────────────────────────────────

void Display::showAircraft(const Aircraft& aircraft, bool isHistorical,
                            int historyIndex, int historyCount, int etaSeconds) {
    AircraftClass cls = aircraft.classification;
    M5.Display.fillScreen(_backgroundColors[toIndex(cls)]);
    M5.Display.setTextColor(_foregroundColors[toIndex(cls)], _backgroundColors[toIndex(cls)]);

    // Row 1-2 - callsign, extra large
    M5.Display.setTextSize(3);
    M5.Display.setCursor(2, 8);
    String cs = aircraft.callsign.length() ? aircraft.callsign : "N/A";
    if (cs.length() > 8) cs = cs.substring(0, 8);
    M5.Display.print(cs);

    // Row 3 - type
    M5.Display.setTextSize(1.5);
    M5.Display.setCursor(2, 34);
    M5.Display.print("Type: ");
    M5.Display.print(aircraft.type.length() ? aircraft.type : "???");

    // Row 4 - altitude
    M5.Display.setTextSize(1.5);
    M5.Display.setCursor(2, 47);
    if (aircraft.altitude > 0) {
        M5.Display.printf("Alt:  %.0f", aircraft.altitude);
    } else {
        M5.Display.print("Alt: GND");
    }

    // Row 5 - ETA until leaving radius (live only, not history)
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

    // Bottom banner
    if (isHistorical) {
        // Classification tag - aircraft type above history counter
        uint16_t tagColor = M5.Display.color565(64, 64, 64);
        M5.Display.fillRect(0, 93, 128, 17, tagColor);
        M5.Display.setTextColor(_colorWhite, tagColor);
        M5.Display.setTextSize(2);
        const char* typeStr = aircraftClassName(aircraft.classification);
        int textWidth = (int)strlen(typeStr) * 12;
        int x = max(0, (128 - textWidth) / 2);
        M5.Display.setCursor(x, 93);
        M5.Display.print(typeStr);

        // History counter - red strip with entry counter, extends to edge
        M5.Display.fillRect(0, 110, 128, 20, _colorRed);
        M5.Display.setTextColor(_colorWhite, _colorRed);
        M5.Display.setTextSize(1);
        char bar[16];
        snprintf(bar, sizeof(bar), "[ %d/%d ]", historyIndex + 1, historyCount);
        int histX = max(0, (128 - (int)strlen(bar) * 6) / 2);
        M5.Display.setCursor(histX, 115);
        M5.Display.print(bar);
    } else {
        // Classification banner - dark gray strip showing aircraft type, extends to edge
        uint16_t bannerColor = M5.Display.color565(64, 64, 64);
        M5.Display.fillRect(0, 106, 128, 25, bannerColor);
        M5.Display.setTextColor(_colorWhite, bannerColor);
        M5.Display.setTextSize(2);
        const char* typeStr = aircraftClassName(aircraft.classification);
        int textWidth = (int)strlen(typeStr) * 12;
        int x = max(0, (128 - textWidth) / 2);
        M5.Display.setCursor(x, 111);
        M5.Display.print(typeStr);
    }

    // Update-available indicator: small red up-arrow in top-right corner (live view only)
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
}

// ── Debug screen ──────────────────────────────────────────────────────────────

void Display::showDebug(const std::vector<String>& lines, int scrollOffset,
                         int responseCode, int aircraftCount, const String& ipAddress,
                         uint32_t uptimeSeconds, const String& version) {
    M5.Display.fillScreen(_colorBlack);
    M5.Display.setTextColor(_colorWhite, _colorBlack);
    M5.Display.setTextSize(1);

    // Header rows - 8px spacing so 4 rows fit before content at y=32
    M5.Display.setCursor(0, 0);
    M5.Display.printf("DBG HTTP:%d ac:%d", responseCode, aircraftCount);

    M5.Display.setCursor(0, 8);
    M5.Display.printf("IP: %s", ipAddress.c_str());

    M5.Display.setCursor(0, 16);
    uint32_t h = uptimeSeconds / 3600;
    uint32_t m = (uptimeSeconds % 3600) / 60;
    uint32_t s = uptimeSeconds % 60;
    M5.Display.printf("UP: %02u:%02u:%02u", h, m, s);

    M5.Display.setCursor(0, 24);
    M5.Display.printf("VER: %s", version.c_str());

    // Content lines (y=32 .. y=128, 12 lines * 8px)
    int lineTotal = (int)lines.size();
    int maxScroll = max(0, lineTotal - kVisibleLineCount);
    for (int i = 0; i < kVisibleLineCount; i++) {
        int li = scrollOffset + i;
        M5.Display.setCursor(0, 32 + i * 8);
        if (li < lineTotal) {
            String line = lines[li];
            if (line.length() > 21) line = line.substring(0, 21);
            M5.Display.print(line);
        }
    }

    // Scroll indicator (bottom-right)
    if (maxScroll > 0) {
        M5.Display.setCursor(96, 120);
        M5.Display.printf("%d/%d", scrollOffset, maxScroll);
    }
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

    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 50);
    M5.Display.print("Press btn to set");
    M5.Display.setCursor(2, 62);
    M5.Display.printf("min + reboot");

    M5.Display.setCursor(2, 82);
    M5.Display.printf("Auto-fix in %ds", secondsRemaining);
}
