/*
  Water Quality Monitoring - LoRa Receiver + Captive Portal Dashboard
  ESP32 + SX127x (433 MHz)

  1. Upload this sketch.
  2. On your phone/laptop connect to the Wi-Fi  "WaterQuality-Monitor".
  3. The dashboard opens automatically (captive portal). If not, open http://192.168.4.1
*/

#include <SPI.h>
#include <LoRa.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

#define DEVICE_NAME "Receiver"

#define LORA_SCK  18
#define LORA_MISO 19
#define LORA_MOSI 23
#define LORA_SS   5
#define LORA_RST  14
#define LORA_DIO0 4
#define LED_PIN   15
#define LORA_FREQUENCY 433E6

// ---------- Wi-Fi access point ----------
const char *AP_SSID = "WaterQuality-Monitor";
const char *AP_PASS = "";            // empty = open network (best for captive portal)
const IPAddress AP_IP(192, 168, 4, 1);

DNSServer dnsServer;
WebServer server(80);

// ---------- Data storage ----------
#define HISTORY 60                    // last 60 packets kept for the graphs
struct Sample { unsigned long ts; unsigned long id; float t, tds, ntu; };
Sample hist[HISTORY];
int histCount = 0, histHead = 0;

String inputBuffer = "";
long lastId = -1;
unsigned long received = 0, lost = 0;
int lastRssi = 0;
float lastSnr = 0;

