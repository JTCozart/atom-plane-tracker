#include "WebUI.h"
#include "Notifier.h"
#include <WiFi.h>
#include <Preferences.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void WebUI::begin(ScreenMode& mode, bool isSetupMode) {
    _screenMode  = &mode;
    _inSetupMode = isSetupMode;

    if (isSetupMode) {
        _ipAddress = "192.168.4.1";
        WiFi.mode(WIFI_AP);
        WiFi.softAP("PlaneTracker", "PlaneTracker");
    }

    _server.on("/",            HTTP_GET,  [this]() { handleRoot();       });
    _server.on("/save",        HTTP_POST, [this]() { handleSave();       });
    _server.on("/clear",       HTTP_POST, [this]() { handleClear();      });
    _server.on("/control",     HTTP_GET,  [this]() { handleControl();    });
    _server.on("/screen",      HTTP_GET,  [this]() { handleScreen();     });
    _server.on("/notify-test", HTTP_POST, [this]() { handleNotifyTest(); });
    _server.on("/ntfy-stats",  HTTP_GET,  [this]() { handleNtfyStats();  });
    _server.on("/api-test",    HTTP_GET,  [this]() { handleApiTest();    });
    _server.on("/ota-check",    HTTP_GET,  [this]() { handleOtaCheck();    });
    _server.on("/ota-update",   HTTP_POST, [this]() { handleOtaUpdate();   });
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
          ".preview-col{flex:0 0 auto;text-align:center;order:-1}"
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
          "#apiOut{background:#111;color:#00ff00;font-family:monospace;font-size:11px;"
          "padding:10px;border-radius:4px;height:320px;overflow-y:auto;"
          "white-space:pre-wrap;word-break:break-all;margin-top:10px;"
          "border:1px solid #333}"
          "#liveWrap:fullscreen{background:#000;display:flex;flex-direction:column;"
          "align-items:center;justify-content:center;gap:12px}"
          "#liveWrap:fullscreen #scr{width:min(80vh,80vw)!important;height:min(80vh,80vw)!important;"
          "font-size:calc(min(80vh,80vw)*12/256)!important}"
          "#liveWrap:fullscreen .ctrl{width:min(80vh,80vw);display:flex;gap:6px}"
          "#liveWrap:fullscreen .cb{flex:1;padding:12px 0;font-size:.9em}"
          "</style>"
          "<link rel='stylesheet' href='https://unpkg.com/leaflet@1.9.4/dist/leaflet.css'>"
          "<link rel='stylesheet' href='https://cdnjs.cloudflare.com/ajax/libs/font-awesome/6.5.0/css/all.min.css'>"
          "<script src='https://unpkg.com/leaflet@1.9.4/dist/leaflet.js'></script>"
          "<script>"
          "function startRadar(c){"
            "if(window._radarRaf){cancelAnimationFrame(window._radarRaf);window._radarRaf=null;}"
            "var ctx=c.getContext('2d');"
            "var cx=128,cy=100,oR=70,iR=36,N=36,fr=0;"
            "var trail=['#003200','#008200','#00d200','#00ff00'];"
            "function draw(){"
              "ctx.fillStyle='#000';ctx.fillRect(0,0,256,256);"
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
              "ctx.fillText('SCANNING',(256-tw)/2,200);"
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
            "fetch('/screen?fragment=1')"
            ".then(function(r){return r.text();})"
            ".then(function(h){"
              "if(window._radarRaf){cancelAnimationFrame(window._radarRaf);window._radarRaf=null;}"
              "var e=document.getElementById('scrWrap');"
              "if(e)e.innerHTML=h;"
              "var rc=document.getElementById('rc');"
              "if(rc)startRadar(rc);"
              "startEtaCountdown();"
            "})"
            ".catch(function(){});"
          "}"
          "function ctrl(s){"
            "fetch('/control?screen='+s).then(function(){setTimeout(refresh,300);});"
            "document.querySelectorAll('.cb').forEach(function(b){"
              "b.classList.toggle('on',b.dataset.s===s);});"
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
            "fetch('/ntfy-stats')"
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
            "fetch('/api-test?lat='+lat+'&lon='+lon+'&radius='+rad)"
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
            "fetch('/notify-test',{method:'POST'})"
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
          "function toggleCats(){"
            "document.getElementById('catDrop').classList.toggle('open');"
          "}"
          "function updateCats(){"
            "var checked=Array.from(document.querySelectorAll('#catDrop input:checked'));"
            "document.getElementById('ntfyClasses').value=checked.map(function(c){return c.value;}).join(',');"
            "var lbl=checked.length?checked.map(function(c){return c.dataset.label;}).join(', '):'All categories';"
            "document.getElementById('catBtn').innerHTML=lbl+'&nbsp;<i class=\"fa-solid fa-chevron-down\" style=\"font-size:.75em\"></i>';"
          "}"
          "document.addEventListener('click',function(e){"
            "if(!e.target.closest('#catWrap'))document.getElementById('catDrop').classList.remove('open');"
          "});"
          "document.addEventListener('DOMContentLoaded',function(){"
            "showTab(localStorage.getItem('pt-tab')||'wifi');"
            "updateCats();"
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
            "fetch('/ota-check')"
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
          "function doOtaUpdate(){"
            "var ver=document.getElementById('otaLatest').textContent;"
            "if(!confirm('Download and install '+ver+'? The device will reboot.'))return;"
            "document.getElementById('otaUpdateBtn').disabled=true;"
            "document.getElementById('otaModalVersion').textContent=ver;"
            "document.getElementById('otaModal').classList.add('open');"
            "fetch('/ota-update',{method:'POST'})"
            ".then(function(r){return r.text();})"
            ".then(function(t){document.open();document.write(t);document.close();})"
            ".catch(function(e){"
              "document.getElementById('otaModal').classList.remove('open');"
              "document.getElementById('otaStatus').textContent='Request failed: '+e;"
              "document.getElementById('otaUpdateBtn').disabled=false;"
            "});"
          "}"
          "function toggleFullscreen(){"
            "var el=document.getElementById('liveWrap');"
            "if(!document.fullscreenElement){el.requestFullscreen().catch(function(){});}"
            "else{document.exitFullscreen();}"
          "}"
          "document.addEventListener('fullscreenchange',function(){"
            "var btn=document.getElementById('fsBtn');"
            "if(!btn)return;"
            "btn.innerHTML=document.fullscreenElement?"
              "'<i class=\"fa-solid fa-compress\"></i>':'<i class=\"fa-solid fa-expand\"></i>';"
          "});"
          "</script>"
          "</head><body>"
          "<button id='dmBtn' onclick='toggleDark()' title='Toggle dark mode'><i class='fa-solid fa-moon'></i></button>"
          "<h2><i class='fa-solid fa-plane' style='margin-right:8px'></i>PlaneTracker Setup</h2>"
          "<div class='layout'>"
          "<div class='form-col'>"
          "<form method='POST' action='/save'>");

    // ── Tab header ────────────────────────────────────────────────────────────
    html += "<div class='card'>"
            "<div class='tabs' style='border:none;border-radius:0;margin:0'>"
            "<div class='tab-hdr'>"
            "<button type='button' class='tab-btn' data-tab='wifi'    onclick='showTab(\"wifi\")'>"
            "<i class='fa-solid fa-wifi'></i><br>WiFi</button>"
            "<button type='button' class='tab-btn' data-tab='detect'  onclick='showTab(\"detect\")'>"
            "<i class='fa-solid fa-satellite-dish'></i><br>Detection</button>"
            "<button type='button' class='tab-btn' data-tab='notify'  onclick='showTab(\"notify\")'>"
            "<i class='fa-solid fa-bell'></i><br>Notifications</button>"
            "<button type='button' class='tab-btn' data-tab='apitest' onclick='showTab(\"apitest\")'>"
            "<i class='fa-solid fa-terminal'></i><br>API Test</button>"
            "<button type='button' class='tab-btn' data-tab='update' onclick='showTab(\"update\")'>"
            "<i class='fa-solid fa-cloud-arrow-down'></i><br>Update</button>"
            "</div>";

    // ── WiFi tab ──────────────────────────────────────────────────────────────
    html += "<div class='tab-panel' id='tab-wifi'>";
    html += "<label>SSID</label>"
            "<input name='ssid' value='" + String(_cfg.ssid) + "'>";
    html += "<label>Password</label>"
            "<input name='pass' type='password' placeholder='leave blank to keep current'>";
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
            "<input name='ntfyTopic' value='" + String(_cfg.notifyTopic) + "'>";
    {
        // Pre-check boxes based on saved filter; empty filter = all (nothing checked)
        const char* f = _cfg.notifyClassFilter;
        bool hasFilter = strlen(f) > 0;
        String milChk  = (hasFilter && strstr(f, "MIL"))    ? " checked" : "";
        String medChk  = (hasFilter && strstr(f, "MEDVAC")) ? " checked" : "";
        String commChk = (hasFilter && strstr(f, "COMM"))   ? " checked" : "";
        String privChk = (hasFilter && strstr(f, "PRIV"))   ? " checked" : "";

        String updChk = _cfg.notifyUpdates ? " checked" : "";
        html += "<label>ntfy Notification Categories</label>"
                "<div class='chk-wrap' id='catWrap'>"
                "<button type='button' class='chk-btn' id='catBtn' onclick='toggleCats()'>"
                "All categories&nbsp;<i class='fa-solid fa-chevron-down' style='font-size:.75em'></i>"
                "</button>"
                "<div class='chk-drop' id='catDrop'>"
                "<label class='chk-item'><input type='checkbox' value='MIL'    data-label='Military'" + milChk  + " onchange='updateCats()'> Military</label>"
                "<label class='chk-item'><input type='checkbox' value='MEDVAC' data-label='Medevac'"  + medChk  + " onchange='updateCats()'> Medevac</label>"
                "<label class='chk-item'><input type='checkbox' value='COMM'   data-label='Commercial'" + commChk + " onchange='updateCats()'> Commercial</label>"
                "<label class='chk-item'><input type='checkbox' value='PRIV'   data-label='Private'"  + privChk + " onchange='updateCats()'> Private</label>"
                "<label class='chk-item'><input type='checkbox' name='ntfyUpdates' value='1'" + updChk + "> Updates</label>"
                "</div>"
                "<input type='hidden' name='ntfyClasses' id='ntfyClasses' value='" + String(f) + "'>"
                "</div>";
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
    html += "<button class='btn' type='submit'>"
            "<i class='fa-solid fa-floppy-disk' style='margin-right:6px'></i>Save &amp; Reboot</button>"
            "</form>"
            "<form method='POST' action='/clear' style='margin-top:10px'>"
            "<button class='btn' type='submit' style='background:#D32F2F' "
            "onclick='return confirm(\"Reset all settings to factory defaults? This cannot be undone.\")'>"
            "<i class='fa-solid fa-trash' style='margin-right:6px'></i>Clear All Settings</button>"
            "</form>"
            "</div>"; // end form-col

    // Right column - live screen preview + controls
    html += "<div class='preview-col'>"
            "<div class='card'>"
            "<div class='card-hdr' style='justify-content:space-between'>"
            "<span><i class='fa-solid fa-display'></i>&nbsp;Live Screen</span>"
            "<button id='fsBtn' onclick='toggleFullscreen()' title='Fullscreen' "
            "style='background:none;border:none;cursor:pointer;color:inherit;"
            "font-size:.9em;padding:0 2px'>"
            "<i class='fa-solid fa-expand'></i></button>"
            "</div>"
            "<div class='card-body' style='text-align:center'>"
            "<div id='liveWrap'>"
            "<div id='scrWrap'></div>"
            "<div class='ctrl' style='margin-top:10px'>"
            "<button class='cb' data-s='scan'    onclick='ctrl(\"scan\")'><i class='fa-solid fa-satellite-dish'></i><br>Scan</button>"
            "<button class='cb' data-s='history' onclick='ctrl(\"history\")'><i class='fa-solid fa-clock-rotate-left'></i><br>History</button>"
            "<button class='cb' data-s='summary' onclick='ctrl(\"summary\")'><i class='fa-solid fa-chart-bar'></i><br>Summary</button>"
            "<button class='cb' data-s='debug'   onclick='ctrl(\"debug\")'><i class='fa-solid fa-bug'></i><br>Debug</button>"
            "</div>"
            "</div>"
            "</div>"
            "</div>"
            "</div>"; // end preview-col

    // OTA update progress modal - no close button, cannot be dismissed
    html += "<div class='modal' id='otaModal' style='z-index:2000'>"
            "<div style='background:#fff;padding:32px 28px;border-radius:10px;"
            "text-align:center;max-width:320px;width:90vw'>"
            "<div style='font-size:2.5em;margin-bottom:12px'>&#128257;</div>"
            "<div style='font-size:1.1em;font-weight:700;margin-bottom:8px'>Updating Firmware</div>"
            "<div id='otaModalVersion' style='font-family:monospace;font-size:.9em;"
            "color:#555;margin-bottom:16px'></div>"
            "<div style='color:#c00;font-weight:600;margin-bottom:6px'>"
            "<i class='fa-solid fa-triangle-exclamation' style='margin-right:5px'></i>"
            "Do not unplug the device</div>"
            "<div style='font-size:.85em;color:#666'>The device will reboot automatically when complete.</div>"
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

    html += "<div style='text-align:center;margin-top:24px;padding-bottom:16px;font-size:.85em;color:#888'>"
            "<a href='https://github.com/JTCozart/atom-plane-tracker' target='_blank' "
            "style='color:inherit;text-decoration:none'>"
            "<i class='fa-brands fa-github' style='font-size:1.4em;margin-right:6px;vertical-align:middle'></i>"
            "JTCozart/atom-plane-tracker</a>"
            "</div>";
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
        uint32_t pollValue = (uint32_t)_server.arg("poll").toInt() * 1000;  // form sends seconds
        if (pollValue < Config::kMinPollIntervalMs) pollValue = Config::kMinPollIntervalMs;
        prefs.putUInt("pollMs", pollValue);
    }
    if (_server.hasArg("ntfyToken") && _server.arg("ntfyToken").length() > 0)
        prefs.putString("ntfyToken",   _server.arg("ntfyToken"));
    if (_server.hasArg("ntfyTopic"))
        prefs.putString("ntfyTopic",   _server.arg("ntfyTopic"));
    if (_server.hasArg("ntfyClasses"))
        prefs.putString("ntfyClasses", _server.arg("ntfyClasses"));
    prefs.putBool("ntfyUpdates", _server.hasArg("ntfyUpdates"));

    prefs.end();

    _server.send(200, "text/html", buildRebootPage("Saved!", "Rebooting&hellip;"));
    delay(1500);
    ESP.restart();
}

