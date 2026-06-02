#include "WebUI.h"
#include "Notifier.h"
#include <WiFi.h>
#include <freertos/task.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <mbedtls/sha256.h>
#include <esp_random.h>

// ── Password / auth helpers ───────────────────────────────────────────────────

static String computePasswordHash(const String& password, const String& salt) {
    String input = salt + password;
    uint8_t digest[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, (const uint8_t*)input.c_str(), input.length());
    mbedtls_sha256_finish(&ctx, digest);
    mbedtls_sha256_free(&ctx);

    char hex[65];
    for (int i = 0; i < 32; i++) snprintf(hex + i * 2, 3, "%02x", digest[i]);
    hex[64] = '\0';
    return String(hex);
}

static String generateHexRandom(int bytes) {
    String out;
    out.reserve(bytes * 2);
    for (int i = 0; i < bytes; i += 4) {
        uint32_t r = esp_random();
        char buf[9];
        snprintf(buf, sizeof(buf), "%08x", r);
        int take = min(8, (bytes - i) * 2);
        out += String(buf).substring(0, take);
    }
    return out;
}

static String generateSalt()  { return generateHexRandom(16); }  // 32-char hex
static String generateToken() { return generateHexRandom(16); }  // 32-char hex, reused for session

static String htmlEscape(const char* s) {
    String out;
    out.reserve(strlen(s));
    for (; *s; s++) {
        switch (*s) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += *s;       break;
        }
    }
    return out;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void WebUI::begin(bool isSetupMode) {
    _inSetupMode = isSetupMode;

    if (isSetupMode) {
        _ipAddress = "192.168.4.1";
        WiFi.mode(WIFI_AP);
        WiFi.softAP("PlaneTracker", "PlaneTracker");
    }

    _server.on("/",               HTTP_GET,  [this]() { handleRoot();           });
    _server.on("/save",           HTTP_POST, [this]() { handleSave();           });
    _server.on("/clear",          HTTP_POST, [this]() { handleClear();          });
    _server.on("/control",        HTTP_GET,  [this]() { handleControl();        });
    _server.on("/screen",         HTTP_GET,  [this]() { handleScreen();         });
    _server.on("/notify-test",    HTTP_POST, [this]() { handleNotifyTest();     });
    _server.on("/ntfy-stats",     HTTP_GET,  [this]() { handleNtfyStats();      });
    _server.on("/api-test",       HTTP_GET,  [this]() { handleApiTest();        });
    _server.on("/ota-check",      HTTP_GET,  [this]() { handleOtaCheck();       });
    _server.on("/ota-update",     HTTP_POST, [this]() { handleOtaUpdate();      });
    _server.on("/aircraft",       HTTP_GET,  [this]() { handleAircraft();       });
    _server.on("/history",        HTTP_GET,  [this]() { handleHistory();        });
    _server.on("/clear-summary",  HTTP_POST, [this]() { handleClearSummary();   });
    _server.on("/login",          HTTP_POST, [this]() { handleLoginPost();      });
    _server.on("/forgot-password",HTTP_GET,  [this]() { handleForgotPassword(); });
    _server.on("/pin-reset",      HTTP_POST, [this]() { handlePinReset();       });
    _server.on("/pin-cancel",     HTTP_POST, [this]() { handlePinCancel();      });
    _server.on("/pin-status",     HTTP_GET,  [this]() { handlePinStatus();      });

    const char* headers[] = {"X-Auth-Token"};
    _server.collectHeaders(headers, 1);
    _server.begin();
}

void WebUI::processRequests() {
    _server.handleClient();
}

bool WebUI::checkAuth() {
    if (_inSetupMode) return true;
    if (!_cfg.requireWebPassword || !_cfg.hasWebPassword()) return true;
    if (_sessionToken.length() == 0) {
        _server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
        return false;
    }
    String token = _server.header("X-Auth-Token");
    if (token.length() > 0 && token == _sessionToken) return true;
    _server.send(401, "application/json", "{\"error\":\"unauthorized\"}");
    return false;
}

// ── Handlers ──────────────────────────────────────────────────────────────────

