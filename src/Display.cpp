#include "Display.h"
#include <M5Unified.h>

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
}

// ── Scan / AP screens ─────────────────────────────────────────────────────────

void Display::showScanning() {
    M5.Display.fillScreen(_colorBlack);
    M5.Display.setTextColor(_colorWhite, _colorBlack);
    M5.Display.setTextSize(2);
    // Center "SCANNING" — each char 12px wide, 8 chars = 96px; (128-96)/2 = 16
    M5.Display.setCursor(16, 56);
    M5.Display.print("SCANNING");
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

    // Row 6-7 — owner (up to two lines of 21 chars each)
    int ownerY = isHistorical ? 58 : 70;
    M5.Display.setCursor(2, ownerY);
    M5.Display.print("Owner:");
    String owner = aircraft.owner.length() ? aircraft.owner : "Unknown";
    M5.Display.setCursor(2, ownerY + 12);
    if (owner.length() <= 21) {
        M5.Display.print(owner);
    } else {
        M5.Display.print(owner.substring(0, 21));
        M5.Display.setCursor(2, ownerY + 24);
        M5.Display.print(owner.substring(21, 42));
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
