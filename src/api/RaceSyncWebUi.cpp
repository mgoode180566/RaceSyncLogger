#include "RaceSyncApi.h"

namespace {
const char RACESYNC_UI[] PROGMEM = R"HTML(<!doctype html>
<html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>RaceSync</title><style>
:root{font-family:system-ui,-apple-system,sans-serif;color:#f4f6f8;background:#101418}*{box-sizing:border-box}body{margin:0}.wrap{max-width:900px;margin:auto;padding:18px}.head,.bar,.session{background:#192027;border:1px solid #2a343d;border-radius:12px;padding:16px;margin-bottom:12px}.head{display:flex;justify-content:space-between;align-items:center}.brand{font-size:26px;font-weight:800}.sub{color:#9ba8b4;font-size:13px}.status{font-size:13px;text-align:right}.ok{color:#75d69c}.warn{color:#ffca6b}.bar{display:flex;gap:10px;justify-content:space-between;align-items:center}.btn{border:0;border-radius:8px;padding:10px 13px;font-weight:700;cursor:pointer;background:#e9eef2;color:#111}.primary{background:#54bdf5}.danger{background:#e76b6b}.ghost{background:#303a43;color:#fff}.session{display:grid;grid-template-columns:1fr auto;gap:12px}.title{font-weight:750}.meta{font-size:13px;color:#9ba8b4;margin-top:5px}.new{display:inline-block;background:#54bdf5;color:#071018;border-radius:12px;padding:2px 8px;font-size:11px;font-weight:800;margin-right:7px}.done{color:#75d69c;font-size:12px;margin-right:7px}.actions{display:flex;gap:7px;align-items:center;flex-wrap:wrap}.empty{text-align:center;color:#9ba8b4;padding:35px}.foot{color:#87939e;font-size:12px;text-align:center;padding:12px}@media(max-width:650px){.session{grid-template-columns:1fr}.actions .btn{flex:1}.bar{align-items:stretch;flex-direction:column}.head{align-items:flex-start}.status{text-align:left;margin-top:10px}}
</style></head><body><main class="wrap">
<div class="head"><div><div class="brand">RaceSync</div><div class="sub">Motorcycle Data Logger</div></div><div class="status"><div id="gps">GPS ...</div><div id="sd">Storage ...</div><div id="log">Logger ...</div></div></div>
<div class="bar"><div><strong>Stored Sessions</strong><div class="sub" id="summary">Loading...</div></div><button class="btn primary" id="all" onclick="downloadAllNew()">Download All New</button></div>
<div id="sessions"></div><div class="foot" id="footer">RaceSync</div></main>
<script>
const key='racesync_downloaded_sessions';let sessions=[];
function downloaded(){try{return new Set(JSON.parse(localStorage.getItem(key)||'[]').map(String))}catch(e){return new Set()}}
function mark(id){const s=downloaded();s.add(String(id));localStorage.setItem(key,JSON.stringify([...s]));render()}
function fmt(n){if(n==null)return '';if(n<1024)return n+' B';if(n<1048576)return (n/1024).toFixed(1)+' KB';return (n/1048576).toFixed(1)+' MB'}
function esc(s){return String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]))}
function label(file){return file.replace(/\.vbo$/i,'').replace(/^RS_/,'').replace(/_(\d{6})$/, ' $1')}
function render(){const seen=downloaded(),box=document.getElementById('sessions');const complete=sessions.filter(s=>s.complete);const fresh=complete.filter(s=>!seen.has(String(s.id)));document.getElementById('summary').textContent=complete.length+' sessions · '+fresh.length+' new';document.getElementById('all').disabled=!fresh.length;box.innerHTML=complete.length?'': '<div class="empty">No stored sessions</div>';[...sessions].reverse().forEach(s=>{const isNew=!seen.has(String(s.id))&&s.complete;const d=document.createElement('div');d.className='session';d.innerHTML='<div><div class="title">'+(isNew?'<span class="new">NEW</span>':'<span class="done">✓</span>')+esc(label(s.file))+'</div><div class="meta">'+esc(s.file)+' · '+fmt(s.sizeBytes)+(s.active?' · RECORDING':'')+'</div></div><div class="actions"><button class="btn primary">Download VBO</button>'+(s.hasKml?'<button class="btn ghost">Download KML</button>':'')+(s.deletable?'<button class="btn danger">Delete</button>':'')+'</div>';const b=d.querySelectorAll('button');let i=0;b[i++].onclick=()=>dl(s.downloadUrl,s.id);if(s.hasKml)b[i++].onclick=()=>dl(s.kmlDownloadUrl,s.id);if(s.deletable)b[i++].onclick=()=>del(s);box.appendChild(d)})}
function dl(url,id){const a=document.createElement('a');a.href=url;a.click();mark(id)}
async function del(s){if(!confirm('Delete '+s.file+' and its matching KML file?'))return;const r=await fetch(s.deleteUrl,{method:'DELETE'});if(!r.ok){alert('Unable to delete session');return}await load()}
function downloadAllNew(){const seen=downloaded();const fresh=sessions.filter(s=>s.complete&&!seen.has(String(s.id)));fresh.forEach((s,i)=>setTimeout(()=>dl(s.downloadUrl,s.id),i*600))}
async function load(){try{const [sr,st]=await Promise.all([fetch('/api/sessions',{cache:'no-store'}),fetch('/api/status',{cache:'no-store'})]);const sj=await sr.json(),x=await st.json();sessions=sj.sessions||[];document.getElementById('gps').innerHTML='<span class="'+(x.gps&&x.gps.validFix?'ok':'warn')+'">●</span> GPS '+(x.gps&&x.gps.validFix?'READY':'WAITING');document.getElementById('sd').innerHTML='<span class="'+(x.storage&&x.storage.ready?'ok':'warn')+'">●</span> '+(x.storage&&x.storage.ready?'STORAGE READY':'STORAGE ERROR');document.getElementById('log').textContent=x.logger&&x.logger.recording?'● RECORDING':'Logger idle';document.getElementById('footer').textContent=fmt(x.storage.freeBytes)+' free · '+(x.system.firmware||'RaceSync');render()}catch(e){document.getElementById('summary').textContent='Unable to contact RaceSync'}}
load();setInterval(load,5000);
</script></body></html>)HTML";
}

void RaceSyncApi::beginWebUiRoute()
{
    _server.on("/", HTTP_GET, [this]() {
        _server.sendHeader("Cache-Control", "no-store");
        _server.send_P(200, "text/html", RACESYNC_UI);
    });
}