void WebUI::handleRoot() {
    String html =
        F("<!DOCTYPE html><html><head><title>PlaneTracker</title>"
          "<meta name='viewport' content='width=device-width,initial-scale=1'>"
          "<style>"
          "body{font-family:sans-serif;max-width:820px;margin:24px auto;padding:0 12px;"
          "background:#fff;color:#222;transition:background .2s,color .2s}"
          "h2{margin-bottom:16px}h3{margin:0 0 10px}"
          "label{display:block;margin-top:10px;font-size:.9em}"
          "input{width:100%;padding:7px;box-sizing:border-box;margin-top:3px;font-size:1em;"
          "background:#fff;color:#222;border:1px solid #ccc;border-radius:3px}"
          ".btn{display:block;margin-top:18px;width:100%;padding:10px;background:#1976D2;"
          "color:#fff;border:none;font-size:1em;cursor:pointer;border-radius:4px}"
          ".layout{display:flex;flex-wrap:wrap;gap:32px;align-items:flex-start}"
          ".form-col{flex:1;min-width:280px}"
          ".preview-col{flex:0 0 auto;text-align:center;order:-1;min-width:285px}"
          ".lv-tab-hdr{display:flex;background:#f5f5f5;border-bottom:1px solid #ddd}"
          ".lv-tab-btn{flex:1;padding:8px 4px;border:none;background:none;cursor:pointer;"
          "font-size:.82em;border-bottom:3px solid transparent;color:#555}"
          ".lv-tab-btn.on{background:#fff;border-bottom-color:#1976D2;color:#1976D2;font-weight:600}"
          ".lv-tab-panel{display:none}"
          ".lv-tab-panel.on{display:block}"
          "body.dark .lv-tab-hdr{background:#252525;border-color:#444}"
          "body.dark .lv-tab-btn{color:#aaa}"
          "body.dark .lv-tab-btn.on{background:#1a1a1a;border-bottom-color:#64b5f6;color:#64b5f6}"
          "#scr{width:256px;height:256px;display:inline-block;position:relative;"
          "font-family:monospace;font-size:12px;padding:6px;box-sizing:border-box;"
          "border:4px solid #222;border-radius:6px;overflow:hidden;text-align:left}"
          ".ctrl{display:flex;gap:6px;margin-top:8px}"
          ".cb{flex:1;padding:8px 0;background:#37474F;color:#fff;border:none;"
          "cursor:pointer;border-radius:4px;font-size:.78em;line-height:1.6}"
          ".cb:hover{background:#546E7A}.cb.on{background:#1976D2}"
          ".card{border:1px solid #ddd;border-radius:6px;margin-bottom:4px}"
          ".card-hdr{background:#f5f5f5;border-bottom:1px solid #ddd;padding:10px 14px;"
          "font-weight:600;font-size:.88em;color:#555;display:flex;align-items:center;gap:7px;"
          "border-radius:6px 6px 0 0}"
          ".card-body{padding:14px}"
          "body.dark .card{border-color:#444}"
          "body.dark .card-hdr{background:#252525;border-color:#444;color:#aaa}"
          ".tabs{border:1px solid #ddd;border-radius:6px;margin-bottom:4px}"
          ".tab-hdr{display:flex;background:#f5f5f5;border-bottom:1px solid #ddd;"
          "border-radius:6px 6px 0 0}"
          ".tab-btn{flex:1;padding:10px 4px;border:none;background:none;cursor:pointer;"
          "font-size:.85em;border-bottom:3px solid transparent;color:#555}"
          ".tab-btn.on{background:#fff;border-bottom-color:#1976D2;color:#1976D2;font-weight:600}"
          ".tab-panel{padding:14px;display:none}"
          ".tab-panel.on{display:block}"
          "body.dark .tabs{border-color:#444}"
          "body.dark .tab-hdr{background:#252525;border-color:#444}"
          "body.dark .tab-btn{color:#aaa}"
          "body.dark .tab-btn.on{background:#1a1a1a;border-bottom-color:#64b5f6;color:#64b5f6}"
          "body.dark #dmBtn{border-color:#666;color:#e0e0e0}"
          ".chk-wrap{position:relative;margin-top:3px}"
          ".chk-btn{width:100%;padding:7px;text-align:left;background:#fff;color:#222;"
          "border:1px solid #ccc;border-radius:3px;cursor:pointer;font-size:1em;"
          "display:flex;justify-content:space-between;align-items:center}"
          ".chk-drop{display:none;position:absolute;left:0;right:0;background:#fff;"
          "border:1px solid #ccc;border-top:none;border-radius:0 0 3px 3px;"
          "z-index:200;box-shadow:0 4px 8px rgba(0,0,0,.12)}"
          ".chk-drop.open{display:block}"
          ".chk-item{display:flex;align-items:center;gap:8px;padding:8px 10px;"
          "cursor:pointer;font-size:.95em;font-weight:400;margin:0}"
          ".chk-item:hover{background:#f0f0f0}"
          ".chk-item input{margin:0;cursor:pointer;width:auto;accent-color:#1976D2}"
          "body.dark .chk-btn{background:#2a2a2a;color:#e0e0e0;border-color:#555}"
          "body.dark .chk-drop{background:#2a2a2a;border-color:#555}"
          "body.dark .chk-item:hover{background:#333}"
          ".modal{display:none;position:fixed;inset:0;background:rgba(0,0,0,.6);"
          "z-index:1000;align-items:center;justify-content:center}"
          ".modal.open{display:flex}"
          "#mapWrap{background:#fff;padding:12px;border-radius:8px;width:min(92vw,520px)}"
          "#map{height:340px;width:100%;margin-top:8px}"
          ".map-hdr{display:flex;justify-content:space-between;align-items:center}"
          ".map-hint{font-size:.82em;color:#555;margin-top:4px}"
          "#dmBtn{position:fixed;top:10px;right:14px;background:none;border:1px solid #aaa;"
          "border-radius:20px;padding:4px 10px;cursor:pointer;font-size:1em;z-index:999}"
          "body.dark{background:#1a1a1a;color:#e0e0e0}"
          "body.dark input{background:#2a2a2a;color:#e0e0e0;border-color:#555}"
          "body.dark label{color:#bbb}"
          "body.dark small{color:#999}"
          "body.dark h2,body.dark h3{color:#e0e0e0}"
          ".sec{font-size:.78em;font-weight:700;text-transform:uppercase;letter-spacing:.06em;"
          "color:#666;margin-top:20px;padding-bottom:5px;border-bottom:1px solid #ddd}"
          ".sec:first-child{margin-top:4px}"
          "body.dark .sec{color:#aaa;border-bottom-color:#444}"
          "body.dark #mapWrap{background:#2a2a2a;color:#e0e0e0}"
          "body.dark .map-hint{color:#aaa}"
          "body.dark #dmBtn{border-color:#666;color:#e0e0e0}"
          "body.dark a{color:#64b5f6}"
          "body.dark #ntfyUsage{color:#aaa}"
          "body.dark .usage-box{border-color:#444}"
          ".ver-box{padding:7px;font-family:monospace;border:1px solid #ccc;border-radius:3px;"
          "margin-top:3px;background:#f5f5f5;color:#222}"
          "body.dark .ver-box{background:#2a2a2a;border-color:#555;color:#e0e0e0}"
          "body.dark #otaStatus{color:#aaa}"
          ".modal-box{background:#fff;color:#222;padding:32px 28px;border-radius:10px;"
          "text-align:center;max-width:320px;width:90vw}"
          ".modal-hint{font-size:.85em;color:#666;margin-bottom:4px}"
          ".modal-cancel{flex:1;padding:10px;background:#eee;color:#222;border:none;"
          "border-radius:4px;cursor:pointer;font-size:.9em}"
          "body.dark .modal-box{background:#2a2a2a;color:#e0e0e0}"
          "body.dark .modal-hint{color:#999}"
          "body.dark .modal-cancel{background:#444;color:#e0e0e0}"
          ".wifi-warn{display:none;margin:16px auto 0;max-width:280px;padding:12px 14px;"
          "background:#FFF8E1;border:1px solid #FFB300;border-radius:6px;"
          "color:#5D4037;font-size:.82em;text-align:left;line-height:1.5}"
          "body.dark .wifi-warn{background:#2d2500;border-color:#6d5800;color:#e0c84a}"
          "#apiOut{background:#111;color:#00ff00;font-family:monospace;font-size:11px;"
          "padding:10px;border-radius:4px;height:320px;overflow-y:auto;"
          "white-space:pre-wrap;word-break:break-all;margin-top:10px;"
          "border:1px solid #333}"
          "@keyframes sqwkFlash{0%,100%{color:#ff0000;background:transparent}50%{color:#fff;background:#ff0000}}"
          ".sqwk-emrg{animation:sqwkFlash 0.6s infinite;font-weight:bold;border-radius:2px;padding:0 2px}"
          "#liveMap:fullscreen,#liveMap:-webkit-full-screen{"
          "width:100vw!important;height:100vh!important}"
          "#liveWrap:fullscreen{background:#000;display:flex;flex-direction:column;"
          "align-items:center;justify-content:center;gap:12px}"
          "#liveWrap:fullscreen #scr{width:min(80vh,80vw)!important;height:min(80vh,80vw)!important;"
          "font-size:calc(min(80vh,80vw)*12/256)!important}"
          "#liveWrap:fullscreen .ctrl{width:min(80vh,80vw);display:flex;gap:6px}"
          "#liveWrap:fullscreen .cb{flex:1;padding:12px 0;font-size:.9em}"
          ".auth-overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.85);"
          "z-index:9999;align-items:center;justify-content:center}"
          ".auth-overlay.open{display:flex}"
          "body.pre-auth>*:not(#authOverlay){display:none!important}"
          "#logoutBtn{position:fixed;top:10px;right:56px;background:none;"
          "border:1px solid #aaa;border-radius:20px;padding:4px 10px;"
          "cursor:pointer;font-size:1em;z-index:999;display:none}"
          "body.dark #logoutBtn{border-color:#666;color:#e0e0e0}"
          ".auth-box{background:#fff;color:#222;padding:32px 28px;border-radius:10px;"
          "max-width:320px;width:90vw}"
          ".auth-box h3{margin:0 0 16px;font-size:1.2em;text-align:center}"
          ".auth-err{color:#c00;font-size:.85em;margin-top:6px;display:none}"
          ".pin-sect{display:none;margin-top:16px;border-top:1px solid #ddd;padding-top:14px}"
          "body.dark .auth-box{background:#2a2a2a;color:#e0e0e0}"
          "body.dark .pin-sect{border-color:#444}"
          "body.dark .auth-overlay .modal-cancel{background:#444;color:#e0e0e0}"
          "</style>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'>"
          "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css'>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<script>"
          // ── Auth helpers ──────────────────────────────────────────────────────
          "var _authLocked=false;"
          "function _getTok(){return sessionStorage.getItem('pt_tok')||'';}"
          "function _authHdrs(){var t=_getTok();return t?{'X-Auth-Token':t}:{};}"
          "function authFetch(url,opts){"
            "opts=opts||{};"
            "opts.headers=Object.assign({},opts.headers||{},_authHdrs());"
            "return fetch(url,opts).then(function(r){"
              "if(r.status===401){"
                "sessionStorage.removeItem('pt_tok');"
                "showAuthOverlay();"
                "return new Promise(function(){});"
              "}"
              "return r;"
            "});"
          "}"
          "function showAuthOverlay(){"
            "var _ov=document.getElementById('authOverlay');"
            "if(_ov.classList.contains('open'))return;"
            "['saveModal','clearModal','otaModal'].forEach(function(id){"
              "var e=document.getElementById(id);if(e)e.classList.remove('open');"
            "});"
            "_ov.classList.add('open');"
            "setTimeout(function(){document.getElementById('authPwd').focus();},50);"
          "}"
          "function hideAuthOverlay(){"
            "stopPinPoll();"
            "document.getElementById('authOverlay').classList.remove('open');"
            "document.body.classList.remove('pre-auth');"
            "_authLocked=false;"
            "document.getElementById('authLoginView').style.display='';"
            "document.getElementById('authLockout').style.display='none';"
            "document.getElementById('authCode').style.display='none';"
            "document.getElementById('authPinSect').style.display='none';"
            "document.getElementById('authErr').style.display='none';"
            "document.getElementById('authPinErr').style.display='none';"
            "if(cfgAuthRequired){var lb=document.getElementById('logoutBtn');if(lb)lb.style.display='block';}"
          "}"
          "function doLogout(){"
            "stopPinPoll();"
            "sessionStorage.removeItem('pt_tok');"
            "_authLocked=false;"
            "document.getElementById('authPwd').value='';"
            "document.getElementById('authErr').style.display='none';"
            "document.getElementById('authPinErr').style.display='none';"
            "document.getElementById('authLockout').style.display='none';"
            "document.getElementById('authCode').style.display='none';"
            "document.getElementById('authPinSect').style.display='none';"
            "document.getElementById('authLoginView').style.display='';"
            "document.getElementById('logoutBtn').style.display='none';"
            "document.body.classList.add('pre-auth');"
            "document.getElementById('authOverlay').classList.add('open');"
            "setTimeout(function(){document.getElementById('authPwd').focus();},50);"
          "}"
          "function doLogin(){"
            "var pwd=document.getElementById('authPwd').value;"
            "var e=document.getElementById('authErr');e.style.display='none';"
            "var f=new FormData();"
            "f.append('password',pwd);"
            "if(_authLocked){"
              "var code=document.getElementById('authCode').value;"
              "f.append('code',code);"
            "}"
            "fetch('/login',{method:'POST',body:f})"
            ".then(function(r){return r.json();})"
            ".then(function(d){"
              "if(d.ok){"
                "sessionStorage.setItem('pt_tok',d.token);"
                "hideAuthOverlay();"
              "}else if(d.lockout){"
                "_authLocked=true;"
                "document.getElementById('authLockout').style.display='';"
                "document.getElementById('authCode').style.display='';"
                "startPinPoll();"
                "e.style.display='block';"
                "e.textContent=d.wrongCode"
                  "?(d.regen?'Wrong code (3 attempts - new code shown on device).':'Wrong security code. Try again.')"
                  ":'Too many attempts. Check your device for a security code.';"
              "}else{"
                "_authLocked=false;"
                "document.getElementById('authLockout').style.display='none';"
                "document.getElementById('authCode').style.display='none';"
                "e.style.display='block';e.textContent='Incorrect password.';"
              "}"
            "}).catch(function(){e.style.display='block';e.textContent='Request failed.';});"
          "}"
          "function showForgot(){"
            "fetch('/forgot-password')"
            ".then(function(r){return r.json();})"
            ".then(function(d){"
              "if(d.ok){"
                "document.getElementById('authLoginView').style.display='none';"
                "document.getElementById('authPinSect').style.display='block';"
                "startPinPoll();"
                "setTimeout(function(){document.getElementById('authPin').focus();},50);"
              "}else alert('Could not start PIN reset. Try again.');"
            "}).catch(function(){alert('Request failed.');});"
          "}"
          "function doReset(){"
            "var pin=document.getElementById('authPin').value;"
            "var pwd=document.getElementById('authNewPwd').value;"
            "var e=document.getElementById('authPinErr');e.style.display='none';"
            "var f=new FormData();f.append('pin',pin);f.append('newPass',pwd);"
            "fetch('/pin-reset',{method:'POST',body:f})"
            ".then(function(r){return r.json();})"
            ".then(function(d){"
              "if(d.ok){sessionStorage.setItem('pt_tok',d.token);hideAuthOverlay();}"
              "else{"
                "e.style.display='block';"
                "e.textContent=d.regen"
                  "?'Wrong PIN (3 attempts - new PIN shown on device).'"
                  ":'Wrong PIN. Try again.';"
                "document.getElementById('authPin').value='';"
              "}"
            "}).catch(function(){e.style.display='block';e.textContent='Request failed.';});"
          "}"
          "var _pinPoll=null;"
          "function startPinPoll(){"
            "if(_pinPoll)return;"
            "_pinPoll=setInterval(function(){"
              "fetch('/pin-status')"
              ".then(function(r){return r.json();})"
              ".then(function(d){"
                "if(d.active)return;"
                "stopPinPoll();"
                // Device button cancelled — reset whichever PIN flow was active
                "var ps=document.getElementById('authPinSect');"
                "if(ps&&ps.style.display==='block'){"
                  "ps.style.display='none';"
                  "document.getElementById('authLoginView').style.display='';"
                "}"
                "if(_authLocked){"
                  "_authLocked=false;"
                  "document.getElementById('authLockout').style.display='none';"
                  "document.getElementById('authCode').style.display='none';"
                "}"
                "setTimeout(function(){document.getElementById('authPwd').focus();},50);"
              "}).catch(function(){});"
            "},2000);"
          "}"
          "function stopPinPoll(){"
            "clearInterval(_pinPoll);_pinPoll=null;"
          "}"
          "function doCancelReset(){"
            "fetch('/pin-cancel',{method:'POST'}).then(function(){}).catch(function(){});"
            "stopPinPoll();"
            "document.getElementById('authPinSect').style.display='none';"
            "document.getElementById('authLoginView').style.display='';"
            "setTimeout(function(){document.getElementById('authPwd').focus();},50);"
          "}"
          // ─────────────────────────────────────────────────────────────────────
          "function startRadar(c){"
            "if(window._radarRaf){cancelAnimationFrame(window._radarRaf);window._radarRaf=null;}"
            "var ctx=c.getContext('2d');"
            "var w=c.width,h=c.height,cx=w/2;"
            "var oR=70,iR=36,N=36,fr=0;"
            "var gap=14,textH=12;"
            "var cy=(h-(2*oR+gap+textH))/2+oR;"
            "var textY=cy+oR+gap+textH;"
            "var trail=['#003200','#008200','#00d200','#00ff00'];"
            "function draw(){"
              "ctx.fillStyle='#000';ctx.fillRect(0,0,w,h);"
              "ctx.lineWidth=1;"
              "ctx.strokeStyle='#00ff00';ctx.beginPath();"
              "ctx.arc(cx,cy,oR,0,Math.PI*2);ctx.stroke();"
              "ctx.strokeStyle='#003200';ctx.beginPath();"
              "ctx.arc(cx,cy,iR,0,Math.PI*2);ctx.stroke();"
              "ctx.fillStyle='#00ff00';ctx.fillRect(cx,cy,2,2);"
              "for(var t=0;t<4;t++){"
                "var a=(fr-(3-t))*Math.PI*2/N;"
                "ctx.strokeStyle=trail[t];ctx.lineWidth=t===3?2:1;"
                "ctx.beginPath();ctx.moveTo(cx,cy);"
                "ctx.lineTo(cx+oR*Math.sin(a),cy-oR*Math.cos(a));ctx.stroke();"
              "}"
              "ctx.fillStyle='#00ff00';ctx.font='bold 12px monospace';"
              "var tw=ctx.measureText('SCANNING').width;"
              "ctx.fillText('SCANNING',(w-tw)/2,textY);"
              "fr+=0.25;window._radarRaf=requestAnimationFrame(draw);"
            "}"
            "draw();"
          "}"
          "var _etaInterval=null;"
          "function startEtaCountdown(){"
            "if(_etaInterval)clearInterval(_etaInterval);"
            "var cells=document.querySelectorAll('[data-eta]');"
            "if(!cells.length)return;"
            "var start=Date.now();"
            "cells.forEach(function(c){c._etaStart=parseInt(c.dataset.eta);});"
            "_etaInterval=setInterval(function(){"
              "var elapsed=Math.round((Date.now()-start)/1000);"
              "cells.forEach(function(c){"
                "var rem=c._etaStart-elapsed;"
                "if(rem<=0){c.textContent='0:00';return;}"
                "c.textContent=Math.floor(rem/60)+':'+(rem%60<10?'0':'')+(rem%60);"
              "});"
            "},1000);"
          "}"
          "function refresh(){"
            "authFetch('/screen?fragment=1')"
            ".then(function(r){return r.text();})"
            ".then(function(h){"
              "if(window._radarRaf){cancelAnimationFrame(window._radarRaf);window._radarRaf=null;}"
              "if(window._radarBlipsTimer){clearTimeout(window._radarBlipsTimer);window._radarBlipsTimer=null;}"
              "var e=document.getElementById('scrWrap');"
              "if(e)e.innerHTML=h;"
              "var rc=document.getElementById('rc');"
              "if(rc)startRadar(rc);"
              "var rb=document.getElementById('radarBc');"
              "if(rb)startRadarBlips(rb);"
              "startEtaCountdown();"
            "})"
            ".catch(function(){});"
          "var _mp=document.getElementById('lvtab-map');"
          "if(_mp&&_mp.classList.contains('on'))refreshLiveMap();"
          "}"
          "function ctrl(s){"
            "authFetch('/control?screen='+s).then(function(){setTimeout(refresh,300);});"
            "document.querySelectorAll('.cb').forEach(function(b){"
              "b.classList.toggle('on',b.dataset.s===s);});"
          "}"
          "function startRadarBlips(c){"
            "if(window._radarBlipsTimer)clearTimeout(window._radarBlipsTimer);"
            "var ctx=c.getContext('2d'),W=c.width,H=c.height;"
            "var cx=W/2,cy=H/2,oR=Math.min(W,H)/2-18,iR=oR/2;"
            "function bearNm(lat,lon){"
              "var dlon=(lon-cfgLon)*Math.cos(cfgLat*Math.PI/180);"
              "var dlat=lat-cfgLat;"
              "var brg=(Math.atan2(dlon,dlat)*180/Math.PI+360)%360;"
              "var dist=Math.sqrt(dlat*dlat+dlon*dlon)*60;"
              "return{brg:brg,dist:dist};"
            "}"
            "function draw(aircraft){"
              "ctx.fillStyle='#000';ctx.fillRect(0,0,W,H);"
              "ctx.strokeStyle='#004400';ctx.lineWidth=1;"
              "[oR,iR].forEach(function(r){"
                "ctx.beginPath();ctx.arc(cx,cy,r,0,Math.PI*2);ctx.stroke();"
              "});"
              "ctx.beginPath();ctx.moveTo(cx,cy-oR);ctx.lineTo(cx,cy+oR);ctx.stroke();"
              "ctx.beginPath();ctx.moveTo(cx-oR,cy);ctx.lineTo(cx+oR,cy);ctx.stroke();"
              "ctx.fillStyle='#00cc00';ctx.beginPath();ctx.arc(cx,cy,3,0,Math.PI*2);ctx.fill();"
              "ctx.fillStyle='#00cc00';ctx.font='bold 13px monospace';"
              "ctx.textAlign='center';"
              "ctx.fillText('N',cx,cy-oR-5);"
              "ctx.fillText('S',cx,cy+oR+14);"
              "ctx.textAlign='left'; ctx.fillText('E',cx+oR+4,cy+5);"
              "ctx.textAlign='right';ctx.fillText('W',cx-oR-4,cy+5);"
              "if(aircraft)aircraft.forEach(function(ac){"
                "if(!ac.lat||!ac.lon)return;"
                "var bn=bearNm(ac.lat,ac.lon);"
                "var ratio=Math.min(bn.dist/(cfgRadiusM/1852),1);"
                "var rad=bn.brg*Math.PI/180;"
                "var bx=cx+ratio*oR*Math.sin(rad);"
                "var by=cy-ratio*oR*Math.cos(rad);"
                "var tRad=(ac.track||0)*Math.PI/180,r=6;"
                "var ttx=bx+r*Math.sin(tRad),tty=by-r*Math.cos(tRad);"
                "var tlx=bx+r*0.65*Math.sin(tRad+2.3),tly=by-r*0.65*Math.cos(tRad+2.3);"
                "var trx=bx+r*0.65*Math.sin(tRad-2.3),try_=by-r*0.65*Math.cos(tRad-2.3);"
                "ctx.fillStyle=ac.bg;"
                "ctx.beginPath();ctx.moveTo(ttx,tty);ctx.lineTo(tlx,tly);ctx.lineTo(trx,try_);ctx.closePath();ctx.fill();"
                "ctx.fillStyle='#fff';"
                "ctx.beginPath();ctx.arc(ttx,tty,1.5,0,Math.PI*2);ctx.fill();"
              "});"
              "if(aircraft&&aircraft.length){"
                "ctx.fillStyle='#00cc00';ctx.font='bold 12px monospace';"
                "ctx.textAlign='left';ctx.fillText(aircraft.length+' AC',6,16);"
              "}"
            "}"
            "draw(null);"
            "authFetch('/aircraft').then(function(r){return r.json();})"
            ".then(function(d){draw(d.aircraft);})"
            ".catch(function(){});"
            "window._radarBlipsTimer=setTimeout(function(){"
              "var el=document.getElementById('radarBc');if(el)startRadarBlips(el);"
            "},5000);"
          "}"
          "function clearSummary(){"
            "showConfirm("
              "'<i class=\"fa-solid fa-eraser\" style=\"color:#37474F\"></i>',"
              "'Clear Summary?',"
              "'Reset all session totals to zero. This cannot be undone.',"
              "'Clear','#37474F',"
              "function(){authFetch('/clear-summary',{method:'POST'}).then(function(){refresh();});}"
            ");"
          "}"
          "function isEmergencySqwk(s){return s==='7500'||s==='7600'||s==='7700';}"
          "function showLiveTab(id){"
            "document.querySelectorAll('.lv-tab-btn').forEach(function(b){b.classList.remove('on');});"
            "document.querySelectorAll('.lv-tab-panel').forEach(function(p){p.classList.remove('on');});"
            "document.querySelector('.lv-tab-btn[data-lvtab=\"'+id+'\"]').classList.add('on');"
            "document.getElementById('lvtab-'+id).classList.add('on');"
            "var fsb=document.getElementById('fsBtn');"
            "if(fsb)fsb.style.display=(id==='live'||id==='map')?'':'none';"
            "if(id==='map'){initLiveMap();setTimeout(function(){_lmap&&_lmap.invalidateSize();_rCircle&&_lmap.fitBounds(_rCircle.getBounds(),{padding:[20,20]});refreshLiveMap();},60);}"
            "if(id==='hist')refreshHistory();"
          "}"
          "var _lmap=null,_acMarkers={},_rCircle=null;"
          "function initLiveMap(){"
            "if(_lmap)return;"
            "_lmap=L.map('liveMap').setView([cfgLat,cfgLon],11);"
            "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',"
              "{attribution:'&copy; OpenStreetMap',maxZoom:18}).addTo(_lmap);"
            "_rCircle=L.circle([cfgLat,cfgLon],{radius:cfgRadiusM,"
              "color:'#1976D2',fillColor:'#1976D2',fillOpacity:0.06,weight:1.5}).addTo(_lmap);"
            "L.circleMarker([cfgLat,cfgLon],{radius:5,color:'#1976D2',"
              "fillColor:'#1976D2',fillOpacity:1}).addTo(_lmap);"
          "}"
          "function makeAcIcon(bg,fg,track){"
            "return L.divIcon({html:'<div style=\"width:26px;height:26px;background:'+bg+"
              "';border:2px solid rgba(0,0,0,0.25);border-radius:50%;display:flex;"
              "align-items:center;justify-content:center\">"
              "<span style=\"transform:rotate('+(track-90)+'deg);display:inline-block;"
              "font-size:14px;color:'+fg+'\">&#9992;</span></div>',"
              "iconSize:[26,26],iconAnchor:[13,13],className:''});"
          "}"
          "function refreshLiveMap(){"
            "if(!_lmap)return;"
            "authFetch('/aircraft').then(function(r){return r.json();})"
            ".then(function(d){"
              "Object.keys(_acMarkers).forEach(function(k){_lmap.removeLayer(_acMarkers[k]);});"
              "_acMarkers={};"
              "d.aircraft.forEach(function(ac){"
                "if(!ac.lat&&!ac.lon)return;"
                "var m=L.marker([ac.lat,ac.lon],{icon:makeAcIcon(ac.bg,ac.fg,ac.track)}).addTo(_lmap);"
                "var sqwkHtml=ac.sqwk?(isEmergencySqwk(ac.sqwk)?"
                  "'<br><span class=\\'sqwk-emrg\\'>SQK '+ac.sqwk+'</span>':"
                  "'<br>SQK: '+ac.sqwk):'';"
                "m.bindPopup('<b>'+ac.callsign+'</b><br>'+ac.type+'<br>'"
                  "+'<b>'+Math.round(ac.alt)+'ft</b> &bull; '+ac.speed+'kt'"
                  "+'<br>'+ac.dist+'NM '+ac.compass+(ac.inbound?' &#x25b2;':' &#x25bc;')"
                  "+sqwkHtml);"
                "_acMarkers[ac.icao]=m;"
              "});"
            "}).catch(function(){});"
          "}"
          "function refreshHistory(){"
            "authFetch('/history').then(function(r){return r.json();})"
            ".then(function(d){"
              "var el=document.getElementById('histPanel');"
              "if(!d.aircraft.length){"
                "el.innerHTML='<p style=\"color:#888;text-align:center;margin:20px 0\">No history yet.</p>';"
                "return;"
              "}"
              "var t='<table style=\"width:100%;border-collapse:collapse;font-family:monospace;font-size:11px\">'+"
                "'<thead><tr style=\"border-bottom:1px solid #888\">'+"
                "'<th style=\"text-align:center;padding:2px 4px\">Cat</th>'+"
                "'<th style=\"text-align:left;padding:2px 4px\">Tail</th>'+"
                "'<th style=\"text-align:left;padding:2px 4px\">Type</th>'+"
                "'<th style=\"text-align:right;padding:2px 4px\">Alt</th>'+"
                "'<th style=\"text-align:right;padding:2px 4px\">Spd</th>'+"
                "'<th style=\"text-align:center;padding:2px 4px\">SQK</th>'+"
                "'</tr></thead><tbody>';"
              "d.aircraft.forEach(function(ac){"
                "var sq=ac.sqwk?ac.sqwk:'----';"
                "var sqCell=isEmergencySqwk(ac.sqwk)?'<span class=\\'sqwk-emrg\\'>'+sq+'</span>':sq;"
                "var cat=ac.cls.charAt(0);"
                "t+='<tr style=\"border-bottom:1px solid #333\">'+"
                  "'<td style=\"text-align:center;padding:2px 4px\">'+"
                  "'<span style=\"background:'+ac.bg+';color:'+ac.fg+';padding:1px 5px;border-radius:3px;font-weight:bold\">'+cat+'</span></td>'+"
                  "'<td style=\"padding:2px 4px\"><a href=\"'+ac.url+'\" target=\"_blank\" style=\"color:inherit\">'+ac.callsign+'</a></td>'+"
                  "'<td style=\"padding:2px 4px\">'+ac.type+'</td>'+"
                  "'<td style=\"text-align:right;padding:2px 4px\">'+(ac.alt>0?Math.round(ac.alt)+'ft':'GND')+'</td>'+"
                  "'<td style=\"text-align:right;padding:2px 4px\">'+(ac.speed>0?Math.round(ac.speed)+'kt':'-')+'</td>'+"
                  "'<td style=\"text-align:center;padding:2px 4px\">'+sqCell+'</td>'+"
                  "'</tr>';"
              "});"
              "t+='</tbody></table>';"
              "el.innerHTML=t;"
            "}).catch(function(){});"
          "}"
          "var _map=null,_marker=null;"
          "function openMap(){"
            "var lat=parseFloat(document.querySelector('[name=lat]').value)||0;"
            "var lon=parseFloat(document.querySelector('[name=lon]').value)||0;"
            "document.getElementById('mapModal').classList.add('open');"
            "if(!_map){"
              "_map=L.map('map').setView([lat||40,lon||-95],lat?12:4);"
              "L.tileLayer('https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png',"
                "{attribution:'&copy; OpenStreetMap contributors',maxZoom:19}"
              ").addTo(_map);"
              "_map.on('click',function(e){"
                "document.querySelector('[name=lat]').value=e.latlng.lat.toFixed(6);"
                "document.querySelector('[name=lon]').value=e.latlng.lng.toFixed(6);"
                "if(_marker)_marker.setLatLng(e.latlng);"
                "else _marker=L.marker(e.latlng).addTo(_map);"
              "});"
              "if(lat&&lon)_marker=L.marker([lat,lon]).addTo(_map);"
            "}else{"
              "if(lat&&lon){_map.setView([lat,lon],12);"
              "if(_marker)_marker.setLatLng([lat,lon]);"
              "else _marker=L.marker([lat,lon]).addTo(_map);}"
            "}"
            "setTimeout(function(){_map.invalidateSize();},60);"
          "}"
          "function closeMap(){"
            "document.getElementById('mapModal').classList.remove('open');"
          "}"
          "function loadNtfyStats(btn){"
            "if(btn){btn.disabled=true;btn.textContent='Checking...';}"
            "authFetch('/ntfy-stats')"
            ".then(function(r){return r.json();})"
            ".then(function(d){"
              "var el=document.getElementById('ntfyUsage');"
              "if(!el)return;"
              "if(d.error){"
                "el.textContent=d.error==='not_configured'?'Configure token first':'Error: '+d.error;"
              "}else{"
                "var pct=d.limit>0?Math.round(d.sent/d.limit*100):0;"
                "el.innerHTML=d.sent+' sent / '+d.limit+(d.limit>0?' ('+pct+'%)':'')"
                "+'<br><span style=\"color:#4CAF50\">'+d.remaining+' remaining</span>';"
              "}"
              "if(btn){btn.disabled=false;btn.textContent='Refresh';}"
            "}).catch(function(){"
              "var el=document.getElementById('ntfyUsage');"
              "if(el)el.textContent='Request failed';"
              "if(btn){btn.disabled=false;btn.textContent='Refresh';}"
            "});"
          "}"
          "function runApiTest(){"
            "var btn=document.getElementById('apiBtn');"
            "var pre=document.getElementById('apiOut');"
            "var lat=document.querySelector('[name=lat]').value;"
            "var lon=document.querySelector('[name=lon]').value;"
            "var rad=document.querySelector('[name=radius]').value;"
            "btn.disabled=true;btn.textContent='Fetching...';"
            "pre.textContent='Waiting for response...';"
            "authFetch('/api-test?lat='+lat+'&lon='+lon+'&radius='+rad)"
            ".then(function(r){return r.text();})"
            ".then(function(t){"
              "try{pre.textContent=JSON.stringify(JSON.parse(t),null,2);}"
              "catch(e){pre.textContent=t;}"
              "btn.disabled=false;btn.textContent='Run Test';"
            "}).catch(function(e){"
              "pre.textContent='Error: '+e;"
              "btn.disabled=false;btn.textContent='Run Test';"
            "});"
          "}"
          "function testNotify(btn){"
            "btn.disabled=true;btn.textContent='Sending...';"
            "authFetch('/notify-test',{method:'POST'})"
            ".then(function(r){return r.text();})"
            ".then(function(t){"
              "if(t==='NOT_CONFIGURED')btn.textContent='Not configured';"
              "else if(parseInt(t)>=200&&parseInt(t)<300)btn.textContent='Sent! (HTTP '+t+')';"
              "else btn.textContent='Failed (HTTP '+t+')';"
              "setTimeout(function(){btn.disabled=false;btn.textContent='Send Test Notification';},3000);"
            "}).catch(function(){"
              "btn.textContent='Error';"
              "setTimeout(function(){btn.disabled=false;btn.textContent='Send Test Notification';},3000);"
            "});"
          "}"
          "function showTab(id){"
            "document.querySelectorAll('.tab-panel').forEach(function(p){p.classList.remove('on');});"
            "document.querySelectorAll('.tab-btn').forEach(function(b){b.classList.remove('on');});"
            "document.getElementById('tab-'+id).classList.add('on');"
            "document.querySelector('[data-tab='+id+']').classList.add('on');"
            "localStorage.setItem('pt-tab',id);"
            "if(id==='notify')loadNtfyStats(null);"
            "if(id==='update')checkOta();"
          "}"
          "function updateCats(){"
            "if(document.getElementById('poiNtfy').checked){"
              "document.getElementById('ntfyClasses').value='';"
              "return;"
            "}"
            "var checked=Array.from(document.querySelectorAll('#catList input[data-label]:checked'));"
            "document.getElementById('ntfyClasses').value=checked.map(function(c){return c.value;}).join(',');"
          "}"
          "function updatePoiMode(){"
            "var poi=document.getElementById('poiNtfy').checked;"
            "document.querySelectorAll('#catList input[data-label]').forEach(function(cb){"
              "cb.disabled=poi;"
              "cb.closest('label').style.opacity=poi?'0.4':'1';"
              "cb.closest('label').style.pointerEvents=poi?'none':'';"
            "});"
            "updateCats();"
          "}"
          "document.addEventListener('DOMContentLoaded',function(){"
            "var t=localStorage.getItem('pt-tab');"
            "showTab((!t||t==='wifi')?'general':t);"
            "updateCats();"
            "updatePoiMode();"
            "if(cfgAuthRequired){"
              "if(!_getTok()){"
                "document.body.classList.add('pre-auth');"
                "showAuthOverlay();"
              "}else{"
                "var lb=document.getElementById('logoutBtn');if(lb)lb.style.display='block';"
              "}"
            "}"
          "});"
          "refresh();setInterval(refresh,5000);"
          "function toggleDark(){"
            "var d=document.body.classList.toggle('dark');"
            "localStorage.setItem('pt-dark',d?'1':'0');"
            "document.getElementById('dmBtn').innerHTML=d?'<i class=\"fa-solid fa-sun\"></i>':'<i class=\"fa-solid fa-moon\"></i>';"
          "}"
          "document.addEventListener('DOMContentLoaded',function(){"
            "var stored=localStorage.getItem('pt-dark');"
            "var sysDark=window.matchMedia&&window.matchMedia('(prefers-color-scheme:dark)').matches;"
            "if(stored!==null?stored==='1':sysDark){"
              "document.body.classList.add('dark');"
              "document.getElementById('dmBtn').innerHTML='<i class=\"fa-solid fa-sun\"></i>';"
            "}"
          "});"
          "function checkOta(){"
            "var btn=document.getElementById('otaCheckBtn');"
            "btn.disabled=true;btn.textContent='Checking...';"
            "authFetch('/ota-check')"
            ".then(function(r){return r.json();})"
            ".then(function(d){"
              "document.getElementById('otaCurrent').textContent=d.current;"
              "document.getElementById('otaLatest').textContent=d.latest||'-';"
              "document.getElementById('otaStatus').textContent=d.status;"
              "var ub=document.getElementById('otaUpdateBtn');"
              "ub.style.display=d.hasUpdate?'block':'none';"
              "btn.disabled=false;btn.textContent='Check for Updates';"
            "}).catch(function(){"
              "document.getElementById('otaStatus').textContent='Request failed';"
              "btn.disabled=false;btn.textContent='Check for Updates';"
            "});"
          "}"
          "var _confirmCb=null;"
          "function showConfirm(icon,title,msg,btnLabel,btnColor,cb){"
            "document.getElementById('confirmIcon').innerHTML=icon;"
            "document.getElementById('confirmTitle').textContent=title;"
            "document.getElementById('confirmMsg').textContent=msg;"
            "var b=document.getElementById('confirmBtn');"
            "b.textContent=btnLabel;b.style.background=btnColor;"
            "_confirmCb=cb;"
            "document.getElementById('confirmModal').classList.add('open');"
          "}"
          "function hideConfirm(){"
            "document.getElementById('confirmModal').classList.remove('open');"
            "_confirmCb=null;"
          "}"
          "function doConfirm(){"
            "document.getElementById('confirmModal').classList.remove('open');"
            "if(_confirmCb){var cb=_confirmCb;_confirmCb=null;cb();}"
          "}"
          "function doOtaUpdate(){"
            "var ver=document.getElementById('otaLatest').textContent;"
            "showConfirm("
              "'<i class=\"fa-solid fa-cloud-arrow-down\" style=\"color:#1976D2\"></i>',"
              "'Install '+ver+'?',"
              "'The device will download and install the update\\u2014this takes about 30 seconds\\u2014then reboot.',"
              "'Update Now','#1976D2',"
              "function(){"
                "document.getElementById('otaUpdateBtn').disabled=true;"
                "document.getElementById('otaModalVersion').textContent=ver;"
                "document.getElementById('otaProgress').style.display='';"
                "document.getElementById('otaSuccess').style.display='none';"
                "document.getElementById('otaError').style.display='none';"
                "document.getElementById('otaModal').classList.add('open');"
                "authFetch('/ota-update',{method:'POST'})"
                ".then(function(r){return r.json();})"
                ".then(function(d){"
                  "document.getElementById('otaProgress').style.display='none';"
                  "if(d.ok){"
                    "document.getElementById('otaSuccess').style.display='';"
                    "pollReconnect(null);"
                  "}else{"
                    "document.getElementById('otaError').style.display='';"
                    "document.getElementById('otaErrMsg').textContent=d.msg||'Unknown error';"
                    "document.getElementById('otaUpdateBtn').disabled=false;"
                  "}"
                "})"
                ".catch(function(e){"
                  "document.getElementById('otaProgress').style.display='none';"
                  "document.getElementById('otaError').style.display='';"
                  "document.getElementById('otaErrMsg').textContent='Request failed: '+e;"
                  "document.getElementById('otaUpdateBtn').disabled=false;"
                "});"
              "}"
            ");"
          "}"
          "function pollReconnect(warnId){"
            "setTimeout(function(){"
              "if(warnId)setTimeout(function(){"
                "var w=document.getElementById(warnId);if(w)w.style.display='block';"
              "},26000);"
              "(function p(){"
                "fetch('/').then(function(r){"
                  "if(r.ok)window.location.href='/';"
                  "else setTimeout(p,1000);"
                "}).catch(function(){setTimeout(p,1000);});"
              "})();"
            "},4000);"
          "}"
          "function doSave(){"
            "var reqPwd=document.getElementById('reqWebPass')&&document.getElementById('reqWebPass').checked;"
            "var newPwd=document.querySelector('[name=webPass]')&&document.querySelector('[name=webPass]').value;"
            "var hasPwd=document.getElementById('hasPwd')&&document.getElementById('hasPwd').value==='1';"
            "if(reqPwd&&!newPwd&&!hasPwd){alert('Enter a password before enabling password protection.');return;}"
            "var data=new FormData(document.querySelector('form[action=\"/save\"]'));"
            "authFetch('/save',{method:'POST',body:data})"
            ".then(function(){"
              "document.getElementById('saveModal').classList.add('open');"
              "pollReconnect('saveWifiWarn');"
            "})"
            ".catch(function(){pollReconnect('saveWifiWarn');});"
          "}"
          "function doClear(){"
            "document.getElementById('clearConfirm').style.display='';"
            "document.getElementById('clearProgress').style.display='none';"
            "document.getElementById('clearModal').classList.add('open');"
          "}"
          "function doClearCancel(){"
            "document.getElementById('clearModal').classList.remove('open');"
          "}"
          "function doClearConfirm(){"
            "document.getElementById('clearConfirm').style.display='none';"
            "document.getElementById('clearProgress').style.display='';"
            "authFetch('/clear',{method:'POST'})"
            ".then(function(){pollReconnect('clearWifiWarn');})"
            ".catch(function(){pollReconnect('clearWifiWarn');});"
          "}"
          "function toggleFullscreen(){"
            "var mapOn=document.getElementById('lvtab-map')&&document.getElementById('lvtab-map').classList.contains('on');"
            "var el=document.getElementById(mapOn?'liveMap':'liveWrap');"
            "if(!document.fullscreenElement){el.requestFullscreen().catch(function(){});}"
            "else{document.exitFullscreen();}"
          "}"
          "document.addEventListener('fullscreenchange',function(){"
            "var btn=document.getElementById('fsBtn');"
            "if(btn)btn.innerHTML=document.fullscreenElement?"
              "'<i class=\"fa-solid fa-compress\"></i>':'<i class=\"fa-solid fa-expand\"></i>';"
            "if(_lmap&&_rCircle)setTimeout(function(){"
              "_lmap.invalidateSize();"
              "_lmap.fitBounds(_rCircle.getBounds(),{padding:[20,20]});"
            "},100);"
          "});"
          "</script>"
          "</head><body>"
          "<button id='logoutBtn' onclick='doLogout()' title='Log out'>"
          "<i class='fa-solid fa-right-from-bracket'></i></button>"
          "<button id='dmBtn' onclick='toggleDark()' title='Toggle dark mode'><i class='fa-solid fa-moon'></i></button>"
          "<h2><i class='fa-solid fa-plane' style='margin-right:8px'></i>PlaneTracker</h2>"
          "<div class='layout'>"
          "<div class='form-col'>"
          "<form method='POST' action='/save'>");

    // Inject runtime config coordinates for the live map JS
    {
        char cfgVars[128];
        snprintf(cfgVars, sizeof(cfgVars),
            "<script>var cfgLat=%.6f,cfgLon=%.6f,cfgRadiusM=%d,cfgAuthRequired=%s;</script>",
            _cfg.latitude, _cfg.longitude, (int)(_cfg.radius * 1852.0f),
            (_cfg.requireWebPassword && _cfg.hasWebPassword()) ? "true" : "false");
        html += cfgVars;
    }

    // ── Tab header ────────────────────────────────────────────────────────────
    html += "<div class='card'>"
            "<div class='tabs' style='border:none;border-radius:0;margin:0'>"
            "<div class='tab-hdr'>"
            "<button type='button' class='tab-btn' data-tab='general' onclick='showTab(\"general\")'>"
            "<i class='fa-solid fa-gear'></i><br>General</button>"
            "<button type='button' class='tab-btn' data-tab='detect'  onclick='showTab(\"detect\")'>"
            "<i class='fa-solid fa-satellite-dish'></i><br>Detection</button>"
            "<button type='button' class='tab-btn' data-tab='notify'  onclick='showTab(\"notify\")'>"
            "<i class='fa-solid fa-bell'></i><br>Notifications</button>"
            "<button type='button' class='tab-btn' data-tab='apitest' onclick='showTab(\"apitest\")'>"
            "<i class='fa-solid fa-terminal'></i><br>API Test</button>"
            "<button type='button' class='tab-btn' data-tab='update' onclick='showTab(\"update\")'>"
            "<i class='fa-solid fa-cloud-arrow-down'></i><br>Update</button>"
            "</div>";

    // ── General tab ───────────────────────────────────────────────────────────
    html += "<div class='tab-panel' id='tab-general'>";

    html += "<div class='sec'>"
            "<i class='fa-solid fa-wifi' style='margin-right:5px'></i>WiFi</div>";
    html += "<label>SSID</label>"
            "<input name='ssid' value='" + htmlEscape(_cfg.ssid) + "'>";
    html += "<label>Password</label>"
            "<input name='pass' type='password' placeholder='leave blank to keep current'>";

    {
        String reqChk    = _cfg.requireWebPassword ? " checked" : "";
        String hasPwdVal = _cfg.hasWebPassword() ? "1" : "0";
        String pholder   = _cfg.hasWebPassword() ? "leave blank to keep current" : "set a password";
        html += "<div class='sec'>"
                "<i class='fa-solid fa-lock' style='margin-right:5px'></i>Web UI Security</div>";
        html += "<label>Password</label>"
                "<input name='webPass' type='password' placeholder='" + pholder + "'>";
        html += "<input type='hidden' id='hasPwd' value='" + hasPwdVal + "'>";
        html += "<label class='chk-item' style='margin-top:8px;padding:8px 0'>"
                "<input type='checkbox' id='reqWebPass' name='reqWebPass' value='1'" + reqChk + ">"
                " Require password to access web interface"
                "</label>";
    }
    html += "</div>";

    // ── Detection tab ─────────────────────────────────────────────────────────
    html += "<div class='tab-panel' id='tab-detect'>";
    html += "<label>Latitude</label>"
            "<input name='lat' value='" + String(_cfg.latitude, 6) + "'>";
    html += "<label>Longitude</label>"
            "<input name='lon' value='" + String(_cfg.longitude, 6) + "'>";
    html += "<button type='button' onclick='openMap()' "
            "style='margin-top:8px;padding:6px 14px;background:#37474F;color:#fff;"
            "border:none;border-radius:4px;cursor:pointer;font-size:.9em'>"
            "<i class='fa-solid fa-location-dot' style='margin-right:5px'></i>Pick on map</button>";
    html += "<label>Search Radius (nautical miles)</label>"
            "<input name='radius' value='" + String(_cfg.radius) + "'>";
    html += "<label>Scan Interval <small style='font-weight:normal'>(seconds &mdash; minimum " +
            String(Config::kMinPollIntervalMs / 1000) + "s)</small></label>"
            "<input name='poll' type='number' min='" + String(Config::kMinPollIntervalMs / 1000) +
            "' value='" + String(_cfg.pollIntervalMs / 1000) + "'>";
    {
        String poiChk = _cfg.poiEnabled ? " checked" : "";
        html += "<label style='margin-top:14px'>POI Aircraft Types "
                "<small style='font-weight:normal'>(ICAO type codes, comma-separated &mdash; e.g. B737,F16,C172)</small></label>"
                "<input name='poiTypes' placeholder='e.g. B737,F16,C172' value='" + htmlEscape(_cfg.poiTypes) + "'>";
        html += "<label class='chk-item' style='margin-top:6px;padding:8px 0'>"
                "<input type='checkbox' name='poiEnabled' value='1'" + poiChk + ">"
                " Enable POI Filter &mdash; show only listed types on device, map &amp; radar"
                "</label>";
    }
    html += "</div>";

    // ── Notifications tab ─────────────────────────────────────────────────────
    html += "<div class='tab-panel' id='tab-notify'>";
    html += "<p style='margin:0 0 10px;font-size:.85em;color:#666'>"
            "<i class='fa-solid fa-circle-info' style='margin-right:4px'></i>"
            "Need an account? <a href='https://ntfy.sh' target='_blank'>"
            "<i class='fa-solid fa-arrow-up-right-from-square' style='margin-right:3px'></i>"
            "Create a free ntfy account</a></p>";
    html += "<label>ntfy Token</label>"
            "<input name='ntfyToken' type='password' placeholder='leave blank to keep current'>";
    html += "<label>ntfy Topic</label>"
            "<input name='ntfyTopic' value='" + htmlEscape(_cfg.notifyTopic) + "'>";
    {
        // Pre-check boxes based on saved filter; empty filter = all (nothing checked)
        const char* f = _cfg.notifyClassFilter;
        bool hasFilter = strlen(f) > 0;
        String milChk  = (hasFilter && strstr(f, "MIL"))    ? " checked" : "";
        String medChk  = (hasFilter && strstr(f, "MEDVAC")) ? " checked" : "";
        String commChk = (hasFilter && strstr(f, "COMM"))   ? " checked" : "";
        String privChk = (hasFilter && strstr(f, "PRIV"))   ? " checked" : "";

        String updChk   = _cfg.notifyUpdates         ? " checked" : "";
        String emergChk = _cfg.notifyEmergencySquawk  ? " checked" : "";
        String poiChk   = _cfg.notifyPoi              ? " checked" : "";
        html += "<label>ntfy Notification Categories</label>"
                "<div id='catList' style='margin-top:4px'>"
                "<label class='chk-item' style='border-bottom:1px solid #eee;margin-bottom:2px;padding-bottom:10px'>"
                "<input type='checkbox' id='poiNtfy' name='ntfyPoi' value='1'" + poiChk + " onchange='updatePoiMode()'>"
                " POI Aircraft"
                "<span title='POI Aircraft notifications override Military, Medevac, Commercial, and Private filters."
                " Emergency Squawk and Firmware Update notifications are unaffected.'"
                " style='cursor:help;margin-left:6px;color:#1976D2;font-size:.9em'>"
                "<i class='fa-solid fa-circle-info'></i></span>"
                "</label>"
                "<label class='chk-item'><input type='checkbox' value='MIL'    data-label='Military'"    + milChk  + " onchange='updateCats()'> Military</label>"
                "<label class='chk-item'><input type='checkbox' value='MEDVAC' data-label='Medevac'"     + medChk  + " onchange='updateCats()'> Medevac</label>"
                "<label class='chk-item'><input type='checkbox' value='COMM'   data-label='Commercial'"  + commChk + " onchange='updateCats()'> Commercial</label>"
                "<label class='chk-item'><input type='checkbox' value='PRIV'   data-label='Private'"     + privChk + " onchange='updateCats()'> Private</label>"
                "<label class='chk-item'><input type='checkbox' name='ntfyEmergSquawk' value='1'"        + emergChk + "> Emergency Squawk</label>"
                "<label class='chk-item'><input type='checkbox' name='ntfyUpdates' value='1'"            + updChk   + "> Firmware Updates</label>"
                "</div>"
                "<input type='hidden' name='ntfyClasses' id='ntfyClasses' value='" + htmlEscape(f) + "'>";
    }
    html += "<button type='button' onclick='testNotify(this)' "
            "style='margin-top:10px;padding:8px 14px;background:#37474F;color:#fff;"
            "border:none;border-radius:4px;cursor:pointer;font-size:.9em;width:100%'>"
            "<i class='fa-solid fa-paper-plane' style='margin-right:6px'></i>Send Test Notification</button>";
    html += "<div style='margin-top:14px;padding:10px 12px;border:1px solid #ddd;"
            "border-radius:4px;font-size:.85em'>"
            "<div style='display:flex;justify-content:space-between;align-items:center;"
            "margin-bottom:6px'>"
            "<span style='font-weight:600;color:#555'>"
            "<i class='fa-solid fa-chart-simple' style='margin-right:5px'></i>Account Usage</span>"
            "<button type='button' onclick='loadNtfyStats(this)' "
            "style='padding:3px 9px;background:#37474F;color:#fff;border:none;"
            "border-radius:3px;cursor:pointer;font-size:.82em'>Refresh</button>"
            "</div>"
            "<div id='ntfyUsage' style='color:#666'>Loading&hellip;</div>"
            "</div>";
    html += "</div>";

    // ── API Test tab ──────────────────────────────────────────────────────────
    html += "<div class='tab-panel' id='tab-apitest'>";
    html += "<p style='margin:0 0 10px;font-size:.85em;color:#666'>"
            "<i class='fa-solid fa-circle-info' style='margin-right:4px'></i>"
            "Runs a live query using the lat/lon/radius currently in the Detection tab.</p>";
    html += "<button type='button' id='apiBtn' onclick='runApiTest()' "
            "style='padding:8px 14px;background:#37474F;color:#fff;"
            "border:none;border-radius:4px;cursor:pointer;font-size:.9em;width:100%'>"
            "<i class='fa-solid fa-play' style='margin-right:6px'></i>Run Test</button>";
    html += "<pre id='apiOut'>Press Run Test to execute a query.</pre>";
    html += "</div>";

    // ── Update tab ────────────────────────────────────────────────────────────
    html += "<div class='tab-panel' id='tab-update'>";
    html += "<p style='margin:0 0 10px;font-size:.85em;color:#666'>"
            "<i class='fa-solid fa-circle-info' style='margin-right:4px'></i>"
            "Updates are checked automatically every 24 hours. "
            "A notification is sent via ntfy if an update is found.</p>";
    html += "<label>Current version</label>"
            "<div id='otaCurrent' class='ver-box'>" + String(OtaUpdater::currentVersion()) + "</div>";
    html += "<label style='margin-top:10px'>Latest version</label>"
            "<div id='otaLatest' class='ver-box'>-</div>";
    html += "<div id='otaStatus' style='margin-top:8px;font-size:.85em;color:#666'>"
            + _ota.statusMessage() + "</div>";
    html += "<button type='button' id='otaCheckBtn' onclick='checkOta()' "
            "style='margin-top:10px;padding:8px 14px;background:#37474F;color:#fff;"
            "border:none;border-radius:4px;cursor:pointer;font-size:.9em;width:100%'>"
            "<i class='fa-solid fa-rotate' style='margin-right:6px'></i>Check for Updates</button>";
    html += "<button type='button' id='otaUpdateBtn' onclick='doOtaUpdate()' "
            "style='display:none;margin-top:8px;padding:8px 14px;background:#1976D2;color:#fff;"
            "border:none;border-radius:4px;cursor:pointer;font-size:.9em;width:100%'>"
            "<i class='fa-solid fa-cloud-arrow-down' style='margin-right:6px'></i>Update Now</button>";
    html += "</div>";

    html += "</div></div>"; // end tabs + card

    // ── Save / Clear ──────────────────────────────────────────────────────────
    html += "<button class='btn' type='button' onclick='doSave()'>"
            "<i class='fa-solid fa-floppy-disk' style='margin-right:6px'></i>Save &amp; Reboot</button>"
            "</form>"
            "<button class='btn' type='button' style='background:#D32F2F;margin-top:10px' onclick='doClear()'>"
            "<i class='fa-solid fa-trash' style='margin-right:6px'></i>Clear All Settings</button>"
            "</div>"; // end form-col

    // Right column - live screen preview with tabs
    html += "<div class='preview-col'>"
            "<div class='card'>"
            "<div class='card-hdr' style='justify-content:space-between'>"
            "<span><i class='fa-solid fa-display'></i>&nbsp;Live Screen</span>"
            "<button id='fsBtn' onclick='toggleFullscreen()' "
            "title='Fullscreen' style='background:none;border:none;cursor:pointer;color:inherit;"
            "font-size:.9em;padding:0 2px'><i class='fa-solid fa-expand'></i></button>"
            "</div>"
            "<div class='lv-tab-hdr'>"
            "<button class='lv-tab-btn on' data-lvtab='live' onclick='showLiveTab(\"live\")'>"
            "<i class='fa-solid fa-display'></i>&nbsp;Live</button>"
            "<button class='lv-tab-btn' data-lvtab='map' onclick='showLiveTab(\"map\")'>"
            "<i class='fa-solid fa-map-location-dot'></i>&nbsp;Map</button>"
            "<button class='lv-tab-btn' data-lvtab='hist' onclick='showLiveTab(\"hist\")'>"
            "<i class='fa-solid fa-list'></i>&nbsp;History</button>"
            "</div>"
            "<div class='lv-tab-panel on' id='lvtab-live' style='padding:14px;text-align:center'>"
            "<div id='liveWrap'>"
            "<div id='scrWrap'></div>"
            "<div class='ctrl' style='margin-top:10px'>"
            "<button class='cb' data-s='scan'    onclick='ctrl(\"scan\")'><i class='fa-solid fa-satellite-dish'></i><br>Scan</button>"
            "<button class='cb' data-s='history' onclick='ctrl(\"history\")'><i class='fa-solid fa-clock-rotate-left'></i><br>History</button>"
            "<button class='cb' data-s='summary' onclick='ctrl(\"summary\")'><i class='fa-solid fa-chart-bar'></i><br>Summary</button>"
            "<button class='cb' data-s='radar'   onclick='ctrl(\"radar\")'><i class='fa-solid fa-circle-dot'></i><br>Radar</button>"
            "<button class='cb' data-s='debug'   onclick='ctrl(\"debug\")'><i class='fa-solid fa-bug'></i><br>Debug</button>"
            "</div>"
            "<div style='margin-top:6px'>"
            "<button onclick='clearSummary()' style='background:none;border:none;color:#888;"
            "font-size:.78em;cursor:pointer;padding:2px 6px'>"
            "<i class='fa-solid fa-eraser' style='margin-right:4px'></i>Clear Summary</button>"
            "</div>"
            "</div>"
            "</div>"
            "<div class='lv-tab-panel' id='lvtab-map' style='padding:0'>"
            "<div id='liveMap' style='height:290px;width:100%'></div>"
            "</div>"
            "<div class='lv-tab-panel' id='lvtab-hist' style='padding:8px'>"
            "<div id='histPanel' style='font-family:monospace;font-size:11px'>"
            "<p style='color:#888;text-align:center;margin:20px 0'>No history yet.</p>"
            "</div>"
            "</div>"
            "</div>"
            "</div>"; // end preview-col

    // OTA update modal - progress / success / error states
    html += "<div class='modal' id='otaModal' style='z-index:2000'>"
            "<div class='modal-box'>"
            // progress state
            "<div id='otaProgress'>"
            "<div style='font-size:2.5em;margin-bottom:12px;color:#1976D2'><i class='fa-solid fa-arrows-rotate fa-spin'></i></div>"
            "<div style='font-size:1.1em;font-weight:700;margin-bottom:8px'>Updating Firmware</div>"
            "<div id='otaModalVersion' style='font-family:monospace;font-size:.9em;margin-bottom:16px'></div>"
            "<div style='color:#c00;font-weight:600;margin-bottom:6px'>"
            "<i class='fa-solid fa-triangle-exclamation' style='margin-right:5px'></i>"
            "Do not unplug the device</div>"
            "<div class='modal-hint'>This takes about 30 seconds.</div>"
            "</div>"
            // success state
            "<div id='otaSuccess' style='display:none'>"
            "<div style='font-size:2.5em;margin-bottom:12px;color:#388E3C'><i class='fa-solid fa-circle-check'></i></div>"
            "<div style='font-size:1.1em;font-weight:700;margin-bottom:8px'>Update Complete</div>"
            "<div class='modal-hint'>Rebooting&hellip; returning to settings when back online.</div>"
            "<div style='color:#c00;font-weight:600;margin-bottom:6px'>"
            "<i class='fa-solid fa-triangle-exclamation' style='margin-right:5px'></i>"
            "Do not unplug the device</div>"
            "</div>"
            // error state
            "<div id='otaError' style='display:none'>"
            "<div style='font-size:2.5em;margin-bottom:12px;color:#D32F2F'><i class='fa-solid fa-circle-xmark'></i></div>"
            "<div style='font-size:1.1em;font-weight:700;margin-bottom:8px'>Update Failed</div>"
            "<div id='otaErrMsg' class='modal-hint' style='margin-bottom:20px'></div>"
            "<button onclick=\"document.getElementById('otaModal').classList.remove('open')\" "
            "class='modal-cancel' style='width:100%'>Dismiss</button>"
            "</div>"
            "</div>"
            "</div>";

    // Save modal
    html += "<div class='modal' id='saveModal' style='z-index:2000'>"
            "<div class='modal-box'>"
            "<div style='font-size:2.5em;margin-bottom:12px;color:#37474F'><i class='fa-solid fa-floppy-disk'></i></div>"
            "<div style='font-size:1.1em;font-weight:700;margin-bottom:8px'>Saving Settings</div>"
            "<div class='modal-hint'>Rebooting&hellip; returning to settings when back online.</div>"
            "<div style='color:#c00;font-weight:600;margin-bottom:6px'>"
            "<i class='fa-solid fa-triangle-exclamation' style='margin-right:5px'></i>"
            "Do not unplug the device</div>"
            "<div id='saveWifiWarn' class='wifi-warn'>"
            "<strong>Taking too long?</strong><br>"
            "If you changed your WiFi credentials the device may have started its own "
            "<strong>PlaneTracker</strong> hotspot. Connect to it and visit "
            "<strong>192.168.4.1</strong> to reconfigure."
            "</div>"
            "</div>"
            "</div>";

    // Clear confirmation / progress modal
    html += "<div class='modal' id='clearModal' style='z-index:2000'>"
            "<div class='modal-box'>"
            "<div id='clearConfirm'>"
            "<div style='font-size:2.5em;margin-bottom:12px;color:#D32F2F'><i class='fa-solid fa-trash'></i></div>"
            "<div style='font-size:1.1em;font-weight:700;margin-bottom:8px'>Clear All Settings?</div>"
            "<div class='modal-hint' style='margin-bottom:20px'>"
            "Resets all saved preferences to factory defaults. This cannot be undone.</div>"
            "<div style='display:flex;gap:10px'>"
            "<button onclick='doClearCancel()' class='modal-cancel'>Cancel</button>"
            "<button onclick='doClearConfirm()' "
            "style='flex:1;padding:10px;background:#D32F2F;color:#fff;border:none;"
            "border-radius:4px;cursor:pointer;font-size:.9em'>"
            "<i class='fa-solid fa-trash' style='margin-right:5px'></i>Clear &amp; Reboot</button>"
            "</div>"
            "</div>"
            "<div id='clearProgress' style='display:none'>"
            "<div style='font-size:2.5em;margin-bottom:12px;color:#D32F2F'><i class='fa-solid fa-trash'></i></div>"
            "<div style='font-size:1.1em;font-weight:700;margin-bottom:8px'>Clearing Settings</div>"
            "<div class='modal-hint'>Rebooting&hellip; returning to settings when back online.</div>"
            "<div style='color:#c00;font-weight:600;margin-bottom:6px'>"
            "<i class='fa-solid fa-triangle-exclamation' style='margin-right:5px'></i>"
            "Do not unplug the device</div>"
            "<div id='clearWifiWarn' class='wifi-warn'>"
            "<strong>Taking too long?</strong><br>"
            "The device may have started its own <strong>PlaneTracker</strong> hotspot. "
            "Connect to it and visit <strong>192.168.4.1</strong> to reconfigure."
            "</div>"
            "</div>"
            "</div>"
            "</div>";

    // Generic confirm modal
    html += "<div class='modal' id='confirmModal' style='z-index:2001'>"
            "<div class='modal-box'>"
            "<div id='confirmIcon' style='font-size:2em;margin-bottom:12px'></div>"
            "<div id='confirmTitle' style='font-size:1.1em;font-weight:700;margin-bottom:8px'></div>"
            "<div id='confirmMsg' class='modal-hint' style='margin-bottom:20px'></div>"
            "<div style='display:flex;gap:10px'>"
            "<button onclick='hideConfirm()' class='modal-cancel'>Cancel</button>"
            "<button id='confirmBtn' onclick='doConfirm()' "
            "style='flex:1;padding:10px;border:none;border-radius:4px;"
            "cursor:pointer;font-size:.9em;color:#fff'></button>"
            "</div>"
            "</div>"
            "</div>";

    // Map picker modal
    html += "<div class='modal' id='mapModal'>"
            "<div id='mapWrap'>"
            "<div class='map-hdr'>"
            "<strong>Pick your location</strong>"
            "<button type='button' onclick='closeMap()' "
            "style='background:#555;color:#fff;border:none;padding:4px 10px;"
            "border-radius:4px;cursor:pointer'><i class='fa-solid fa-xmark' style='margin-right:4px'></i>Close</button>"
            "</div>"
            "<p class='map-hint'>Click anywhere on the map to set your latitude &amp; longitude.</p>"
            "<div id='map'></div>"
            "</div>"
            "</div>";

    html += "</div>"; // end layout

    html += "<div style='text-align:center;margin-top:24px;padding-bottom:16px;font-size:.85em;color:#888'>"
            "<a href='https://github.com/JTCozart/atom-plane-tracker' target='_blank' "
            "style='color:inherit;text-decoration:none;margin-right:20px'>"
            "<i class='fa-brands fa-github' style='font-size:1.4em;margin-right:6px;vertical-align:middle'></i>"
            "JTCozart/atom-plane-tracker</a>"
            "<a href='https://linktr.ee/jtczrt' target='_blank' "
            "style='color:inherit;text-decoration:none'>"
            "<i class='fa-solid fa-tree' style='font-size:1.4em;margin-right:6px;vertical-align:middle'></i>"
            "linktr.ee/jtczrt</a>"
            "</div>";
    // Auth overlay (visible only when requireWebPassword=true and session is missing/expired)
    html += "<div class='auth-overlay' id='authOverlay'>"
            "<div class='auth-box'>"
            "<h3><i class='fa-solid fa-lock' style='margin-right:8px'></i>PlaneTracker</h3>"
            // Login view — hidden when switched to forgot-password PIN view
            "<div id='authLoginView'>"
            "<label>Password</label>"
            "<input type='password' id='authPwd' autocomplete='current-password' "
            "onkeydown='if(event.key===\"Enter\")doLogin()'>"
            "<div class='auth-err' id='authErr'></div>"
            // 2FA lockout banner + code input (hidden until 5 failed attempts)
            "<div id='authLockout' style='display:none;margin-top:10px;padding:8px 10px;"
            "background:#FFF8E1;border:1px solid #FFB300;border-radius:4px;"
            "font-size:.83em;color:#5D4037'>"
            "<i class='fa-solid fa-shield-halved' style='margin-right:5px'></i>"
            "Check your device for a security code.</div>"
            "<input type='text' id='authCode' maxlength='4' inputmode='numeric' "
            "autocomplete='off' placeholder='Device security code' "
            "style='display:none;margin-top:6px'>"
            "<button class='btn' style='margin-top:12px' onclick='doLogin()'>Log In</button>"
            "<div style='text-align:center;margin-top:10px;font-size:.9em'>"
            "<a style='cursor:pointer;color:#1976D2' onclick='showForgot()'>Forgot password?</a>"
            "</div>"
            "</div>"
            // Forgot-password PIN view (hidden until showForgot() is called)
            "<div class='pin-sect' id='authPinSect'>"
            "<p style='font-size:.85em;color:#666;margin:0 0 10px;text-align:center'>"
            "A reset PIN is now shown on your device screen.</p>"
            "<label>Device PIN</label>"
            "<input type='text' id='authPin' maxlength='4' inputmode='numeric' "
            "autocomplete='off'>"
            "<label style='margin-top:10px'>New password</label>"
            "<input type='password' id='authNewPwd' autocomplete='new-password'>"
            "<div class='auth-err' id='authPinErr'></div>"
            "<button class='btn' style='margin-top:10px' onclick='doReset()'>Reset Password</button>"
            "<button class='btn' style='background:#666;margin-top:6px' "
            "onclick='doCancelReset()'>Cancel</button>"
            "</div>"
            "</div>"
            "</div>";

    html += "</body></html>";

    _server.send(200, "text/html", html);
}

