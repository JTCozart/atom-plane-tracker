#include "WebUI.h"
#include <WiFi.h>
#include <Preferences.h>

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void WebUI::begin(ScreenMode& mode, bool isSetupMode) {
    _screenMode  = &mode;
    _inSetupMode = isSetupMode;

    if (isSetupMode) {
        _ipAddress = "192.168.4.1";
        WiFi.mode(WIFI_AP);
        WiFi.softAP("PlaneTracker", "PlaneTracker");
    }

    _server.on("/",        HTTP_GET,  [this]() { handleRoot();    });
    _server.on("/save",    HTTP_POST, [this]() { handleSave();    });
    _server.on("/clear",   HTTP_POST, [this]() { handleClear();   });
    _server.on("/control", HTTP_GET,  [this]() { handleControl(); });
    _server.on("/screen",  HTTP_GET,  [this]() { handleScreen();  });
    _server.begin();
}

void WebUI::processRequests() {
    _server.handleClient();
}

// ── Handlers ──────────────────────────────────────────────────────────────────

void WebUI::handleRoot() {
    String html =
        F("<!DOCTYPE html><html><head><title>PlaneTracker Setup</title>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'>"
          "<style>"
          "body{font-family:sans-serif;max-width:820px;margin:24px auto;padding:0 12px}"
          "h2{margin-bottom:16px}h3{margin:0 0 10px}"
          "label{display:block;margin-top:10px;font-size:.9em}"
          "input{width:100%;padding:7px;box-sizing:border-box;margin-top:3px;font-size:1em}"
          ".btn{display:block;margin-top:18px;width:100%;padding:10px;background:#1976D2;"
          "color:#fff;border:none;font-size:1em;cursor:pointer;border-radius:4px}"
          ".layout{display:flex;flex-wrap:wrap;gap:32px;align-items:flex-start}"
          ".form-col{flex:1;min-width:280px}"
          ".preview-col{flex:0 0 auto;text-align:center}"
          "#scr{width:256px;height:256px;display:inline-block;position:relative;"
          "font-family:monospace;font-size:12px;padding:6px;box-sizing:border-box;"
          "border:4px solid #222;border-radius:6px;overflow:hidden;text-align:left}"
          ".ctrl{display:flex;gap:6px;margin-top:8px}"
          ".cb{flex:1;padding:8px 0;background:#37474F;color:#fff;border:none;"
          "cursor:pointer;border-radius:4px;font-size:.82em}"
          ".cb:hover{background:#546E7A}.cb.on{background:#1976D2}"
          "</style>"
          "<script>"
          "function refresh(){"
            "fetch('/screen?fragment=1')"
            ".then(function(r){return r.text();})"
            ".then(function(h){var e=document.getElementById('scrWrap');if(e)e.innerHTML=h;})"
            ".catch(function(){});"
          "}"
          "function ctrl(s){"
            "fetch('/control?screen='+s).then(function(){setTimeout(refresh,300);});"
            "document.querySelectorAll('.cb').forEach(function(b){"
              "b.classList.toggle('on',b.dataset.s===s);});"
          "}"
          "refresh();setInterval(refresh,5000);"
          "</script>"
          "</head><body><h2>&#9992; PlaneTracker Setup</h2>"
          "<div class='layout'>"
          "<div class='form-col'>"
          "<form method='POST' action='/save'>");

    html += "<label>WiFi SSID</label>"
            "<input name='ssid' value='" + String(_cfg.ssid) + "'>";
    html += "<label>WiFi Password</label>"
            "<input name='pass' type='password' placeholder='leave blank to keep current'>";
    html += "<label>Latitude</label>"
            "<input name='lat' value='" + String(_cfg.latitude, 6) + "'>";
    html += "<label>Longitude</label>"
            "<input name='lon' value='" + String(_cfg.longitude, 6) + "'>";
    html += "<label>Search Radius (nautical miles)</label>"
            "<input name='radius' value='" + String(_cfg.radius) + "'>";
    html += "<label>Poll Interval (ms) <small style='font-weight:normal'> - minimum " +
            String(Config::kMinPollIntervalMs) + "ms</small></label>"
            "<input name='poll' type='number' min='" + String(Config::kMinPollIntervalMs) +
            "' value='" + String(_cfg.pollIntervalMs) + "'>";
    html += "<hr style='margin:18px 0'>";
    html += "<label>ntfy Token</label>"
            "<input name='ntfyToken' type='password' placeholder='leave blank to keep current'>";
    html += "<label>ntfy Topic</label>"
            "<input name='ntfyTopic' value='" + String(_cfg.notifyTopic) + "'>";
    html += "<label>ntfy Classes <small style='font-weight:normal'>(comma-separated: MIL, MEDVAC, COMM, PRIV - empty&nbsp;=&nbsp;all)</small></label>"
            "<input name='ntfyClasses' value='" + String(_cfg.notifyClassFilter) + "' placeholder='empty = all classes'>";
    html += "<button class='btn' type='submit'>Save &amp; Reboot</button>"
            "</form>"
            "<form method='POST' action='/clear' style='margin-top:12px'>"
            "<button class='btn' type='submit' style='background:#D32F2F' "
            "onclick='return confirm(\"Reset all settings to factory defaults? This cannot be undone.\")'>"
            "Clear All Settings</button>"
            "</form>"
            "</div>"; // end form-col

    // Right column — live screen preview + controls
    html += "<div class='preview-col'>"
            "<h3>Live Screen</h3>"
            "<div id='scrWrap'></div>"
            "<div class='ctrl'>"
            "<button class='cb' data-s='scan'    onclick='ctrl(\"scan\")'>Scan</button>"
            "<button class='cb' data-s='history' onclick='ctrl(\"history\")'>History</button>"
            "<button class='cb' data-s='summary' onclick='ctrl(\"summary\")'>Summary</button>"
            "<button class='cb' data-s='debug'   onclick='ctrl(\"debug\")'>Debug</button>"
            "</div>"
            "<p style='color:#666;font-size:.8em;margin-top:6px'>Refreshes every 5s</p>"
            "</div>"; // end preview-col

    html += "</div></body></html>"; // end layout + body

    _server.send(200, "text/html", html);
}

