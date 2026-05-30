#include "Display.h"
#include <M5Unified.h>

// ── Color initialisation ─────────────────────────────────────────────────────

void Display::initColors() {
    _black = M5.Display.color565(0,   0,   0);
    _white = M5.Display.color565(255, 255, 255);
    _red   = M5.Display.color565(255, 0,   0);

    _bg[CLS_MIL]    = M5.Display.color565(255, 0,   0);    // red
    _bg[CLS_MEDVAC] = M5.Display.color565(0,   0,   255);  // blue
    _bg[CLS_COMM]   = M5.Display.color565(0,   255, 0);    // green
    _bg[CLS_PRIV]   = M5.Display.color565(255, 255, 0);    // yellow

    _fg[CLS_MIL]    = _black;
    _fg[CLS_MEDVAC] = _white;
    _fg[CLS_COMM]   = _black;
    _fg[CLS_PRIV]   = _black;
}

void Display::begin() {
    initColors();
}

// ── Scan / AP screens ─────────────────────────────────────────────────────────

void Display::drawScan() {
    M5.Display.fillScreen(_black);
    M5.Display.setTextColor(_white, _black);
    M5.Display.setTextSize(2);
    // Center "SCANNING" — each char 12px wide, 8 chars = 96px; (128-96)/2 = 16
    M5.Display.setCursor(16, 56);
    M5.Display.print("SCANNING");
}

void Display::drawAPMode() {
    M5.Display.fillScreen(_black);
    M5.Display.setTextColor(_white, _black);
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

void Display::drawAircraft(const Ac& ac, bool hist,
                            int histIdx, int histTotal, int etaSecs) {
    AcClass c = ac.cls;
    M5.Display.fillScreen(_bg[c]);
    M5.Display.setTextColor(_fg[c], _bg[c]);

    // Row 0 — class label
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 2);
    M5.Display.print(AC_LABELS[c]);

    // Row 1-2 — callsign, large
    M5.Display.setTextSize(2);
    M5.Display.setCursor(2, 14);
    String cs = ac.callsign.length() ? ac.callsign : "N/A";
    if (cs.length() > 10) cs = cs.substring(0, 10);
    M5.Display.print(cs);

    // Row 3 — type
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 34);
    M5.Display.print("Type: ");
    M5.Display.print(ac.type.length() ? ac.type : "???");

    // Row 4 — altitude
    M5.Display.setCursor(2, 46);
    if (ac.alt > 0) {
        M5.Display.printf("Alt:  %.0f ft", ac.alt);
    } else {
        M5.Display.print("Alt:  ground");
    }

    // Row 5 — ETA until leaving radius (live only, not history)
    if (!hist) {
        M5.Display.setCursor(2, 58);
        if (etaSecs < 0) {
            M5.Display.print("ETA:  --:--");
        } else {
            int elapsed = (int)((millis() - ac.lastFixMs) / 1000);
            int eta = max(0, etaSecs - elapsed);
            M5.Display.printf("ETA:  %d:%02d", eta / 60, eta % 60);
        }
    }

    // Row 6-7 — owner (up to two lines of 21 chars each)
    int ownerY = hist ? 58 : 70;
    M5.Display.setCursor(2, ownerY);
    M5.Display.print("Owner:");
    String owner = ac.owner.length() ? ac.owner : "Unknown";
    M5.Display.setCursor(2, ownerY + 12);
    if (owner.length() <= 21) {
        M5.Display.print(owner);
    } else {
        M5.Display.print(owner.substring(0, 21));
        M5.Display.setCursor(2, ownerY + 24);
        M5.Display.print(owner.substring(21, 42));
    }

    // History bar — red strip across the bottom with entry counter
    if (hist) {
        M5.Display.fillRect(0, 116, 128, 12, _red);
        M5.Display.setTextColor(_white, _red);
        char bar[24];
        snprintf(bar, sizeof(bar), "[ HIST %d/%d ]", histIdx + 1, histTotal);
        int x = max(0, (128 - (int)strlen(bar) * 6) / 2);
        M5.Display.setCursor(x, 118);
        M5.Display.print(bar);
    }
}

// ── Summary screen ────────────────────────────────────────────────────────────

void Display::drawSummary(int mil, int medvac, int comm, int priv) {
    M5.Display.fillScreen(_black);
    M5.Display.setTextColor(_white, _black);
    M5.Display.setTextSize(2);
    M5.Display.setCursor(8, 4);
    M5.Display.print("SUMMARY");

    M5.Display.setTextSize(1);
    int y = 38;
    M5.Display.setCursor(4, y); M5.Display.printf("Military:   %d", mil);    y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Medevac:    %d", medvac); y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Commercial: %d", comm);   y += 14;
    M5.Display.setCursor(4, y); M5.Display.printf("Private:    %d", priv);
}

// ── Debug screen ──────────────────────────────────────────────────────────────

void Display::drawDebug(const std::vector<String>& lines, int scroll,
                         int httpCode, int total, const String& ip) {
    M5.Display.fillScreen(_black);
    M5.Display.setTextColor(_white, _black);
    M5.Display.setTextSize(1);

    // Header row 1 — HTTP status and aircraft count
    M5.Display.setCursor(0, 0);
    M5.Display.printf("DBG HTTP:%d ac:%d", httpCode, total);

    // Header row 2 — device IP (tap-able via browser for config)
    M5.Display.setCursor(0, 10);
    M5.Display.printf("IP: %s", ip.c_str());

    // Content lines
    int lineTotal = (int)lines.size();
    int maxScroll = max(0, lineTotal - LINES_VISIBLE);
    for (int i = 0; i < LINES_VISIBLE; i++) {
        int li = scroll + i;
        M5.Display.setCursor(0, CONTENT_Y + i * 8);
        if (li < lineTotal) {
            String line = lines[li];
            if (line.length() > 21) line = line.substring(0, 21);
            M5.Display.print(line);
        }
    }

    // Scroll indicator (bottom-right)
    if (maxScroll > 0) {
        M5.Display.setCursor(96, 120);
        M5.Display.printf("%d/%d", scroll, maxScroll);
    }
}

// ── Generic message ───────────────────────────────────────────────────────────

void Display::showMessage(const String& line1, const String& line2) {
    M5.Display.fillScreen(_black);
    M5.Display.setTextColor(_white, _black);
    M5.Display.setTextSize(1);
    M5.Display.setCursor(2, 56);
    M5.Display.print(line1);
    if (line2.length() > 0) {
        M5.Display.setCursor(2, 70);
        M5.Display.print(line2);
    }
}