void WebUI::handleSave() {
    if (!checkAuth()) return;
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
        uint32_t pollValue = (uint32_t)_server.arg("poll").toInt() * 1000;  // form sends seconds
        if (pollValue < Config::kMinPollIntervalMs) pollValue = Config::kMinPollIntervalMs;
        prefs.putUInt("pollMs", pollValue);
    }
    if (_server.hasArg("ntfyToken") && _server.arg("ntfyToken").length() > 0)
        prefs.putString("ntfyToken",   _server.arg("ntfyToken"));
    if (_server.hasArg("ntfyTopic"))
        prefs.putString("ntfyTopic",   _server.arg("ntfyTopic"));
    if (_server.hasArg("ntfyClasses"))
        prefs.putString("ntfyClasses", _server.hasArg("ntfyPoi") ? "" : _server.arg("ntfyClasses"));
    prefs.putBool("ntfyUpdates",    _server.hasArg("ntfyUpdates"));
    prefs.putBool("ntfyEmergSquawk", _server.hasArg("ntfyEmergSquawk"));
    prefs.putBool("ntfyPoi",        _server.hasArg("ntfyPoi"));
    if (_server.hasArg("poiTypes"))
        prefs.putString("poiTypes", _server.arg("poiTypes"));
    prefs.putBool("poiEnabled",     _server.hasArg("poiEnabled"));

    // Web UI password
    {
        String newPass  = _server.arg("webPass");
        bool   reqPwd   = _server.hasArg("reqWebPass");
        bool   hasNewPass = newPass.length() > 0;

        if (hasNewPass) {
            String salt = generateSalt();
            String hash = computePasswordHash(newPass, salt);
            prefs.putString("webPassHash", hash);
            prefs.putString("webSalt",     salt);
            strncpy(_cfg.webPasswordHash, hash.c_str(), sizeof(_cfg.webPasswordHash) - 1);
            strncpy(_cfg.webPasswordSalt, salt.c_str(), sizeof(_cfg.webPasswordSalt) - 1);
        }
        bool hashExists = hasNewPass || prefs.getString("webPassHash", "").length() > 0;
        bool enable = reqPwd && hashExists;
        prefs.putBool("reqWebPass", enable);
        _cfg.requireWebPassword = enable;
    }

    prefs.end();

    _server.send(200, "application/json", "{\"ok\":true}");
    delay(1500);
    ESP.restart();
}