void WebUI::handleSave() {
    Preferences prefs;
    prefs.begin("plantracker", false);

    if (_server.hasArg("ssid") && _server.arg("ssid").length() > 0)
        prefs.putString("ssid", _server.arg("ssid"));
    if (_server.hasArg("pass") && _server.arg("pass").length() > 0)
        prefs.putString("pass", _server.arg("pass"));
    if (_server.hasArg("lat"))
        prefs.putDouble("lat",    _server.arg("lat").toDouble());
    if (_server.hasArg("lon"))
        prefs.putDouble("lon",    _server.arg("lon").toDouble());
    if (_server.hasArg("radius"))
        prefs.putFloat ("radius", _server.arg("radius").toFloat());
    if (_server.hasArg("poll")) {
        uint32_t pollValue = (uint32_t)_server.arg("poll").toInt();
        if (pollValue < Config::kMinPollIntervalMs) pollValue = Config::kMinPollIntervalMs;
        prefs.putUInt("pollMs", pollValue);
    }
    if (_server.hasArg("ntfyToken") && _server.arg("ntfyToken").length() > 0)
        prefs.putString("ntfyToken",   _server.arg("ntfyToken"));
    if (_server.hasArg("ntfyTopic"))
        prefs.putString("ntfyTopic",   _server.arg("ntfyTopic"));
    if (_server.hasArg("ntfyClasses"))
        prefs.putString("ntfyClasses", _server.arg("ntfyClasses"));

    prefs.end();

    _server.send(200, "text/html",
        F("<!DOCTYPE html><html><head><title>PlaneTracker - Saved</title>"
          "<style>body{font-family:sans-serif;text-align:center;margin-top:60px}"
          "p{color:#666}</style>"
          "<script>"
          "function poll(){"
            "fetch('/').then(function(r){"
              "if(r.ok)window.location.href='/';"
              "else setTimeout(poll,1000);"
            "}).catch(function(){setTimeout(poll,1000);});"
          "}"
          "setTimeout(poll,4000);"
          "</script>"
          "</head><body>"
          "<h2>Saved!</h2>"
          "<p>Rebooting&hellip;</p>"
          "<p>Returning to settings when back online.</p>"
          "</body></html>"));
    delay(1500);
    ESP.restart();
}