// ---------- Dashboard page (stored in flash) ----------
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Water Quality Monitoring</title>
<style>
:root{
  --bg:#0a1020;--card:#111a2f;--card2:#0e1628;--line:#1e2b47;--text:#e8eefb;--muted:#8a9bbd;
  --temp:#fb923c;--tds:#38bdf8;--ntu:#a78bfa;--good:#34d399;--warn:#fbbf24;--bad:#f87171;
}
*{box-sizing:border-box;margin:0;padding:0}
body{font-family:-apple-system,"Segoe UI",Roboto,Helvetica,Arial,sans-serif;background:
  radial-gradient(1200px 500px at 15% -10%,#12315a55,transparent),
  radial-gradient(900px 500px at 100% 0,#0e4a6a44,transparent),var(--bg);
  color:var(--text);min-height:100vh;-webkit-font-smoothing:antialiased}
.wrap{max-width:1180px;margin:0 auto;padding:22px 18px 40px}
header{display:flex;align-items:center;justify-content:space-between;gap:16px;flex-wrap:wrap;margin-bottom:22px}
.brand{display:flex;align-items:center;gap:14px}
.logo{width:48px;height:48px;border-radius:14px;background:linear-gradient(135deg,#0ea5e9,#22d3ee);display:grid;place-items:center;box-shadow:0 8px 24px #0ea5e944}
.brand h1{font-size:1.45rem;font-weight:700;letter-spacing:.2px}
.brand p{color:var(--muted);font-size:.85rem;margin-top:2px}
.pill{display:flex;align-items:center;gap:8px;padding:8px 14px;border-radius:999px;background:var(--card);border:1px solid var(--line);font-size:.85rem;font-weight:600}
.dot{width:9px;height:9px;border-radius:50%;background:var(--muted)}
.pill.on .dot{background:var(--good);box-shadow:0 0 0 0 #34d39988;animation:pulse 1.8s infinite}
.pill.off .dot{background:var(--bad)}
@keyframes pulse{70%{box-shadow:0 0 0 9px #34d39900}100%{box-shadow:0 0 0 0 #34d39900}}

.panel{background:linear-gradient(180deg,var(--card),var(--card2));border:1px solid var(--line);border-radius:18px;padding:18px}
.node{display:flex;align-items:center;gap:18px;flex-wrap:wrap;justify-content:space-between;margin-bottom:18px}
.node .title{display:flex;align-items:center;gap:12px}
.node .badge{background:#0ea5e922;color:#7dd3fc;border:1px solid #0ea5e944;padding:6px 12px;border-radius:10px;font-weight:700;font-size:.9rem}
.node .sub{color:var(--muted);font-size:.85rem}
.overall{padding:7px 14px;border-radius:10px;font-weight:700;font-size:.85rem}
.stats{display:grid;grid-template-columns:repeat(auto-fit,minmax(130px,1fr));gap:12px;width:100%}
.stat{background:#0b1325;border:1px solid var(--line);border-radius:12px;padding:12px 14px}
.stat small{display:block;color:var(--muted);font-size:.72rem;text-transform:uppercase;letter-spacing:.8px;margin-bottom:4px}
.stat b{font-size:1.1rem}

.grid3{display:grid;grid-template-columns:repeat(auto-fit,minmax(340px,1fr));gap:18px}
.card .top{display:flex;justify-content:space-between;align-items:flex-start;margin-bottom:6px}
.card h3{font-size:.8rem;text-transform:uppercase;letter-spacing:1px;color:var(--muted);font-weight:600}
.card .val{font-size:2.5rem;font-weight:700;line-height:1.1;margin-top:6px}
.card .val span{font-size:1rem;color:var(--muted);font-weight:500;margin-left:5px}
.tag{padding:5px 11px;border-radius:999px;font-size:.75rem;font-weight:700}
.good{background:#34d39922;color:var(--good)}.warn{background:#fbbf2422;color:var(--warn)}.bad{background:#f8717122;color:var(--bad)}.na{background:#8a9bbd22;color:var(--muted)}
.chart{position:relative;margin-top:10px}
.chart svg{width:100%;height:auto;display:block}
.grid{stroke:#ffffff10;stroke-width:1}
.ax{fill:#7586a8;font-size:11px}
.tip{position:absolute;pointer-events:none;background:#050a16ee;border:1px solid var(--line);padding:5px 9px;border-radius:8px;font-size:.75rem;display:none;white-space:nowrap;transform:translate(-50%,-120%)}
.empty{height:200px;display:grid;place-items:center;color:var(--muted);font-size:.9rem}
.meta{display:flex;justify-content:space-between;color:var(--muted);font-size:.75rem;margin-top:8px}

.bottom{display:grid;grid-template-columns:1fr;gap:18px;margin-top:18px}
.ctrl{display:flex;align-items:center;justify-content:space-between;flex-wrap:wrap;gap:12px}
.ctrl h3{font-size:.95rem}.ctrl p{color:var(--muted);font-size:.82rem;margin-top:2px}
.btns{display:flex;gap:10px;flex-wrap:wrap}
button{font:inherit;font-weight:600;font-size:.85rem;padding:10px 18px;border-radius:10px;border:1px solid var(--line);background:#16223c;color:var(--text);cursor:pointer;transition:.15s}
button:hover{background:#1d2c4d;border-color:#2c4170}
button.primary{background:linear-gradient(135deg,#0ea5e9,#06b6d4);border:none;color:#04121f}
footer{text-align:center;color:var(--muted);font-size:.78rem;margin-top:26px}
.toast{position:fixed;bottom:22px;left:50%;transform:translateX(-50%) translateY(80px);background:#16223c;border:1px solid var(--line);padding:10px 18px;border-radius:10px;font-size:.85rem;transition:.3s;opacity:0}
.toast.show{transform:translateX(-50%) translateY(0);opacity:1}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div class="brand">
      <div class="logo">
        <svg width="26" height="26" viewBox="0 0 24 24" fill="none" stroke="#04121f" stroke-width="2" stroke-linecap="round" stroke-linejoin="round"><path d="M12 2.7s6 6.2 6 10.8a6 6 0 0 1-12 0C6 8.9 12 2.7 12 2.7z"/><path d="M9.5 14a2.6 2.6 0 0 0 2.5 2.4"/></svg>
      </div>
      <div>
        <h1>Water Quality Monitoring</h1>
        <p>LoRa Wireless Sensor Network &middot; Real-time Dashboard</p>
      </div>
    </div>
    <div id="pill" class="pill off"><span class="dot"></span><span id="pillTxt">Waiting for Node 1</span></div>
  </header>

  <section class="panel node">
    <div class="title" style="width:100%;justify-content:space-between;flex-wrap:wrap">
      <div class="title">
        <span class="badge">NODE 1</span>
        <div><b>Water Sensor Station</b><div class="sub" id="lastSeen">No data received yet</div></div>
      </div>
      <span id="overall" class="overall na">Water quality: --</span>
    </div>
    <div class="stats">
      <div class="stat"><small>Packet ID</small><b id="sId">--</b></div>
      <div class="stat"><small>Received</small><b id="sRx">--</b></div>
      <div class="stat"><small>Lost</small><b id="sLost">--</b></div>
      <div class="stat"><small>Delivery Rate</small><b id="sPdr">--</b></div>
      <div class="stat"><small>RSSI</small><b id="sRssi">--</b></div>
      <div class="stat"><small>SNR</small><b id="sSnr">--</b></div>
    </div>
  </section>

  <div style="height:18px"></div>

  <section class="grid3">
    <div class="panel card">
      <div class="top"><div><h3>Temperature</h3><div class="val" id="vT">--<span>&deg;C</span></div></div><span class="tag na" id="gT">--</span></div>
      <div class="chart" id="cT"></div>
      <div class="meta"><span>Node 1 &middot; DS18B20</span><span>Last 60 packets</span></div>
    </div>
    <div class="panel card">
      <div class="top"><div><h3>Total Dissolved Solids</h3><div class="val" id="vD">--<span>ppm</span></div></div><span class="tag na" id="gD">--</span></div>
      <div class="chart" id="cD"></div>
      <div class="meta"><span>Node 1 &middot; TDS sensor</span><span>Last 60 packets</span></div>
    </div>
    <div class="panel card">
      <div class="top"><div><h3>Turbidity</h3><div class="val" id="vN">--<span>NTU</span></div></div><span class="tag na" id="gN">--</span></div>
      <div class="chart" id="cN"></div>
      <div class="meta"><span>Node 1 &middot; Turbidity sensor</span><span>Last 60 packets</span></div>
    </div>
  </section>

  <section class="bottom">
    <div class="panel ctrl">
      <div><h3>Node 1 Remote Control</h3><p>Send a command to the node over LoRa (status LED)</p></div>
      <div class="btns">
        <button class="primary" onclick="cmd('ON')">LED ON</button>
        <button onclick="cmd('OFF')">LED OFF</button>
        <button onclick="cmd('TOGGLE')">Toggle</button>
        <button onclick="csv()">Download CSV</button>
      </div>
    </div>
  </section>

  <footer>Final Year Project &middot; Water Quality Monitoring using LoRa &middot; ESP32</footer>
</div>
<div class="toast" id="toast"></div>

<script>
const $=id=>document.getElementById(id);
let rowsCache=[];
const C={t:{ci:1,box:'cT',val:'vT',tag:'gT',color:'#fb923c',dec:1,unit:'\u00b0C'},
         d:{ci:2,box:'cD',val:'vD',tag:'gD',color:'#38bdf8',dec:0,unit:'ppm'},
         n:{ci:3,box:'cN',val:'vN',tag:'gN',color:'#a78bfa',dec:0,unit:'NTU'}};

function fmtAge(s){s=Math.round(s);if(s<60)return s+'s';if(s<3600)return Math.round(s/60)+'m';return (s/3600).toFixed(1)+'h'}

function gradeT(v){if(v===null)return['Sensor error','bad',2];if(v<15)return['Cool','good',0];if(v<=30)return['Normal','good',0];if(v<=35)return['Warm','warn',1];return['High','bad',2]}
function gradeD(v){if(v<300)return['Excellent','good',0];if(v<600)return['Good','good',0];if(v<900)return['Fair','warn',1];if(v<1200)return['Poor','bad',2];return['Unacceptable','bad',2]}
function gradeN(v){if(v<=1)return['Clear','good',0];if(v<=5)return['Acceptable','good',0];if(v<=50)return['Cloudy','warn',1];return['Very turbid','bad',2]}

function draw(key,rows){
  const c=C[key],box=$(c.box),W=640,H=230,L=46,R=14,T=14,B=28,pw=W-L-R,ph=H-T-B;
  const pts=rows.map(r=>({a:r[0],y:r[c.ci]}));
  const ys=pts.filter(p=>p.y!==null).map(p=>p.y);
  if(!ys.length){box.innerHTML='<div class="empty">No data yet</div>';return}
  let mn=Math.min(...ys),mx=Math.max(...ys);
  if(mx-mn<1e-6){mn-=1;mx+=1}
  const pad=(mx-mn)*.18;mn-=pad;mx+=pad;if(key!=='t')mn=Math.max(0,mn);
  const amax=Math.max(...pts.map(p=>p.a),1);
  const X=a=>L+pw*(1-a/amax),Y=y=>T+ph*(1-(y-mn)/(mx-mn));
  let g='';
  for(let i=0;i<=4;i++){const v=mn+(mx-mn)*i/4,y=Y(v);
    g+=`<line x1="${L}" x2="${W-R}" y1="${y}" y2="${y}" class="grid"/><text x="${L-8}" y="${y+4}" text-anchor="end" class="ax">${v.toFixed(c.dec)}</text>`}
  const segs=[];let cur=[];
  pts.forEach(p=>{if(p.y===null){if(cur.length)segs.push(cur);cur=[]}else cur.push(p)});
  if(cur.length)segs.push(cur);
  const base=T+ph;let lines='',areas='';
  segs.forEach(s=>{
    if(s.length<2)return;
    const d=s.map((p,i)=>(i?'L':'M')+X(p.a).toFixed(1)+','+Y(p.y).toFixed(1)).join(' ');
    lines+=`<path d="${d}" fill="none" stroke="${c.color}" stroke-width="2.4" stroke-linejoin="round" stroke-linecap="round"/>`;
    areas+=`<path d="${d} L${X(s[s.length-1].a).toFixed(1)},${base} L${X(s[0].a).toFixed(1)},${base}Z" fill="url(#g${key})"/>`;
  });
  const lp=[...pts].reverse().find(p=>p.y!==null);
  const dots=`<circle cx="${X(lp.a)}" cy="${Y(lp.y)}" r="4.5" fill="${c.color}" stroke="#0a1020" stroke-width="2"/>`;
  const xl=`<text x="${L}" y="${H-7}" class="ax">-${fmtAge(amax)}</text><text x="${L+pw/2}" y="${H-7}" text-anchor="middle" class="ax">-${fmtAge(amax/2)}</text><text x="${W-R}" y="${H-7}" text-anchor="end" class="ax">now</text>`;
  box.innerHTML=`<svg viewBox="0 0 ${W} ${H}"><defs><linearGradient id="g${key}" x1="0" y1="0" x2="0" y2="1"><stop offset="0" stop-color="${c.color}" stop-opacity=".38"/><stop offset="1" stop-color="${c.color}" stop-opacity="0"/></linearGradient></defs>${g}${areas}${lines}${dots}${xl}<line id="hl${key}" y1="${T}" y2="${base}" stroke="#ffffff33" style="display:none"/></svg><div class="tip" id="tp${key}"></div>`;
  box.onpointermove=e=>{
    const r=box.getBoundingClientRect(),fx=(e.clientX-r.left)/r.width*W;
    let best=null,bd=1e9;
    pts.forEach(p=>{if(p.y===null)return;const d=Math.abs(X(p.a)-fx);if(d<bd){bd=d;best=p}});
    if(!best)return;
    const tip=$('tp'+key),hl=$('hl'+key);
    tip.style.display='block';tip.style.left=(X(best.a)/W*r.width)+'px';tip.style.top=(Y(best.y)/H*r.height)+'px';
    tip.textContent=best.y.toFixed(c.dec)+' '+c.unit+' \u00b7 '+(best.a<1?'now':fmtAge(best.a)+' ago');
    hl.setAttribute('x1',X(best.a));hl.setAttribute('x2',X(best.a));hl.style.display='block';
  };
  box.onpointerleave=()=>{$('tp'+key).style.display='none';$('hl'+key).style.display='none'};
}

function setTag(id,g){const e=$(id);e.textContent=g[0];e.className='tag '+g[1]}

function render(d){
  const rows=d.h;rowsCache=rows;
  const last=rows[rows.length-1];
  const online=last&&last[0]<25;
  $('pill').className='pill '+(online?'on':'off');
  $('pillTxt').textContent=online?'Node 1 Online':(last?'Node 1 Offline':'Waiting for Node 1');
  $('sRx').textContent=d.received;$('sLost').textContent=d.lost;
  const tot=d.received+d.lost;
  $('sPdr').textContent=tot?(d.received/tot*100).toFixed(1)+'%':'--';
  $('sRssi').textContent=d.received?d.rssi+' dBm':'--';
  $('sSnr').textContent=d.received?d.snr+' dB':'--';
  if(!last){return}
  $('sId').textContent='#'+last[4];
  $('lastSeen').textContent='Last update '+(last[0]<2?'just now':fmtAge(last[0])+' ago');
  const gt=gradeT(last[1]),gd=gradeD(last[2]),gn=gradeN(last[3]);
  $('vT').innerHTML=(last[1]===null?'--':last[1].toFixed(1))+'<span>\u00b0C</span>';
  $('vD').innerHTML=last[2].toFixed(0)+'<span>ppm</span>';
  $('vN').innerHTML=last[3].toFixed(0)+'<span>NTU</span>';
  setTag('gT',gt);setTag('gD',gd);setTag('gN',gn);
  const worst=Math.max(gd[2],gn[2]);
  const ov=$('overall');
  ov.textContent='Water quality: '+['Safe','Moderate','Poor'][worst];
  ov.className='overall '+['good','warn','bad'][worst];
  draw('t',rows);draw('d',rows);draw('n',rows);
}

async function poll(){
  try{const r=await fetch('/data',{cache:'no-store'});render(await r.json())}catch(e){$('pill').className='pill off';$('pillTxt').textContent='Portal disconnected'}
}
function toast(m){const t=$('toast');t.textContent=m;t.classList.add('show');setTimeout(()=>t.classList.remove('show'),2000)}
async function cmd(c){try{await fetch('/cmd?c='+c);toast('Command sent: '+c)}catch(e){toast('Failed to send')}}
function csv(){
  if(!rowsCache.length){toast('No data yet');return}
  let s='node,seconds_ago,temperature_C,tds_ppm,turbidity_NTU,packet_id\n';
  rowsCache.forEach(r=>{s+=`1,${r[0]},${r[1]===null?'':r[1]},${r[2]},${r[3]},${r[4]}\n`});
  const a=document.createElement('a');a.href=URL.createObjectURL(new Blob([s],{type:'text/csv'}));a.download='water_quality_node1.csv';a.click();
}
poll();setInterval(poll,2000);
</script>
</body>
</html>
)rawliteral";

// ---------- Web handlers ----------
void sendMessage(const String &msg) {
  LoRa.idle();
  LoRa.beginPacket();
  LoRa.print(msg);
  LoRa.endPacket();
  LoRa.receive();
  Serial.print("[Sent] ");
  Serial.println(msg);
}

void handleRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleData() {
  unsigned long now = millis();
  String j;
  j.reserve(4096);
  j += "{\"node\":1,\"received\":" + String(received);
  j += ",\"lost\":" + String(lost);
  j += ",\"rssi\":" + String(lastRssi);
  j += ",\"snr\":" + String(lastSnr, 1);
  j += ",\"h\":[";
  int start = (histHead - histCount + HISTORY) % HISTORY;
  for (int i = 0; i < histCount; i++) {
    const Sample &s = hist[(start + i) % HISTORY];
    if (i) j += ",";
    j += "[" + String((now - s.ts) / 1000.0, 1) + ",";
    j += (s.t < -100) ? String("null") : String(s.t, 2);
    j += "," + String(s.tds, 0) + "," + String(s.ntu, 0) + "," + String(s.id) + "]";
  }
  j += "]}";
  server.sendHeader("Cache-Control", "no-store");
  server.send(200, "application/json", j);
}

void handleCmd() {
  String c = server.arg("c");
  c.toUpperCase();
  if (c == "ON" || c == "OFF" || c == "TOGGLE") {
    sendMessage(c);
    server.send(200, "text/plain", "OK");
  } else {
    server.send(400, "text/plain", "Bad command");
  }
}

// Any unknown URL (Android/iOS/Windows connectivity checks) -> dashboard
void handleNotFound() {
  server.sendHeader("Location", String("http://") + AP_IP.toString() + "/", true);
  server.send(302, "text/plain", "");
}

void storeSample(unsigned long id, float t, float tds, float ntu) {
  hist[histHead] = { millis(), id, t, tds, ntu };
  histHead = (histHead + 1) % HISTORY;
  if (histCount < HISTORY) histCount++;
}

void setup() {
  Serial.begin(115200);
  delay(1000);

  pinMode(LED_PIN, OUTPUT);
  Serial.println(DEVICE_NAME);

  SPI.begin(LORA_SCK, LORA_MISO, LORA_MOSI, LORA_SS);
  LoRa.setPins(LORA_SS, LORA_RST, LORA_DIO0);

  if (!LoRa.begin(LORA_FREQUENCY)) {
    Serial.println("*** LoRa INIT FAILED! Check wiring and 3.3V supply ***");
    while (1) {
      digitalWrite(LED_PIN, !digitalRead(LED_PIN));
      delay(200);
    }
  }

  LoRa.setSpreadingFactor(7);
  LoRa.setSignalBandwidth(125E3);
  LoRa.setCodingRate4(5);
  LoRa.setPreambleLength(8);
  LoRa.setSyncWord(0x12);
  LoRa.enableCrc();
  LoRa.receive();

  // Wi-Fi AP + captive portal
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(AP_IP, AP_IP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(AP_SSID, strlen(AP_PASS) ? AP_PASS : nullptr);
  dnsServer.start(53, "*", AP_IP);

  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.on("/cmd", handleCmd);
  server.onNotFound(handleNotFound);
  server.begin();

  Serial.print("Portal ready. Connect to Wi-Fi '");
  Serial.print(AP_SSID);
  Serial.println("' and open http://192.168.4.1");
  Serial.println("Waiting for sensor data. Type ON, OFF or TOGGLE to control the node LED.");
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  // Serial Monitor -> LoRa (commands to the sensor node)
  while (Serial.available()) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      inputBuffer.trim();
      if (inputBuffer.length() > 0) sendMessage(inputBuffer);
      inputBuffer = "";
    } else {
      inputBuffer += c;
    }
  }

  // LoRa -> Serial Monitor + dashboard
  int packetSize = LoRa.parsePacket();
  if (packetSize) {
    char msg[96];
    int n = 0;
    while (LoRa.available() && n < (int)sizeof(msg) - 1) msg[n++] = (char)LoRa.read();
    msg[n] = '\0';

    unsigned long id;
    float t, tds, ntu;
    if (sscanf(msg, "ID=%lu,T=%f,TDS=%f,NTU=%f", &id, &t, &tds, &ntu) == 4) {
      received++;
      if (lastId >= 0 && (long)id > lastId + 1) lost += (id - lastId - 1);
      lastId = id;
      lastRssi = LoRa.packetRssi();
      lastSnr = LoRa.packetSnr();
      storeSample(id, t, tds, ntu);

      Serial.println("-------- Water Quality --------");
      if (t < -100) Serial.println("Temperature : sensor error");
      else          Serial.printf("Temperature : %.2f C\n", t);
      Serial.printf("TDS         : %.0f ppm\n", tds);
      Serial.printf("Turbidity   : %.0f NTU\n", ntu);
      Serial.printf("Packet #%lu | RSSI %d dBm | SNR %.1f dB | lost so far: %lu\n",
                    id, lastRssi, lastSnr, lost);
    } else {
      Serial.print("[Received] ");
      Serial.println(msg);
    }
  }
}
