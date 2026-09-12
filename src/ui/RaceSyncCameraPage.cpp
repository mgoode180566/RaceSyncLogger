#include "RaceSyncUiPages.h"

const char RACESYNC_CAMERA_UI[] PROGMEM = R"HTML(<!doctype html><html><head><meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1"><title>RaceSync Camera</title><style>:root{font-family:system-ui,-apple-system,sans-serif;color:#f4f6f8;background:#101418}body{margin:0}.wrap{max-width:820px;margin:auto;padding:18px}.panel{background:#192027;border:1px solid #2a343d;border-radius:12px;padding:16px;margin-bottom:12px}.btn{border:0;border-radius:8px;padding:10px 14px;margin:4px;font-weight:700;background:#2f7d4a;color:#fff}.btn.alt{background:#a33b3b}.btn.dim{background:#3c4852}.btn:disabled{opacity:.45}.nav{color:#fff;text-decoration:none;margin-right:12px}.warn{color:#ffd479}.ok{color:#8ee2a8}.diag{font-size:.9rem;color:#aab8c2}pre{white-space:pre-wrap;word-break:break-word;color:#dbe7ef}</style></head><body><main class="wrap"><div class="panel"><h2>GoPro HERO9 Bluetooth</h2><a class="nav" href="/">Sessions</a><a class="nav" href="/status">Status</a><a class="nav" href="/control">Control</a></div><div class="panel"><p class="warn">Bench-test feature: Bluetooth is off at boot and is never connected automatically. Disconnect the camera before riding.</p><button type="button" class="btn" id="connect">Enable Bluetooth &amp; Connect</button><button type="button" class="btn dim" id="refresh">Refresh status</button><button type="button" class="btn alt" id="disconnect">Disconnect</button><p id="notice">Loading…</p><p class="diag" id="clientDiag">Client ready; no camera command sent yet.</p><pre id="status"></pre></div></main><script>
let busy=false;
let commandCount=0;
const connectButton=document.getElementById('connect');
const refreshButton=document.getElementById('refresh');
const disconnectButton=document.getElementById('disconnect');
const notice=document.getElementById('notice');
const clientDiag=document.getElementById('clientDiag');

async function load(){
  try{
    const r=await fetch('/api/camera',{cache:'no-store'});
    const j=await r.json();
    show(j,r.ok);
  }catch(e){
    notice.textContent='RaceSync unavailable';
    clientDiag.textContent='Status request failed: '+String(e);
  }
}

function show(j,ok){
  document.getElementById('status').textContent=JSON.stringify(j,null,2);
  notice.textContent=j.commandsAllowed===false?'Camera controls disabled while logging':(j.connected?'GoPro connected':j.state||'Disconnected');
  notice.className=j.connected?'ok':'warn';
  connectButton.disabled=busy||j.commandsAllowed===false||j.connected;
  refreshButton.disabled=busy||j.commandsAllowed===false||!j.connected;
  disconnectButton.disabled=busy||j.commandsAllowed===false||!j.connected;
}

async function sendCommand(action){
  commandCount++;
  clientDiag.textContent='Click '+commandCount+': sending POST /api/camera/'+action;
  console.log('[CAMERA-UI] sending POST /api/camera/'+action);
  busy=true;
  document.querySelectorAll('button').forEach(b=>b.disabled=true);
  notice.textContent=action==='connect'?'Scanning for GoPro…':'Working…';
  try{
    const r=await fetch('/api/camera/'+action,{method:'POST',cache:'no-store'});
    clientDiag.textContent='POST /api/camera/'+action+' returned HTTP '+r.status;
    console.log('[CAMERA-UI] POST returned',r.status);
    const j=await r.json();
    show(j,r.ok);
  }catch(e){
    notice.textContent='Command failed';
    clientDiag.textContent='POST failed: '+String(e);
    console.error('[CAMERA-UI] command failed',e);
  }
  busy=false;
  setTimeout(load,500);
}

connectButton.addEventListener('click',()=>sendCommand('connect'));
refreshButton.addEventListener('click',()=>sendCommand('refresh'));
disconnectButton.addEventListener('click',()=>sendCommand('disconnect'));

load();
setInterval(()=>{if(!busy)load()},2000);
</script></body></html>)HTML";