void WebUI::handleControl() {
    if (_server.hasArg("screen") && _screenMode) {
        String s = _server.arg("screen");
        if      (s == "scan")    *_screenMode = ScreenMode::Scanning;
        else if (s == "history") *_screenMode = ScreenMode::History;
        else if (s == "summary") *_screenMode = ScreenMode::Summary;
        else if (s == "debug")   *_screenMode = ScreenMode::Debug;
    }
    _server.send(200, "text/plain", "OK");
}

void WebUI::handleClear() {
    Preferences prefs;
    prefs.begin("plantracker", false);
    prefs.clear();  // Erase all keys in this namespace
    prefs.end();

    _server.send(200, "text/html",
        F("<!DOCTYPE html><html><body style='font-family:sans-serif;text-align:center;margin-top:60px'>"
          "<h2>Settings cleared!</h2>"
          "<p>Rebooting with factory defaults...</p>"
          "</body></html>"));
    delay(1500);
    ESP.restart();
}

// Converts a uint16_t RGB565 color to a CSS hex string
String WebUI::rgb565ToCss(uint16_t c) {
    uint8_t r = ((c >> 11) & 0x1F) * 255 / 31;
    uint8_t g = ((c >> 5)  & 0x3F) * 255 / 63;
    uint8_t b = (c & 0x1F) * 255 / 31;
    char buf[8];
    snprintf(buf, sizeof(buf), "#%02X%02X%02X", r, g, b);
    return String(buf);
}

