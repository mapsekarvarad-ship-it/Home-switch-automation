#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <ESPmDNS.h>

// Backup network: the board always creates its own WiFi; join it from the phone and open http://192.168.4.1
const char* AP_SSID = "HomeControl";
const char* AP_PASS = "homecontrol123";

// The board also joins the first of these WiFi networks (2.4 GHz, plain password only) that is in range.
// Its IP is printed on the serial monitor, and on networks that support it the page is at http://homecontrol.local
// A network with a fixed address lists ip, gateway and subnet mask; a network with nullptr uses DHCP.
struct KnownWifi {
  const char* ssid;
  const char* pass;
  const char* ip;
  const char* gateway;
  const char* mask;
};
const KnownWifi KNOWN_WIFI[] = {
  {"YOUR_WIFI_NAME", "YOUR_WIFI_PASSWORD", nullptr, nullptr, nullptr},
  {"YOUR_SECOND_WIFI_NAME", "YOUR_SECOND_WIFI_PASSWORD", nullptr, nullptr, nullptr},
};
const int NUM_KNOWN_WIFI = sizeof(KNOWN_WIFI) / sizeof(KNOWN_WIFI[0]);

struct Device {
  const char* name;
  const char* icon;         // "bell", "fan" or "power" (drawn by the page)
  int pin;
  bool on;
  unsigned long pulseMs;    // > 0 = momentary only: rings for this long, then switches itself off
  unsigned long offAt;      // millis() time to switch off after a ring; 0 = none pending
};

// Relay 1..4 on GPIO 4..7. The order here is also the order of the tiles on the page.
Device devices[] = {
  {"Bell",     "bell",  4, false, 1500, 0},
  {"Fan",      "fan",   5, false, 0, 0},
  {"Switch 3", "power", 6, false, 0, 0},
  {"Switch 4", "power", 7, false, 0, 0},
};
const int NUM_DEVICES = sizeof(devices) / sizeof(devices[0]);

const int RELAY_ON = LOW;    // this module is active-LOW; set to HIGH for active-HIGH modules
const int RELAY_OFF = HIGH;

WebServer server(80);

const char PAGE[] PROGMEM = R"HTML(<!DOCTYPE html>
<html lang="en"><head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,viewport-fit=cover">
<meta name="theme-color" content="#0e1424">
<title>Home Control</title>
<style>
:root{
  --bg1:#0e1424;--bg2:#161f3a;--card:rgba(255,255,255,.07);--line:rgba(255,255,255,.10);
  --text:#f2f5ff;--muted:#9aa6c4;--chip:rgba(255,255,255,.10);
  --on1:#2dd4a0;--on2:#1a9d78;--glow:rgba(45,212,160,.40);--accent1:#5b8cff;--accent2:#7a5bff;--danger:#ff5d6c;
}
@media(prefers-color-scheme:light){:root{
  --bg1:#eef2fb;--bg2:#dfe7f8;--card:rgba(255,255,255,.85);--line:rgba(20,30,70,.08);
  --text:#18203a;--muted:#667194;--chip:rgba(30,50,110,.08);--glow:rgba(26,157,120,.35);
}}
*{box-sizing:border-box;-webkit-tap-highlight-color:transparent}
body{margin:0;min-height:100vh;color:var(--text);background:linear-gradient(160deg,var(--bg1),var(--bg2)) fixed;
  font-family:system-ui,-apple-system,"Segoe UI",Roboto,sans-serif;padding:22px 16px 190px}
.wrap{max-width:520px;margin:0 auto}
header{display:flex;align-items:center;justify-content:space-between;margin-bottom:22px}
h1{font-size:26px;margin:0;letter-spacing:-.5px}
.sub{color:var(--muted);font-size:14px;margin-top:2px}
.alloff{border:1px solid var(--line);background:var(--card);color:var(--text);border-radius:14px;padding:10px 14px;font-size:14px;font-weight:600;cursor:pointer}
.alloff:active{transform:scale(.96)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:14px}
.tile{position:relative;min-height:158px;padding:16px;border-radius:24px;background:var(--card);border:1px solid var(--line);
  display:flex;flex-direction:column;justify-content:space-between;cursor:pointer;user-select:none;
  backdrop-filter:blur(8px);transition:transform .12s,background .25s,box-shadow .25s}