void WebUI::handleControl() {
    if (!checkAuth()) return;
    if (!_server.hasArg("screen")) { _server.send(200, "text/plain", "OK"); return; }

    String name = _server.arg("screen");
    if (name == "history" && _screenController.current() == ScreenMode::History
                          && _store.historyCount() > 0) {
        // Already on History — page to the next entry, wrapping at the end.
        int next = _store.historyIndex() + 1;
        _store.setHistoryIndex(next < _store.historyCount() ? next : 0);
        _screenController.markChanged();
    } else {
        if (name == "history") _store.setHistoryIndex(0);
        _screenController.setModeFromString(name);
    }
    _server.send(200, "text/plain", "OK");
}

void WebUI::handleClear() {
    if (!checkAuth()) return;
    Preferences prefs;
    prefs.begin("plantracker", false);
    prefs.clear();  // Erase all keys in this namespace
    prefs.end();

    _server.send(200, "application/json", "{\"ok\":true}");
    delay(1500);
    ESP.restart();
}

void WebUI::handleNotifyTest() {
    if (!checkAuth()) return;
    if (strlen(_cfg.notifyToken) == 0 || strlen(_cfg.notifyTopic) == 0) {
        _server.send(200, "text/plain", "NOT_CONFIGURED");
        return;
    }
    int code = Notifier::sendTestHttp(_cfg);
    _server.send(200, "text/plain", String(code));
}

