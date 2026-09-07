#include "RaceSyncApi.h"

namespace {
const char RACESYNC_UI[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RaceSync</title><style>
:root{font-family:system-ui,-apple-system,sans-serif;color:#f4f6f8;background:#101418}*{box-sizing:border-box}body{margin:0}.wrap{max-width:920px;margin:auto;padding:18px}.head,.bar,.session{background:#192027;border:1px solid #2a343d;border-radius:12px;padding:16px;margin-bottom:12px}.head{display:flex;justify-content:space-between;align-items:center}.brand{font-size:26px;font-weight:800}.sub,.meta{color:#9ba8b4;font-size:13px}.status{font-size:13px;text-align:right}.ok{color:#75d69c}.warn{color:#ffca6b}.bar{display:flex;gap:10px;justify-content:space-between;align-items:center}.btn{border:0;border-radius:8px;padding:10px 13px;font-weight:700;cursor:pointer;background:#e9eef2;color:#111}.primary{background:#54bdf5}.ghost{background:#303a43;color:#fff}.session{display:grid;grid-template-columns:1fr auto;gap:12px}.title{font-weight:750;font-size:16px}.timing{display:grid;grid-template-columns:repeat(4,minmax(100px,1fr));gap:8px;margin-top:10px}.timing div{background:#12181d;border-radius:8px;padding:8px}.timing b{display:block;font-size:11px;color:#87939e;margin-bottom:3px}.new{display:inline-block;background:#54bdf5;color:#071018;border-radius:12px;padding:2px 8px;font-size:11px;font-weight:800;margin-right:7px}.flag{font-size:15px;margin-right:7px}.actions{display:flex;gap:7px;align-items:center;flex-wrap:wrap}.empty{text-align:center;color:#9ba8b4;padding:35px}.foot{color:#87939e;font-size:12px;text-align:center;padding:12px}.nav{margin-top:10px;display:flex;gap:14px;flex-wrap:wrap}.nav a{color:#54bdf5;text-decoration:none;font-size:13px}@media(max-width:650px){.session{grid-template-columns:1fr}.actions .btn{flex:1}.bar{align-items:stretch;flex-direction:column}.head{align-items:flex-start}.status{text-align:left;margin-top:10px}.timing{grid-template-columns:1fr 1fr}}
</style></head><body><main class="wrap">
<div class="head"><div><div class="brand">RaceSync</div><div class="sub">Motorcycle Data Logger</div><div class="nav"><a href="/control">Logging Control & Settings</a><a href="/status">Full Device Status</a></div></div><div class="status"><div id="gps">GPS ...</div><div id="sd">Storage ...</div><div id="rpm">RPM ...</div><div id="log">Logger ...</div></div></div>
<div class="bar"><div><strong>Stored Sessions</strong><div class="sub" id="summary">Loading...</div></div><button class="btn primary" id="all" onclick="downloadAllNew()">Download All New</button></div>
<div id="sessions"></div><div class="foot" id="footer">RaceSync</div></main>
<script>
const key='racesync_downloaded_sessions';let sessions=[];const months=['Jan','Feb','Mar','Apr','May','Jun','Jul','Aug','Sep','Oct','Nov','Dec'];
function downloaded(){try{return new Set(JSON.parse(localStorage.getItem(key)||'[]').map(String))}catch(e){return new Set()}}
function mark(id){const s=downloaded();s.add(String(id));localStorage.setItem(key,JSON.stringify([...s]));render()}
function fmt(n){if(n==null)return '';if(n<1024)return n+' B';if(n<1048576)return (n/1024).toFixed(1)+' KB';return (n/1048576).toFixed(1)+' MB'}
function esc(s){return String(s??'').replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function parts(file){let m=String(file).match(/^RS_(\d{4})-(\d{2})-(\d{2})_(\d{2})-(\d{2})-(\d{2})\.vbo$/i);if(!m)return null;return {y:+m[1],mo:+m[2],d:+m[3],h:+m[4],mi:+m[5],s:+m[6]}}
function stamp(s){if(s.startTime&&/^\d{4}-/.test(s.startTime)){const t=Date.parse(s.startTime);if(Number.isFinite(t))return t}const p=parts(s.file);return p?Date.UTC(p.y,p.mo-1,p.d,p.h,p.mi,p.s):0}
function sortSessions(list){return [...list].sort((a,b)=>stamp(b)-stamp(a)||String(b.file).localeCompare(String(a.file)))}
function fallbackStart(file){const p=parts(file);if(!p)return {date:'Unknown',time:'—'};return {date:String(p.d).padStart(2,'0')+' '+months[p.mo-1]+' '+p.y,time:String(p.h).padStart(2,'0')+':'+String(p.mi).padStart(2,'0')+':'+String(p.s).padStart(2,'0')}}
function dt(v){if(!v||!/^\d{4}-/.test(v))return null;const d=new Date(v);if(!Number.isFinite(d.getTime()))return null;return {date:String(d.getUTCDate()).padStart(2,'0')+' '+months[d.getUTCMonth()]+' '+d.getUTCFullYear(),time:String(d.getUTCHours()).padStart(2,'0')+':'+String(d.getUTCMinutes()).padStart(2,'0')+':'+String(d.getUTCSeconds()).padStart(2,'0')}}
function elapsed(sec){if(sec==null||!Number.isFinite(Number(sec)))return '—';sec=Math.max(0,Math.round(Number(sec)));const h=Math.floor(sec/3600),m=Math.floor((sec%3600)/60),s=sec%60;return (h?h+'h ':'')+(m?m+'m ':'')+s+'s'}
function render(){const seen=downloaded(),box=document.getElementById('sessions');const complete=sessions.filter(s=>s.complete);const fresh=complete.filter(s=>!seen.has(String(s.id)));document.getElementById('summary').textContent=complete.length+' sessions · '+fresh.length+' new · newest first';document.getElementById('all').disabled=!fresh.length;box.innerHTML=sessions.length?'':'<div class="empty">No stored sessions</div>';sessions.forEach(s=>{const isNew=!seen.has(String(s.id))&&s.complete;const start=dt(s.startTime)||fallbackStart(s.file),end=dt(s.endTime);const d=document.createElement('div');d.className='session';d.innerHTML='<div><div class="title">'+(s.successful?'<span class="flag" title="Completed recording">🏁</span>':'')+(isNew?'<span class="new">NEW</span>':'')+esc(s.file)+'</div><div class="timing"><div><b>DATE</b>'+esc(start.date)+'</div><div><b>START</b>'+esc(start.time)+'</div><div><b>END</b>'+esc(end?end.time:'—')+'</div><div><b>ELAPSED</b>'+esc(elapsed(s.elapsedSeconds))+'</div></div><div class="meta" style="margin-top:8px">'+fmt(s.sizeBytes)+(s.active?' · RECORDING':'')+'</div></div><div class="actions"><button class="btn primary">Download VBO</button>'+(s.hasKml?'<button class="btn ghost">Generate KML</button>':'')+'</div>';const b=d.querySelectorAll('button');let i=0;b[i++].onclick=()=>dl(s.downloadUrl,s.id);if(s.hasKml)b[i++].onclick=()=>dl(s.kmlDownloadUrl,s.id);box.appendChild(d)})}
function dl(url,id){const a=document.createElement('a');a.href=url;a.click();mark(id)}
function downloadAllNew(){const seen=downloaded();const fresh=sessions.filter(s=>s.complete&&!seen.has(String(s.id)));fresh.forEach((s,i)=>setTimeout(()=>dl(s.downloadUrl,s.id),i*600))}
async function load(){try{const [sr,st,tr]=await Promise.all([fetch('/api/session-summaries',{cache:'no-store'}),fetch('/api/status',{cache:'no-store'}),fetch('/api/telemetry',{cache:'no-store'})]);const sj=await sr.json(),x=await st.json(),t=await tr.json();sessions=sortSessions(sj.sessions||[]);const rpm=Math.max(0,Math.round(Number(t.channels&&t.channels.Revs||0)));document.getElementById('gps').innerHTML='<span class="'+(x.gps&&x.gps.validFix?'ok':'warn')+'">●</span> GPS '+(x.gps&&x.gps.validFix?'READY':'WAITING');document.getElementById('sd').innerHTML='<span class="'+(x.storage&&x.storage.ready?'ok':'warn')+'">●</span> '+(x.storage&&x.storage.ready?'STORAGE READY':'STORAGE ERROR');document.getElementById('rpm').textContent='RPM '+rpm.toLocaleString();document.getElementById('log').textContent=x.logger&&x.logger.recording?'● RECORDING':'Logger idle';document.getElementById('footer').textContent=fmt(x.storage.freeBytes)+' free · '+(x.system.firmware||'RaceSync');render()}catch(e){document.getElementById('summary').textContent='Unable to contact RaceSync'}}
load();setInterval(load,5000);
</script></body></html>)HTML";

const char RACESYNC_STATUS_UI[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>RaceSync Device Status</title><style>:root{font-family:system-ui,-apple-system,sans-serif;color:#f4f6f8;background:#101418}body{margin:0}.wrap{max-width:1050px;margin:auto;padding:18px}.head,.panel{background:#192027;border:1px solid #2a343d;border-radius:12px;padding:16px;margin-bottom:12px}.head{display:flex;justify-content:space-between;gap:12px}.btn{display:inline-block;border:0;border-radius:8px;padding:9px 12px;font-weight:700;background:#303a43;color:#fff;text-decoration:none;cursor:pointer}.sub{color:#9ba8b4;font-size:13px}pre{white-space:pre-wrap;word-break:break-word;color:#dbe7ef}</style></head><body><main class="wrap"><div class="head"><div><h2 style="margin:0">RaceSync Device Status</h2><div class="sub">Live device diagnostics</div></div><div><a class="btn" href="/">Sessions</a> <a class="btn" href="/control">Control</a></div></div><div class="panel"><div>RPM <strong id="rpm">--</strong> <label style="float:right"><input type="checkbox" id="led" onchange="saveLed()"> RPM blue LED</label></div></div><div class="panel"><pre id="status">Loading...</pre></div></main><script>async function load(){try{const [s,t]=await Promise.all([fetch('/api/status',{cache:'no-store'}),fetch('/api/telemetry',{cache:'no-store'})]);const x=await s.json(),y=await t.json();document.getElementById('status').textContent=JSON.stringify(x,null,2);document.getElementById('rpm').textContent=Math.round(Number(y.rpm&&y.rpm.value||0)).toLocaleString();if(y.rpm&&typeof y.rpm.ledEnabled==='boolean')document.getElementById('led').checked=y.rpm.ledEnabled}catch(e){document.getElementById('status').textContent='Unable to read status'}}async function saveLed(){const enabled=document.getElementById('led').checked;await fetch('/api/settings/rpm-led',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({enabled})})}load();setInterval(load,1000)</script></body></html>)HTML";

const char RACESYNC_CONTROL_UI[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>RaceSync Logging Control</title><style>:root{font-family:system-ui,-apple-system,sans-serif;color:#f4f6f8;background:#101418}body{margin:0}.wrap{max-width:760px;margin:auto;padding:18px}.panel{background:#192027;border:1px solid #2a343d;border-radius:12px;padding:18px;margin-bottom:12px}.btn,input{border:0;border-radius:9px;padding:11px;font-size:15px}.btn{font-weight:800;cursor:pointer;margin:4px}.start{background:#75d69c}.stop{background:#e76b6b}.save{background:#54bdf5}.reboot{background:#ffca6b}.link{color:#54bdf5}.row{display:grid;grid-template-columns:1fr 1fr;gap:12px}.sub{color:#9ba8b4;font-size:13px}@media(max-width:560px){.row{grid-template-columns:1fr}}</style></head><body><main class="wrap"><div class="panel"><h2 style="margin-top:0">Logging Control & Settings</h2><a class="link" href="/">Sessions</a> · <a class="link" href="/status">Status</a></div><div class="panel"><h3 id="state">Loading...</h3><div id="details" class="sub"></div><button class="btn start" id="start" onclick="cmd('/api/logging/start')">Start Manual Logging</button><button class="btn stop" id="stop" onclick="cmd('/api/logging/stop')">Stop Logging</button></div><div class="panel"><h3>Automatic Logging</h3><div class="row"><label>Start speed km/h<br><input id="speed" type="number" min="1" max="100" step="0.5"></label><label>Stop delay seconds<br><input id="delay" type="number" min="1" max="600"></label></div><button class="btn save" onclick="save()">Save Settings</button></div><div class="panel"><h3>Restart RaceSync</h3><button class="btn reboot" onclick="reboot()">Reboot ESP32</button></div></main><script>let loaded=false;async function load(){try{const r=await fetch('/api/status',{cache:'no-store'}),x=await r.json(),rec=!!x.logger.recording;document.getElementById('state').textContent=rec?'RECORDING':'IDLE';document.getElementById('details').textContent='GPS '+(x.gps.validFix?'ready':'waiting')+' · SD '+(x.storage.ready?'ready':'not ready')+(rec?' · '+x.logger.recordingSeconds+' s':'');document.getElementById('start').disabled=rec||!x.gps.validFix||!x.storage.ready;document.getElementById('stop').disabled=!rec;if(!loaded){document.getElementById('speed').value=x.logger.startSpeedKmh;document.getElementById('delay').value=x.logger.stopDelaySeconds;loaded=true}}catch(e){document.getElementById('state').textContent='OFFLINE'}}async function cmd(u){await fetch(u,{method:'POST'});load()}async function save(){await fetch('/api/settings/logging',{method:'POST',headers:{'Content-Type':'application/json'},body:JSON.stringify({startSpeedKmh:Number(document.getElementById('speed').value),stopDelaySeconds:Number(document.getElementById('delay').value)})});load()}async function reboot(){if(confirm('Reboot RaceSync now?'))await fetch('/api/reboot',{method:'POST'})}load();setInterval(load,2000)</script></body></html>)HTML";

void addSessionMetadata(RaceSyncStorage& storage, JsonObject session)
{
    const String filename = session["file"].as<String>();
    String logFilename = filename;
    if (logFilename.endsWith(".vbo")) logFilename.replace(".vbo", ".log");

    String startTime;
    String endTime;
    long elapsedSeconds = -1;
    bool finalized = false;
    bool metadataFound = false;

    File log = storage.openFileRead(logFilename);
    if (log)
    {
        metadataFound = true;
        while (log.available())
        {
            String line = log.readStringUntil('\n');
            line.trim();
            const int eq = line.indexOf('=');
            if (eq <= 0) continue;
            const String key = line.substring(0, eq);
            const String value = line.substring(eq + 1);
            if (key == "startTime") startTime = value;
            else if (key == "endTime") endTime = value;
            else if (key == "durationSeconds") elapsedSeconds = value.toInt();
            else if (key == "vboFinalized") finalized = value == "true";
        }
        log.close();
    }

    session["metadataAvailable"] = metadataFound;
    if (startTime.length()) session["startTime"] = startTime;
    if (endTime.length()) session["endTime"] = endTime;
    if (elapsedSeconds >= 0) session["elapsedSeconds"] = elapsedSeconds;
    session["successful"] = session["complete"].as<bool>() && (!metadataFound || finalized);
}
}

void RaceSyncApi::beginWebUiRoute()
{
    _server.on("/", HTTP_GET, [this]() {
        _server.sendHeader("Cache-Control", "no-store");
        _server.send_P(200, "text/html", RACESYNC_UI);
    });

    _server.on("/status", HTTP_GET, [this]() {
        _server.sendHeader("Cache-Control", "no-store");
        _server.send_P(200, "text/html", RACESYNC_STATUS_UI);
    });

    _server.on("/control", HTTP_GET, [this]() {
        _server.sendHeader("Cache-Control", "no-store");
        _server.send_P(200, "text/html", RACESYNC_CONTROL_UI);
    });

    _server.on("/api/session-summaries", HTTP_GET, [this]() {
        JsonDocument doc;
        doc["device"] = RaceSyncConfig::PRODUCT;
        JsonArray sessions = doc["sessions"].to<JsonArray>();
        const String activeFilename = _logger.recording() ? _logger.currentFilename() : "";
        _storage.addSessionsToJson(sessions, activeFilename);
        for (JsonObject session : sessions) addSessionMetadata(_storage, session);
        doc["count"] = sessions.size();
        String response;
        serializeJson(doc, response);
        sendJson(200, response);
    });
}