void WebUI::handleControl() {
    if (_server.hasArg("screen") && _screenMode) {
        String s = _server.arg("screen");
        if (s == "history") {
            if (*_screenMode == ScreenMode::History && _store.historyCount() > 0) {
                // Already on history - page to the next entry, wrapping at the end
                int next = _store.historyIndex() + 1;
                _store.setHistoryIndex(next < _store.historyCount() ? next : 0);
            } else {
                *_screenMode = ScreenMode::History;
                _store.setHistoryIndex(0);
            }
        } else if (s == "scan")    *_screenMode = ScreenMode::Scanning;
        else if (s == "summary") *_screenMode = ScreenMode::Summary;
        else if (s == "debug")   *_screenMode = ScreenMode::Debug;
        _controlChanged = true;
    }
    _server.send(200, "text/plain", "OK");
}

void WebUI::handleClear() {
    Preferences prefs;
    prefs.begin("plantracker", false);
    prefs.clear();  // Erase all keys in this namespace
    prefs.end();

    _server.send(200, "text/html", buildRebootPage("Settings Cleared!", "Rebooting with factory defaults&hellip;"));
    delay(1500);
    ESP.restart();
}

void WebUI::handleNotifyTest() {
    if (strlen(_cfg.notifyToken) == 0 || strlen(_cfg.notifyTopic) == 0) {
        _server.send(200, "text/plain", "NOT_CONFIGURED");
        return;
    }
    int code = Notifier::sendTestHttp(_cfg);
    _server.send(200, "text/plain", String(code));
}