String WebUI::buildScreenDiv() {
    ScreenMode mode = _screenMode ? *_screenMode : ScreenMode::Scanning;
    String bg = "#000000", fg = "#FFFFFF", inner = "";

    auto buildAcInner = [&](const Aircraft& ac, bool hist, int histIdx, int histTotal) {
        bg = rgb565ToCss(_display.backgroundColorFor(ac.classification));
        fg = rgb565ToCss(_display.foregroundColorFor(ac.classification));

        String cs = ac.callsign.length() ? ac.callsign : "N/A";
        if (cs.length() > 10) cs = cs.substring(0, 10);

        inner += "<div style='font-size:26px;line-height:1.2;margin:4px 0'>" + cs + "</div>";
        inner += "<div>Type: " + String(ac.type.length() ? ac.type.c_str() : "???") + "</div>";

        char buf[40];
        if (ac.altitude > 0) snprintf(buf, sizeof(buf), "Alt:  %.0f ft", ac.altitude);
        else                  strcpy (buf, "Alt:  ground");
        inner += "<div>" + String(buf) + "</div>";

        if (!hist) {
            int rawEta = ac.etaSeconds(_cfg.latitude, _cfg.longitude, _cfg.radius);
            if (rawEta < 0) {
                inner += "<div>ETA:  --:--</div>";
            } else {
                int elapsed = (int)((millis() - ac.positionTimestamp) / 1000);
                int eta = max(0, rawEta - elapsed);
                snprintf(buf, sizeof(buf), "ETA:  %d:%02d", eta / 60, eta % 60);
                inner += "<div>" + String(buf) + "</div>";
            }
        }

        // Bottom banners
        if (hist) {
            char bar[20];
            snprintf(bar, sizeof(bar), "[ %d/%d ]", histIdx + 1, histTotal);
            inner += "<div style='position:absolute;bottom:0;left:0;right:0;"
                     "background:#FF0000;color:#FFFFFF;text-align:center;"
                     "padding:2px;font-size:10px'>" + String(bar) + "</div>";
            inner += "<div style='position:absolute;bottom:14px;left:0;right:0;"
                     "background:#404040;color:#FFFFFF;text-align:center;"
                     "padding:2px;font-size:14px'>" +
                     String(aircraftClassName(ac.classification)) + "</div>";
        } else {
            inner += "<div style='position:absolute;bottom:0;left:0;right:0;"
                     "background:#404040;color:#FFFFFF;text-align:center;"
                     "padding:4px;font-size:14px'>" +
                     String(aircraftClassName(ac.classification)) + "</div>";
        }
    };

    if (_inSetupMode) {
        inner = "<div style='font-size:20px;margin-bottom:6px'>SETUP</div>"
                "<div>SSID: PlaneTracker</div>"
                "<div>Pass: PlaneTracker</div>"
                "<div style='margin-top:8px'>Open browser:</div>"
                "<div>192.168.4.1</div>";
    } else if (mode == ScreenMode::Scanning && _store.hasActiveAircraft()) {
        const Aircraft* ac = _store.currentAircraft();
        if (ac) buildAcInner(*ac, false, 0, 0);
    } else if (mode == ScreenMode::Scanning) {
        inner = "<div style='position:absolute;top:50%;left:50%;"
                "transform:translate(-50%,-50%);font-size:22px;"
                "white-space:nowrap'>SCANNING</div>";
    } else if (mode == ScreenMode::History) {
        if (_store.historyCount() > 0)
            buildAcInner(_store.historyAt(_store.historyIndex()),
                         true, _store.historyIndex(), _store.historyCount());
        else
            inner = "<div style='margin-top:50%'>No history</div>";
    } else if (mode == ScreenMode::Summary) {
        char buf[64];
        inner += "<div style='font-size:20px;margin-bottom:8px'>SUMMARY</div>";
        snprintf(buf, sizeof(buf), "<div>MIL:   %3d</div>", _store.detectionCount(AircraftClass::Military));   inner += buf;
        snprintf(buf, sizeof(buf), "<div>MED:   %3d</div>", _store.detectionCount(AircraftClass::Medevac));    inner += buf;
        snprintf(buf, sizeof(buf), "<div>COMM:  %3d</div>", _store.detectionCount(AircraftClass::Commercial)); inner += buf;
        snprintf(buf, sizeof(buf), "<div>PRIV:  %3d</div>", _store.detectionCount(AircraftClass::Private));    inner += buf;
    } else if (mode == ScreenMode::Debug) {
        const std::vector<String>& lines = _store.apiResponseLines();
        inner += "<div>DBG HTTP:" + String(_store.lastResponseCode()) +
                 " ac:" + String(_store.lastAircraftCount()) + "</div>";
        inner += "<div>IP: " + _ipAddress + "</div>";
        int shown = min((int)lines.size(), 13);
        for (int i = 0; i < shown; i++) {
            String line = lines[i];
            if (line.length() > 21) line = line.substring(0, 21);
            inner += "<div>" + line + "</div>";
        }
    }

    return "<div id='scr' style='background:" + bg + ";color:" + fg + ";"
           "width:256px;height:256px;display:inline-block;position:relative;"
           "font-family:monospace;font-size:12px;padding:6px;box-sizing:border-box;"
           "border:4px solid #222;border-radius:6px;overflow:hidden;text-align:left'>"
           + inner + "</div>";
}

void WebUI::handleScreen() {
    if (_server.hasArg("fragment")) {
        _server.send(200, "text/html", buildScreenDiv());
        return;
    }

    uint32_t refreshSec = max(5u, _cfg.pollIntervalMs / 1000);
    String html =
        "<!DOCTYPE html><html><head>"
        "<title>PlaneTracker - Screen</title>"
        "<meta name='viewport' content='width=device-width,initial-scale=1'>"
        "<meta http-equiv='refresh' content='" + String(refreshSec) + "'>"
        "<style>body{margin:20px;font-family:sans-serif;text-align:center}</style>"
        "</head><body>"
        "<h2>Device Screen</h2>" +
        buildScreenDiv() +
        "<p style='color:#666;font-size:.85em'>Auto-refreshes every " + String(refreshSec) + "s</p>"
        "<p><a href='/'>Settings</a></p>"
        "</body></html>";

    _server.send(200, "text/html", html);
}
