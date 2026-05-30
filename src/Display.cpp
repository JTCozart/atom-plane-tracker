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
    // Off-screen canvas for the scanning animation — pushSprite() sends the
    // complete frame in one DMA transfer, eliminating per-draw-call flicker.
    _scanCanvas = new M5Canvas(&M5.Display);
    _scanCanvas->setColorDepth(16);
    _scanCanvas->createSprite(128, 128);
}

// ── Scan / AP screens ─────────────────────────────────────────────────────────

void Display::showScanning() {
    // ── Radar sweep — drawn into off-screen canvas, pushed in one transfer ────
    const int   cx = 64, cy = 50;
    const int   outerR = 35, innerR = 18;
    const int   kFramesPerRev = 36;
    const float kRadPerFrame  = 2.0f * (float)M_PI / kFramesPerRev;

    uint16_t dimGray   = _scanCanvas->color565(50,  50,  50);
    uint16_t midGray   = _scanCanvas->color565(110, 110, 110);
    uint16_t nearWhite = _scanCanvas->color565(190, 190, 190);

    _scanCanvas->fillScreen(_colorBlack);

    // Range rings
    _scanCanvas->drawCircle(cx, cy, outerR, _colorWhite);
    _scanCanvas->drawCircle(cx, cy, innerR, dimGray);
    _scanCanvas->drawPixel(cx, cy, _colorWhite);

    // Sweep line + 3-step trailing glow (dim → mid → near-white → white)
    uint16_t trailColors[4] = { dimGray, midGray, nearWhite, _colorWhite };
    for (int t = 0; t < 4; t++) {
        float angle = (_scanAnimFrame - (3 - t)) * kRadPerFrame;
        int   x2    = cx + (int)(outerR * sinf(angle));
        int   y2    = cy - (int)(outerR * cosf(angle));
        _scanCanvas->drawLine(cx, cy, x2, y2, trailColors[t]);
    }

    // "SCANNING..." label with animated dot count
    _scanCanvas->setTextColor(_colorWhite, _colorBlack);
    _scanCanvas->setTextSize(1);
    int    dotCount = (_scanAnimFrame / (kFramesPerRev / 4)) % 4;
    String label    = "SCANNING";
    for (int i = 0; i < dotCount; i++) label += '.';
    int labelX = (128 - (int)label.length() * 6) / 2;
    _scanCanvas->setCursor(max(0, labelX), 100);
    _scanCanvas->print(label);

    // Single atomic transfer to the physical display — no partial-frame flicker
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

    // Row 0 — class label
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 2);
    M5.Display.print(aircraftClassName(cls));

    // Row 1-2 — callsign, large
    M5.Display.setTextSize(2);
    M5.Display.setCursor(2, 14);
    String cs = aircraft.callsign.length() ? aircraft.callsign : "N/A";
    if (cs.length() > 10) cs = cs.substring(0, 10);
    M5.Display.print(cs);

    // Row 3 — type
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 34);
    M5.Display.print("Type: ");
    M5.Display.print(aircraft.type.length() ? aircraft.type : "???");

    // Row 4 — altitude
    M5.Display.setCursor(2, 46);
    if (aircraft.altitude > 0) {
        M5.Display.printf("Alt:  %.0f ft", aircraft.altitude);
    } else {
        M5.Display.print("Alt:  ground");
    }

    // Row 5 — ETA until leaving radius (live only, not history)
    if (!isHistorical) {
        M5.Display.setCursor(2, 58);
        if (etaSeconds < 0) {
            M5.Display.print("ETA:  --:--");
        } else {
            int elapsed = (int)((millis() - aircraft.positionTimestamp) / 1000);
            int eta = max(0, etaSeconds - elapsed);
            M5.Display.printf("ETA:  %d:%02d", eta / 60, eta % 60);
        }
    }

    // History bar — red strip across the bottom with entry counter
    if (isHistorical) {
        M5.Display.fillRect(0, 116, 128, 12, _colorRed);
        M5.Display.setTextColor(_colorWhite, _colorRed);
        char bar[24];
        snprintf(bar, sizeof(bar), "[ HIST %d/%d ]", historyIndex + 1, historyCount);
        int x = max(0, (128 - (int)strlen(bar) * 6) / 2);
        M5.Display.setCursor(x, 118);
        M5.Display.print(bar);
    }
}

// ── Summary screen ────────────────────────────────────────────────────────────

void Display::showSummary(int militaryCount, int medevacCount, int commercialCount, int privateCount) {
    M5.Display.fillScreen(_colorBlack);
    M5.Display.setTextColor(_colorWhite, _colorBlack);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(8, 4);
    M5.Display.print("SUMMARY");

    M5.Display.setTextSize(1);
    int y = 38;
    M5.Display.setCursor(4, y); M5.Display.printf("Military:   %d", militaryCount);   y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Medevac:    %d", medevacCount);    y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Commercial: %d", commercialCount); y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Private:    %d", privateCount);
}

// ── Debug screen ──────────────────────────────────────────────────────────────

void Display::showDebug(const std::vector<String>& lines, int scrollOffset,
                         int responseCode, int aircraftCount, const String& ipAddress) {
    M5.Display.fillScreen(_colorBlack);
    M5.Display.setTextColor(_colorWhite, _colorBlack);
    M5.Display.setTextSize(1);

    // Header row 1 — HTTP status and aircraft count
    M5.Display.setCursor(0, 0);
    M5.Display.printf("DBG HTTP:%d ac:%d", responseCode, aircraftCount);

    // Header row 2 — device IP (tap-able via browser for config)
    M5.Display.setCursor(0, 10);
    M5.Display.printf("IP: %s", ipAddress.c_str());

    // Content lines
    int lineTotal = (int)lines.size();
    int maxScroll = max(0, lineTotal - kVisibleLineCount);
    for (int i = 0; i < kVisibleLineCount; i++) {
        int li = scrollOffset + i;
        M5.Display.setCursor(0, kContentStartY + i * 8);
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