.tile:active{transform:scale(.97)}
.tile.on{background:linear-gradient(145deg,var(--on1),var(--on2));border-color:transparent;color:#fff;box-shadow:0 10px 28px var(--glow)}
.ico{width:58px;height:58px;border-radius:19px;background:var(--chip);display:flex;align-items:center;justify-content:center;transition:background .25s}
.tile.on .ico{background:rgba(255,255,255,.22)}
.ico svg{width:34px;height:34px;overflow:visible}
.name{font-size:18px;font-weight:650}
.st{font-size:13px;color:var(--muted);margin-top:2px}
.tile.on .st{color:rgba(255,255,255,.9)}
.pill{position:absolute;top:18px;right:16px;width:42px;height:24px;border-radius:12px;background:var(--chip);transition:background .25s}
.pill::after{content:"";position:absolute;top:3px;left:3px;width:18px;height:18px;border-radius:50%;background:#fff;box-shadow:0 1px 3px rgba(0,0,0,.35);transition:transform .25s}
.tile.on .pill{background:rgba(255,255,255,.35)}
.tile.on .pill::after{transform:translateX(18px)}
.tag{position:absolute;top:20px;right:16px;font-size:11px;font-weight:700;letter-spacing:.4px;padding:4px 8px;border-radius:8px;background:var(--chip);color:var(--muted)}
.tile.on .tag{background:rgba(255,255,255,.25);color:#fff}
@keyframes spin{to{transform:rotate(360deg)}}
@keyframes shake{0%,100%{transform:rotate(0)}25%{transform:rotate(16deg)}75%{transform:rotate(-16deg)}}
.tile.on .fan{animation:spin .8s linear infinite;transform-origin:12px 12px}
.tile.on .bell{animation:shake .28s ease-in-out infinite;transform-origin:12px 4px}
details{margin-top:18px;color:var(--muted);font-size:13px}
summary{cursor:pointer;padding:6px 0}
input{width:100%;margin-top:8px;padding:13px 15px;border-radius:14px;border:1px solid var(--line);background:var(--card);color:var(--text);font-size:15px;outline:none}
input:focus{border-color:var(--accent1)}
.help{margin-top:8px;line-height:1.5}
.dock{position:fixed;left:0;right:0;bottom:0;padding:14px 16px calc(14px + env(safe-area-inset-bottom));
  background:linear-gradient(to top,var(--bg1) 55%,transparent);text-align:center}
.say{min-height:20px;font-size:13px;color:var(--muted);margin-bottom:10px}
.mic{width:100%;max-width:520px;border:0;border-radius:20px;padding:17px;font-size:17px;font-weight:650;color:#fff;cursor:pointer;
  background:linear-gradient(135deg,var(--accent1),var(--accent2));box-shadow:0 8px 24px rgba(91,140,255,.40);
  display:inline-flex;align-items:center;justify-content:center;gap:10px}
.mic:active{transform:scale(.98)}
.mic svg{width:22px;height:22px}
.mic.live{background:linear-gradient(135deg,var(--danger),#ff8a5b);animation:pulse 1.2s ease-in-out infinite}
@keyframes pulse{50%{box-shadow:0 8px 34px rgba(255,93,108,.65)}}
.net{text-align:center;font-size:12px;color:var(--danger);margin-top:14px;min-height:14px}
</style></head><body><div class="wrap">
<header>
  <div><h1>Home Control</h1><div class="sub" id="sub">Connecting...</div></div>
  <button class="alloff" onclick="allOff()">All off</button>
</header>
<div class="grid" id="grid"></div>
<div class="net" id="net"></div>
<details><summary>Type a command</summary>
  <input id="cmd" placeholder='e.g. "fan on" or "bell"' enterkeyhint="send">
  <div class="help">Voice needs Chrome and microphone permission. Chrome blocks the mic on plain http pages: open chrome://flags/#unsafely-treat-insecure-origin-as-secure, add http://192.168.4.1, set Enabled and relaunch. Recognition also needs internet or an offline English language pack. Typing always works.</div>
</details>
</div>
<div class="dock">
  <div class="say" id="say">Say "bell", "fan on", "switch 3 off" or "everything off"</div>
  <button class="mic" id="mic" onclick="listen()">
    <svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><rect x="9" y="3" width="6" height="11" rx="3"/><path d="M5 11a7 7 0 0 0 14 0M12 18v3"/></svg>
    <span id="miclabel">Tap and speak</span>
  </button>
</div>
<script>
const ICONS={
  bell:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round"><g class="bell"><path d="M12 3.5a6 6 0 0 0-6 6v3.2L4.2 16h15.6L18 12.7V9.5a6 6 0 0 0-6-6z"/><path d="M10 19.2a2 2 0 0 0 4 0"/></g></svg>',
  fan:'<svg viewBox="0 0 24 24" fill="currentColor"><g class="fan"><ellipse cx="12" cy="6.4" rx="2.7" ry="4.4"/><ellipse cx="12" cy="6.4" rx="2.7" ry="4.4" transform="rotate(120 12 12)"/><ellipse cx="12" cy="6.4" rx="2.7" ry="4.4" transform="rotate(240 12 12)"/><circle cx="12" cy="12" r="2" fill="#fff" fill-opacity=".85"/></g></svg>',
  power:'<svg viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 3v8"/><path d="M6.6 6.8a8 8 0 1 0 10.8 0"/></svg>'
};
let devs=[];
const grid=document.getElementById('grid'),say=document.getElementById('say'),mic=document.getElementById('mic');
function buzz(){try{navigator.vibrate&&navigator.vibrate(15);}catch(e){}}
function draw(){
  grid.innerHTML='';
  devs.forEach((d,i)=>{
    const m=d.pulse>0;
    const t=document.createElement('div');
    t.className='tile'+(d.on?' on':'');
    t.innerHTML=(m?'<div class="tag">'+(d.pulse/1000)+' SEC</div>':'<div class="pill"></div>')+
      '<div class="ico">'+(ICONS[d.icon]||ICONS.power)+'</div>'+
      '<div><div class="name">'+d.name+'</div><div class="st">'+(m?(d.on?'Ringing...':'Tap to ring'):(d.on?'On':'Off'))+'</div></div>';
    t.onclick=()=>{buzz();m?ring(i):set(i,!d.on);};
    grid.appendChild(t);
  });
  const n=devs.filter(d=>d.on).length;
  document.getElementById('sub').textContent=n?n+' active':'Everything is off';
}
async function api(url){
  try{
    const r=await fetch(url);devs=await r.json();draw();
    document.getElementById('net').textContent='';
  }catch(e){
    document.getElementById('net').textContent='Connection lost, retrying...';
  }
}
function set(i,s){api('/api/set?id='+i+'&state='+(s?1:0));}
function ring(i){api('/api/ring?id='+i);setTimeout(()=>api('/api/state'),devs[i].pulse+400);}
function allOff(){buzz();api('/api/alloff');}

const P='(?:switch|number|button|relay)\\s*';
function find(t){
  if(/\bbell\b|\bring\b/.test(t)||new RegExp('\\b'+P+'(1|one)\\b').test(t))return 0;
  if(/\bfan\b/.test(t)||new RegExp('\\b'+P+'(2|two|to|too)\\b').test(t))return 1;
  if(new RegExp('\\b'+P+'(3|three)\\b').test(t))return 2;
  if(new RegExp('\\b'+P+'(4|four|for)\\b').test(t))return 3;
  return -1;
}
function tryCmd(raw){
  const t=raw.toLowerCase();
  if(/\b(all|everything)\b/.test(t)&&/\b(off|stop)\b/.test(t)){allOff();return true;}
  const i=find(t);if(i<0||!devs[i])return false;
  const d=devs[i],off=/\b(off|stop)\b/.test(t),on=/\b(on|start)\b/.test(t);
  if(d.pulse>0){if(off)set(i,false);else ring(i);return true;}
  set(i,off?false:on?true:!d.on);return true;
}
function handle(alts){
  say.textContent='Heard: "'+alts[0]+'"';
  if(!alts.some(tryCmd))say.textContent+=' - not understood';
}
document.getElementById('cmd').addEventListener('keydown',e=>{
  if(e.key==='Enter'&&e.target.value.trim()){handle([e.target.value.trim()]);e.target.value='';}
});
const SR=window.SpeechRecognition||window.webkitSpeechRecognition;
function errMsg(e){
  if(e==='not-allowed'||e==='service-not-allowed')return 'Microphone blocked. See "Type a command" below for the fix.';
  if(e==='network')return 'Speech recognition needs internet or an offline language pack. Type a command instead.';
  if(e==='no-speech')return 'Did not hear anything. Tap and try again.';
  return 'Voice error: '+e;
}
function listen(){
  if(!SR){say.textContent='Voice is not supported in this browser. Use Chrome, or type a command.';return;}
  const rec=new SR();
  rec.lang='en-US';rec.interimResults=false;rec.maxAlternatives=3;
  rec.onstart=()=>{mic.classList.add('live');document.getElementById('miclabel').textContent='Listening...';say.textContent='Speak now';};
  rec.onend=()=>{mic.classList.remove('live');document.getElementById('miclabel').textContent='Tap and speak';};
  rec.onerror=e=>{say.textContent=errMsg(e.error);};
  rec.onresult=e=>handle([...e.results[0]].map(a=>a.transcript));
  try{rec.start();}catch(e){say.textContent=errMsg(String(e));}
}
api('/api/state');
setInterval(()=>api('/api/state'),3000);
</script></body></html>)HTML";

void apply(int i) {
  digitalWrite(devices[i].pin, devices[i].on ? RELAY_ON : RELAY_OFF);
}

void sendState() {
  String json = "[";
  for (int i = 0; i < NUM_DEVICES; i++) {
    if (i) json += ",";
    json += "{\"name\":\"" + String(devices[i].name) + "\",\"icon\":\"" + String(devices[i].icon) +
            "\",\"on\":" + (devices[i].on ? "true" : "false") + ",\"pulse\":" + String(devices[i].pulseMs) + "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}

void handleRoot() {
  server.send_P(200, "text/html", PAGE);
}

void handleSet() {
  int id = server.arg("id").toInt();
  bool state = server.arg("state").toInt() != 0;
  if (id >= 0 && id < NUM_DEVICES && server.hasArg("state")) {
    // A momentary device (the bell) can be switched off but never held on.
    if (!(devices[id].pulseMs > 0 && state)) {
      devices[id].on = state;
      devices[id].offAt = 0;
      apply(id);
    }
  }
  sendState();
}

void handleRing() {
  int id = server.arg("id").toInt();
  if (id >= 0 && id < NUM_DEVICES && devices[id].pulseMs > 0) {
    devices[id].on = true;
    devices[id].offAt = millis() + devices[id].pulseMs;
    apply(id);
  }
  sendState();
}

void handleAllOff() {
  for (int i = 0; i < NUM_DEVICES; i++) {
    devices[i].on = false;
    devices[i].offAt = 0;
    apply(i);
  }
  sendState();
}

// While not connected, tries the known networks one after another, 15 seconds each,
// until one of them accepts the board.
void maintainWifi() {
  static unsigned long lastTry = 0;
  static int next = 0;

  if (WiFi.status() == WL_CONNECTED) return;
  if (lastTry != 0 && millis() - lastTry < 15000) return;
  lastTry = millis();

  Serial.println("Trying " + String(KNOWN_WIFI[next].ssid));
  WiFi.disconnect(false);
  const KnownWifi& net = KNOWN_WIFI[next];
  IPAddress ip, gateway, mask;
  if (net.ip && ip.fromString(net.ip) && gateway.fromString(net.gateway) && mask.fromString(net.mask)) {
    WiFi.config(ip, gateway, mask, gateway);             // fixed address (DNS via the router)
  } else {
    WiFi.config(IPAddress(), IPAddress(), IPAddress());  // back to DHCP
  }
  WiFi.begin(net.ssid, net.pass);
  next = (next + 1) % NUM_KNOWN_WIFI;
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < NUM_DEVICES; i++) {
    pinMode(devices[i].pin, OUTPUT);
    apply(i);
  }

  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(AP_SSID, AP_PASS);
  Serial.println("\nBackup WiFi: " + String(AP_SSID) + "  IP: " + WiFi.softAPIP().toString());
  WiFi.setAutoReconnect(true);
  WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) {
    Serial.printf("WiFi join failed, reason code %d\n", info.wifi_sta_disconnected.reason);
  }, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);

  server.on("/", handleRoot);
  server.on("/api/state", sendState);
  server.on("/api/set", handleSet);
  server.on("/api/alloff", handleAllOff);
  server.on("/api/ring", handleRing);
  server.begin();
}

void loop() {
  server.handleClient();
  maintainWifi();

  static bool wifiUp = false;
  bool up = WiFi.status() == WL_CONNECTED;
  if (up && !wifiUp) {
    Serial.println("WiFi connected: " + WiFi.SSID() + "  IP: " + WiFi.localIP().toString());
    if (MDNS.begin("homecontrol")) MDNS.addService("http", "tcp", 80);
  } else if (!up && wifiUp) {
    Serial.println("WiFi lost, searching again");
  }
  wifiUp = up;

  for (int i = 0; i < NUM_DEVICES; i++) {
    if (devices[i].offAt && (long)(millis() - devices[i].offAt) >= 0) {
      devices[i].on = false;
      devices[i].offAt = 0;
      apply(i);
    }
  }
}