void WebUI::handleApiTest() {
    if (!checkAuth()) return;
    double lat    = _server.hasArg("lat")    ? _server.arg("lat").toDouble()   : _cfg.latitude;
    double lon    = _server.hasArg("lon")    ? _server.arg("lon").toDouble()   : _cfg.longitude;
    float  radius = _server.hasArg("radius") ? _server.arg("radius").toFloat() : _cfg.radius;

    char url[128];
    snprintf(url, sizeof(url),
             "https://api.adsb.lol/v2/lat/%.6f/lon/%.6f/dist/%.1f", lat, lon, radius);

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, url);
    http.setTimeout(8000);
    int code = http.GET();

    if (code != 200) {
        http.end();
        _server.send(200, "application/json",
                     "{\"error\":\"HTTP " + String(code) + "\",\"url\":\"" + url + "\"}");
        return;
    }

    String payload = http.getString();
    http.end();
    _server.send(200, "application/json", payload);
}

void WebUI::handleNtfyStats() {
    if (!checkAuth()) return;
    if (strlen(_cfg.notifyToken) == 0) {
        _server.send(200, "application/json", "{\"error\":\"not_configured\"}");
        return;
    }

    WiFiClientSecure client;
    client.setInsecure();
    HTTPClient http;
    http.begin(client, "https://ntfy.sh/v1/account");
    http.addHeader("Authorization", String("Bearer ") + _cfg.notifyToken);
    int code = http.GET();

    if (code != 200) {
        http.end();
        _server.send(200, "application/json",
                     "{\"error\":\"http_" + String(code) + "\"}");
        return;
    }

    JsonDocument filter;
    filter["stats"]["messages"]           = true;
    filter["stats"]["messages_remaining"] = true;
    filter["limits"]["messages"]          = true;

    String body = http.getString();
    http.end();

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, body,
                                               DeserializationOption::Filter(filter));

    if (err) {
        _server.send(200, "application/json", "{\"error\":\"parse_error\"}");
        return;
    }

    long sent      = doc["stats"]["messages"]           | -1L;
    long remaining = doc["stats"]["messages_remaining"] | -1L;
    long limit     = doc["limits"]["messages"]          | -1L;

    char buf[72];
    snprintf(buf, sizeof(buf), "{\"sent\":%ld,\"remaining\":%ld,\"limit\":%ld}",
             sent, remaining, limit);
    _server.send(200, "application/json", String(buf));
}