void WebUI::handleApiTest() {
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

    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, http.getStream(),
                                               DeserializationOption::Filter(filter));
    http.end();

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

String WebUI::buildRebootPage(const String& heading, const String& subtext) {
    return String(
        "<!DOCTYPE html><html><head><title>PlaneTracker</title>"
        "<style>"
        "body{font-family:sans-serif;text-align:center;margin-top:60px;background:#fff;color:#222;transition:background .2s,color .2s}"
        "body.dark{background:#1a1a1a;color:#e0e0e0}"
        "p{color:#666}"
        "body.dark p{color:#aaa}"
        "#wifi-warn{display:none;margin:24px auto;max-width:380px;padding:14px 18px;"
        "background:#FFF8E1;border:1px solid #FFB300;border-radius:6px;"
        "color:#5D4037;font-size:.9em;text-align:left;line-height:1.5}"
        "body.dark #wifi-warn{background:#2d2500;border-color:#6d5800;color:#e0c84a}"
        "</style>"
        "<script>"
        "function poll(){"
          "fetch('/').then(function(r){"
            "if(r.ok)window.location.href='/';"
            "else setTimeout(poll,1000);"
          "}).catch(function(){setTimeout(poll,1000);});"
        "}"
        "setTimeout(poll,4000);"
        "setTimeout(function(){document.getElementById('wifi-warn').style.display='block';},30000);"
        "(function(){"
          "var s=localStorage.getItem('pt-dark');"
          "var sys=window.matchMedia&&window.matchMedia('(prefers-color-scheme:dark)').matches;"
          "if(s!==null?s==='1':sys)document.documentElement.classList.add('dark-pending');"
        "})();"
        "document.addEventListener('DOMContentLoaded',function(){"
          "var s=localStorage.getItem('pt-dark');"
          "var sys=window.matchMedia&&window.matchMedia('(prefers-color-scheme:dark)').matches;"
          "if(s!==null?s==='1':sys)document.body.classList.add('dark');"
        "});"
        "</script>"
        "</head><body>"
        "<h2>") + heading + String("</h2>"
        "<p>") + subtext + String("</p>"
        "<p>Returning to settings when back online.</p>"
        "<div id='wifi-warn'>"
        "<strong>Taking too long?</strong><br>"
        "If you changed the WiFi network or password, the device may have started "
        "its own setup hotspot instead. Connect to the <strong>PlaneTracker</strong> "
        "WiFi network and open <strong>192.168.4.1</strong> to reconfigure."
        "</div>"
        "</body></html>");
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
                "style='position:absolute;top:0;left:0'></canvas>";
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
        {
            uint32_t up = millis() / 1000;
            char upBuf[16];
            snprintf(upBuf, sizeof(upBuf), "UP: %02u:%02u:%02u",
                     up / 3600, (up % 3600) / 60, up % 60);
            inner += "<div>" + String(upBuf) + "</div>";
        }
        int shown = min((int)lines.size(), 12);
        for (int i = 0; i < shown; i++) {
            String line = lines[i];
            if (line.length() > 21) line = line.substring(0, 21);
            inner += "<div>" + line + "</div>";
        }
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
            String catBg  = rgb565ToCss(_display.backgroundColorFor(ac.classification));
            String catFg  = rgb565ToCss(_display.foregroundColorFor(ac.classification));

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

void WebUI::handleOtaCheck() {
    bool hasUpdate = _ota.check();
    String json = "{\"current\":\"" + String(OtaUpdater::currentVersion()) + "\","
                  "\"latest\":\"" + _ota.latestVersion() + "\","
                  "\"hasUpdate\":" + (hasUpdate ? "true" : "false") + ","
                  "\"status\":\"" + _ota.statusMessage() + "\"}";
    _server.send(200, "application/json", json);
}

void WebUI::handleOtaUpdate() {
    // Block here while downloading and flashing (~30-60 s).
    // The browser holds the connection open and receives the result when done.
    if (_ota.apply()) {
        _server.send(200, "text/html",
                     buildRebootPage("Update Successful",
                                     "Rebooting to " + _ota.latestVersion() + "&hellip;"));
        delay(500);
        ESP.restart();
    } else {
        _server.send(200, "text/html",
                     buildRebootPage("Update Failed", _ota.statusMessage()));
    }
}