String WebUI::buildScreenDiv() {
    ScreenMode mode = _screenController.current();
    String bg = "#000000", fg = "#FFFFFF", inner = "";

    auto buildAcInner = [&](const Aircraft& ac, bool hist, int histIdx, int histTotal) {
        bg = Display::rgb565ToCss(_display.backgroundColorFor(ac.classification));
        fg = Display::rgb565ToCss(_display.foregroundColorFor(ac.classification));

        String cs = ac.callsign.length() ? ac.callsign : "N/A";
        if (cs.length() > 10) cs = cs.substring(0, 10);

        String trackUrl = "https://globe.adsbexchange.com/?icao=" + ac.icao;
        inner += "<div style='font-size:26px;line-height:1.2;margin:4px 0'>"
                 "<a href='" + trackUrl + "' target='_blank' "
                 "style='color:inherit;text-decoration:none;border-bottom:1px dotted'>"
                 + cs + "</a></div>";
        inner += "<div>Type: " + String(ac.type.length() ? ac.type.c_str() : "???") + "</div>";

        char buf[40];
        if (ac.altitude > 0) snprintf(buf, sizeof(buf), "Alt:  %.0f ft", ac.altitude);
        else                  strcpy (buf, "Alt:  ground");
        inner += "<div>" + String(buf) + "</div>";

        if (!hist) {
            int eta = ac.adjustedEta(ac.etaSeconds(_cfg.latitude, _cfg.longitude, _cfg.radius));
            if (eta < 0) {
                inner += "<div>ETA:  --:--</div>";
            } else {
                snprintf(buf, sizeof(buf), "ETA:  %d:%02d", eta / 60, eta % 60);
                inner += "<div>" + String(buf) + "</div>";
            }
        }

        if (ac.squawk.length() > 0) {
            if (ac.isEmergencySquawk()) {
                inner += "<div>SQK: <span class='sqwk-emrg'>" + ac.squawk + "</span></div>";
            } else {
                inner += "<div>SQK:  " + ac.squawk + "</div>";
            }
        }

        if (ac.latitude != 0.0f) {
            float dist = ac.distanceNm(_cfg.latitude, _cfg.longitude);
            const char* compass = Aircraft::compassPoint(ac.bearingDeg(_cfg.latitude, _cfg.longitude));
            const char* arrow = ac.isApproaching(_cfg.latitude, _cfg.longitude) ? " &#x25b2;" : " &#x25bc;";
            snprintf(buf, sizeof(buf), "DST:  %.1f NM %s", dist, compass);
            inner += "<div>" + String(buf) + String(arrow) + "</div>";
        }
        if (ac.groundSpeed > 0) {
            snprintf(buf, sizeof(buf), "SPD:  %.0f kt", ac.groundSpeed);
            inner += "<div>" + String(buf) + "</div>";
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
    } else if (mode == ScreenMode::Scanning && _store.consecutiveFailures() > 0) {
        bg = "#FF0000"; fg = "#000000";
        inner = "<div style='position:absolute;inset:0;display:flex;flex-direction:column;"
                "align-items:center;justify-content:center;font-weight:bold'>"
                "<div style='font-size:32px'>LOST</div>"
                "<div style='font-size:20px'>CONNECTION</div>"
                "<div style='font-size:13px;margin-top:10px'>RETRYING (" +
                String(_store.consecutiveFailures()) + ")</div>"
                "</div>";
    } else if (mode == ScreenMode::Scanning) {
        // Canvas element only - animation is started by startRadar() in the main page JS
        inner = "<canvas id='rc' width='256' height='256' "
                "style='position:absolute;top:0;left:0;width:100%;height:100%'></canvas>";
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
    } else if (mode == ScreenMode::Radar) {
        inner = "<canvas id='radarBc' width='256' height='256' "
                "style='position:absolute;top:0;left:0;width:100%;height:100%'></canvas>";
    } else if (mode == ScreenMode::Debug) {
        const std::vector<String>& lines = _store.apiResponseLines();
        // Wrap text content in a height-limited div so it never bleeds into the bar zone
        inner += "<div style='overflow:hidden;height:216px'>";
        inner += "<div>DBG HTTP:" + String(_store.lastResponseCode()) +
                 " ac:" + String(_store.lastAircraftCount()) + "</div>";
        inner += "<div>IP: " + _ipAddress + "</div>";
        {
            uint32_t up = millis() / 1000;
            char upBuf[16];
            snprintf(upBuf, sizeof(upBuf), "UP: %02u:%02u:%02u",
                     up / 3600, (up % 3600) / 60, up % 60);
            inner += "<div>" + String(upBuf) + "</div>";
        }
        inner += "<div>VER: " + String(OtaUpdater::currentVersion()) + "</div>";
        int shown = min((int)lines.size(), 10);
        for (int i = 0; i < shown; i++) {
            String line = lines[i];
            if (line.length() > 21) line = line.substring(0, 21);
            inner += "<div>" + line + "</div>";
        }
        inner += "</div>"; // end height-limited content wrapper
        // System bars pinned to bottom of debug screen
        auto sysBar = [](const char* label, float used, const char* pct, int bottomPx) -> String {
            const char* col = used > 0.75f ? "#cc0000"
                            : used > 0.50f ? "#b8a800"
                            :                "#00aa00";
            return String("<div style='position:absolute;bottom:") + bottomPx +
                   "px;left:6px;right:6px;display:flex;align-items:center;gap:3px;font-size:10px'>"
                   "<span style='color:#00cc00;width:26px'>" + label + "</span>"
                   "<div style='flex:1;height:4px;background:#282828;border-radius:2px'>"
                   "<div style='width:" + (int)(used * 100) + "%;height:4px;background:" + col + ";border-radius:2px'></div>"
                   "</div>"
                   "<span style='color:#00cc00;width:28px;text-align:right'>" + pct + "</span>"
                   "</div>";
        };
        uint32_t freeH = ESP.getFreeHeap(), totH = ESP.getHeapSize();
        float ramU = 1.0f - (float)freeH / (float)totH;
        char ramP[5]; snprintf(ramP, sizeof(ramP), "%d%%", (int)(ramU * 100));
        inner += sysBar("RAM", ramU, ramP, 14);

        uint32_t stkFree = uxTaskGetStackHighWaterMark(NULL) * 4;
        float stkU = max(0.0f, 1.0f - (float)stkFree / (24.0f * 1024));
        char stkP[5]; snprintf(stkP, sizeof(stkP), "%d%%", (int)(stkU * 100));
        inner += sysBar("STK", stkU, stkP, 2);
    } else if (mode == ScreenMode::PinDisplay) {
        bg = "#00006e";
        fg = "#ffffff";
        inner = "<div style='text-align:center;margin-top:25%'>"
                "<div style='font-size:14px;margin-bottom:8px'>RESET PASSWORD</div>"
                "<div style='font-size:11px;color:#aaa'>PIN shown on device screen</div>"
                "</div>";
    }

    String html = "<div id='scr' style='background:" + bg + ";color:" + fg + ";"
                  "width:256px;height:256px;display:inline-block;position:relative;"
                  "font-family:monospace;font-size:12px;padding:6px;box-sizing:border-box;"
                  "border:4px solid #222;border-radius:6px;overflow:hidden;text-align:left'>"
                  + inner + "</div>";

    if (!_inSetupMode && _store.hasActiveAircraft()) {
        html += "<table style='width:256px;border-collapse:collapse;font-family:monospace;"
                "font-size:11px;margin-top:6px'>"
                "<thead><tr style='border-bottom:1px solid #888'>"
                "<th style='text-align:center;padding:2px 4px'>Cat</th>"
                "<th style='text-align:left;padding:2px 4px'>Tail</th>"
                "<th style='text-align:left;padding:2px 4px'>Type</th>"
                "<th style='text-align:right;padding:2px 4px'>Alt</th>"
                "<th style='text-align:center;padding:2px 4px'>SQK</th>"
                "<th style='text-align:right;padding:2px 4px'>ETA</th>"
                "</tr></thead><tbody>";

        for (const auto& kv : _store.activeAircraft()) {
            const Aircraft& ac = kv.second;
            String tail = ac.callsign.length() ? ac.callsign :
                          ac.registration.length() ? ac.registration : ac.icao;
            String trackUrl = "https://globe.adsbexchange.com/?icao=" + ac.icao;
            String altStr   = ac.altitude > 0 ? String((int)ac.altitude) + "ft" : "GND";

            int rawEta = ac.etaSeconds(_cfg.latitude, _cfg.longitude, _cfg.radius);
            int eta    = ac.adjustedEta(rawEta);
            char etaBuf[8];
            if (eta < 0) strcpy(etaBuf, "--:--");
            else         snprintf(etaBuf, sizeof(etaBuf), "%d:%02d", eta / 60, eta % 60);

            const char* catLetter;
            switch (ac.classification) {
                case AircraftClass::Military:   catLetter = "M"; break;
                case AircraftClass::Medevac:    catLetter = "E"; break;
                case AircraftClass::Commercial: catLetter = "C"; break;
                default:                        catLetter = "P"; break;
            }
            String catBg  = Display::rgb565ToCss(_display.backgroundColorFor(ac.classification));
            String catFg  = Display::rgb565ToCss(_display.foregroundColorFor(ac.classification));

            html += "<tr style='border-bottom:1px solid #333'>"
                    "<td style='text-align:center;padding:2px 4px'>"
                    "<span style='background:" + catBg + ";color:" + catFg + ";"
                    "padding:1px 5px;border-radius:3px;font-weight:bold'>" + catLetter + "</span>"
                    "</td>"
                    "<td style='padding:2px 4px'>"
                    "<a href='" + trackUrl + "' target='_blank' style='color:inherit'>" + tail + "</a>"
                    "</td>"
                    "<td style='padding:2px 4px'>" + String(ac.type.length() ? ac.type.c_str() : "?") + "</td>"
                    "<td style='text-align:right;padding:2px 4px'>" + altStr + "</td>"
                    "<td style='text-align:center;padding:2px 4px'>" +
                    (ac.squawk.length() ? (ac.isEmergencySquawk()
                        ? "<span class='sqwk-emrg'>" + ac.squawk + "</span>"
                        : ac.squawk)
                        : String("----")) +
                    "</td>"
                    "<td style='text-align:right;padding:2px 4px'" +
                    (eta >= 0 ? " data-eta='" + String(eta) + "'" : "") +
                    ">" + String(etaBuf) + "</td>"
                    "</tr>";
        }
        html += "</tbody></table>";
    }

    return html;
}

void WebUI::handleScreen() {
    if (!checkAuth()) return;
    if (_server.hasArg("fragment")) {
        _server.send(200, "text/html", buildScreenDiv());
        return;
    }

    uint32_t refreshSec = max((uint32_t)5, _cfg.pollIntervalMs / 1000u);
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

void WebUI::handleOtaCheck() {
    if (!checkAuth()) return;
    bool hasUpdate = _ota.check();
    String json = "{\"current\":\"" + String(OtaUpdater::currentVersion()) + "\","
                  "\"latest\":\"" + _ota.latestVersion() + "\","
                  "\"hasUpdate\":" + (hasUpdate ? "true" : "false") + ","
                  "\"status\":\"" + _ota.statusMessage() + "\"}";
    _server.send(200, "application/json", json);
}

void WebUI::handleOtaUpdate() {
    if (!checkAuth()) return;
    // Block here while downloading and flashing (~30-60 s).
    // The browser holds the connection open and receives the result when done.
    if (_ota.apply()) {
        _server.send(200, "application/json", "{\"ok\":true}");
        delay(500);
        ESP.restart();
    } else {
        String msg = _ota.statusMessage();
        msg.replace("\"", "'");
        _server.send(200, "application/json", "{\"ok\":false,\"msg\":\"" + msg + "\"}");
    }
}

void WebUI::handleAircraft() {
    if (!checkAuth()) return;
    _server.send(200, "application/json", _api.serializeActiveAircraft());
}

void WebUI::handleClearSummary() {
    if (!checkAuth()) return;
    _store.clearCounts();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUI::handleHistory() {
    if (!checkAuth()) return;
    _server.send(200, "application/json", _api.serializeHistory());
}

// ── Auth handlers ─────────────────────────────────────────────────────────────

void WebUI::handleLoginPost() {
    if (!_cfg.requireWebPassword || !_cfg.hasWebPassword()) {
        _sessionToken  = generateToken();
        _loginFailures = 0;
        _server.send(200, "application/json",
                     "{\"ok\":true,\"token\":\"" + _sessionToken + "\"}");
        return;
    }

    // While locked: 2FA code must accompany the password
    if (_lockoutCode.length() > 0) {
        String code = _server.arg("code");
        if (code != _lockoutCode) {
            _lockoutCodeAttempts++;
            bool regen = (_lockoutCodeAttempts >= 3);
            if (regen) {
                char buf[5];
                snprintf(buf, sizeof(buf), "%04u", esp_random() % 10000);
                _lockoutCode         = String(buf);
                _lockoutCodeAttempts = 0;
                _screenController.setPendingPin(_lockoutCode, "LOGIN CODE");
                _screenController.markChanged();
            }
            String resp = String("{\"ok\":false,\"lockout\":true,\"wrongCode\":true,\"regen\":")
                          + (regen ? "true" : "false") + "}";
            _server.send(401, "application/json", resp);
            return;
        }
        // Correct code — clear lockout and restore forgot-password pin if active
        _lockoutCode         = "";
        _loginFailures       = 0;
        _lockoutCodeAttempts = 0;
        if (_forgotPin.length() > 0) {
            _screenController.setPendingPin(_forgotPin);
        } else {
            _screenController.setPendingPin("");
            _screenController.setMode(ScreenMode::Scanning);
        }
        _screenController.markChanged();
    }

    String password = _server.arg("password");
    String hash     = computePasswordHash(password, String(_cfg.webPasswordSalt));

    if (hash == String(_cfg.webPasswordHash)) {
        _loginFailures = 0;
        _sessionToken  = generateToken();
        _server.send(200, "application/json",
                     "{\"ok\":true,\"token\":\"" + _sessionToken + "\"}");
        return;
    }

    _loginFailures++;
    if (_loginFailures >= 5 && _lockoutCode.length() == 0) {
        char buf[5];
        snprintf(buf, sizeof(buf), "%04u", esp_random() % 10000);
        _lockoutCode = String(buf);
        _screenController.setPendingPin(_lockoutCode, "LOGIN CODE");
        _screenController.setMode(ScreenMode::PinDisplay);
        _screenController.markChanged();
        _server.send(401, "application/json", "{\"ok\":false,\"lockout\":true}");
    } else {
        bool locked = _lockoutCode.length() > 0;
        _server.send(401, "application/json",
                     String("{\"ok\":false,\"lockout\":") + (locked ? "true" : "false") + "}");
    }
}

void WebUI::handleForgotPassword() {
    if (!_cfg.hasWebPassword()) {
        _server.send(400, "application/json", "{\"ok\":false,\"msg\":\"No password set\"}");
        return;
    }

    // If a lockout is active, clear it so the device screen can transition cleanly.
    _lockoutCode         = "";
    _lockoutCodeAttempts = 0;
    _loginFailures       = 0;

    char buf[5];
    snprintf(buf, sizeof(buf), "%04u", esp_random() % 10000);
    _forgotPin         = String(buf);
    _forgotPinAttempts = 0;

    _screenController.setPendingPin(_forgotPin);
    _screenController.setMode(ScreenMode::PinDisplay);
    _screenController.markChanged();

    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUI::handlePinReset() {
    if (_forgotPin.length() == 0) {
        _server.send(400, "application/json", "{\"ok\":false,\"msg\":\"No active PIN\"}");
        return;
    }

    String submittedPin = _server.arg("pin");
    String newPass      = _server.arg("newPass");

    if (submittedPin != _forgotPin) {
        _forgotPinAttempts++;
        bool regen = (_forgotPinAttempts >= 3);
        if (regen) {
            char buf[5];
            snprintf(buf, sizeof(buf), "%04u", esp_random() % 10000);
            _forgotPin         = String(buf);
            _forgotPinAttempts = 0;
            _screenController.setPendingPin(_forgotPin);
            _screenController.markChanged();
        }
        _server.send(200, "application/json",
                     String("{\"ok\":false,\"regen\":") + (regen ? "true" : "false") + "}");
        return;
    }

    // PIN correct — update or clear the password in NVS and in memory
    Preferences prefs;
    prefs.begin("plantracker", false);
    if (newPass.length() > 0) {
        String salt = generateSalt();
        String hash = computePasswordHash(newPass, salt);
        prefs.putString("webPassHash", hash);
        prefs.putString("webSalt",     salt);
        prefs.putBool  ("reqWebPass",  true);
        strncpy(_cfg.webPasswordHash, hash.c_str(), sizeof(_cfg.webPasswordHash) - 1);
        strncpy(_cfg.webPasswordSalt, salt.c_str(), sizeof(_cfg.webPasswordSalt) - 1);
        _cfg.requireWebPassword = true;
    } else {
        // Remove the hash/salt entirely so hasWebPassword() returns false
        // and a future save with reqWebPass=true cannot re-enable auth with a
        // password the user thought they cleared.
        prefs.remove("webPassHash");
        prefs.remove("webSalt");
        prefs.putBool("reqWebPass", false);
        _cfg.webPasswordHash[0] = '\0';
        _cfg.webPasswordSalt[0] = '\0';
        _cfg.requireWebPassword = false;
    }
    prefs.end();

    _forgotPin            = "";
    _forgotPinAttempts    = 0;
    _lockoutCode          = "";
    _lockoutCodeAttempts  = 0;
    _loginFailures        = 0;
    _screenController.setPendingPin("");
    _screenController.setMode(ScreenMode::Scanning);
    _screenController.markChanged();

    _sessionToken = generateToken();
    _server.send(200, "application/json",
                 "{\"ok\":true,\"token\":\"" + _sessionToken + "\"}");
}

void WebUI::handlePinCancel() {
    _forgotPin         = "";
    _forgotPinAttempts = 0;
    _screenController.setPendingPin("");
    _screenController.setMode(ScreenMode::Scanning);
    _screenController.markChanged();
    _server.send(200, "application/json", "{\"ok\":true}");
}

void WebUI::handlePinStatus() {
    bool active = _forgotPin.length() > 0 || _lockoutCode.length() > 0;
    _server.send(200, "application/json",
                 active ? "{\"active\":true}" : "{\"active\":false}");
}

