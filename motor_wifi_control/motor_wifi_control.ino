/*
 * ============================================================
 *  BRICHS - Control de Motores + GPS via WiFi - ESP32
 *  Puente H: L298N | GPS: 2x ATGM336H (GPS+BDS)
 *  Modo: Access Point (WiFi propio del ESP32)
 * ============================================================
 *  MOTORES (L298N -> ESP32):
 *    IN1=GPIO27 | IN2=GPIO26 | ENA=GPIO14  (Motor A)
 *    IN3=GPIO25 | IN4=GPIO33 | ENB=GPIO32  (Motor B)
 *
 *  GPS (ATGM336H -> ESP32):
 *    GPS: TX -> GPIO 16 (UART2-RX) | VCC -> 3.3V | GND -> GND
 *    (Solo conectar TX del módulo al RX del ESP32, el TX del ESP32 no se usa)
 *
 *  LIBRERÍA REQUERIDA: TinyGPS++
 *    Arduino IDE -> Herramientas -> Administrar librerías -> buscar "TinyGPS++"
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

// ─── WiFi ──────────────────────────────────────────────────
const char* ssid     = "Brichs_MotorControl";
const char* password = "brichs2026";

// ─── PINES MOTORES ─────────────────────────────────────────
#define IN1 27
#define IN2 26
#define ENA 14
#define IN3 25
#define IN4 33
#define ENB 32

// ─── PINES GPS ─────────────────────────────────────────────
#define GPS_RX   16   // UART2-RX -> TX del ATGM336H
#define GPS_BAUD 9600 // Baud rate por defecto del ATGM336H

// ─── PWM ───────────────────────────────────────────────────
#define PWM_FREQ       5000
#define PWM_RESOLUTION 8

// ─── OBJETOS GPS ───────────────────────────────────────────
TinyGPSPlus    gps;
HardwareSerial serialGPS(2);  // UART2

// ─── ESTADO ────────────────────────────────────────────────
int    velocidadA   = 200;
int    velocidadB   = 200;
String estadoMotorA = "Detenido";
String estadoMotorB = "Detenido";

WebServer server(80);

// ═══════════════════════════════════════════════════════════
//  FUNCIONES DE MOTOR
// ═══════════════════════════════════════════════════════════

void motorA_adelante() { digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); ledcWrite(ENA,velocidadA); estadoMotorA="Adelante"; }
void motorA_atras()    { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  ledcWrite(ENA,velocidadA); estadoMotorA="Atrás"; }
void motorA_parar()    { digitalWrite(IN1,LOW);  digitalWrite(IN2,LOW);  ledcWrite(ENA,0);           estadoMotorA="Detenido"; }
void motorB_adelante() { digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);  ledcWrite(ENB,velocidadB); estadoMotorB="Adelante"; }
void motorB_atras()    { digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); ledcWrite(ENB,velocidadB); estadoMotorB="Atrás"; }
void motorB_parar()    { digitalWrite(IN3,LOW);  digitalWrite(IN4,LOW);  ledcWrite(ENB,0);           estadoMotorB="Detenido"; }

void avanzar()        { motorA_adelante(); motorB_adelante(); }
void retroceder()     { motorA_atras();    motorB_atras(); }
void girarIzquierda() { motorA_parar();    motorB_adelante(); }
void girarDerecha()   { motorA_adelante(); motorB_parar(); }
void detenerTodo()    { motorA_parar();    motorB_parar(); }

// ═══════════════════════════════════════════════════════════
//  HTML
// ═══════════════════════════════════════════════════════════

String generarHTML() {
  String html = R"rawliteral(
<!DOCTYPE html>
<html lang="es">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
  <title>Brichs Motor Control</title>
  <style>
    @import url('https://fonts.googleapis.com/css2?family=Inter:wght@300;400;600;700;800&display=swap');
    * { margin:0; padding:0; box-sizing:border-box; }
    :root {
      --bg:#0a0e1a; --card:rgba(15,23,42,.85); --glass:rgba(255,255,255,.04);
      --cyan:#06d6a0; --blue:#4361ee; --purple:#7209b7;
      --red:#ef233c; --yellow:#f8e16c;
      --text:#e2e8f0; --muted:#94a3b8; --border:rgba(255,255,255,.08);
      --shadow:0 8px 32px rgba(0,0,0,.4);
    }
    body { font-family:'Inter',sans-serif; background:var(--bg); color:var(--text); min-height:100vh; overflow-x:hidden; }
    body::before {
      content:''; position:fixed; top:-50%; left:-50%; width:200%; height:200%;
      background:
        radial-gradient(circle at 30% 20%,rgba(67,97,238,.08) 0%,transparent 50%),
        radial-gradient(circle at 70% 80%,rgba(114,9,183,.06)  0%,transparent 50%),
        radial-gradient(circle at 50% 50%,rgba(6,214,160,.04)  0%,transparent 50%);
      z-index:0; animation:bgf 20s ease-in-out infinite;
    }
    @keyframes bgf { 0%,100%{transform:translate(0,0) rotate(0deg)} 33%{transform:translate(2%,-2%) rotate(1deg)} 66%{transform:translate(-1%,1%) rotate(-.5deg)} }
    .container { position:relative; z-index:1; max-width:480px; margin:0 auto; padding:16px; }
    .header { text-align:center; padding:24px 0 16px; }
    .logo { font-size:2rem; font-weight:800; background:linear-gradient(135deg,var(--cyan),var(--blue),var(--purple)); -webkit-background-clip:text; -webkit-text-fill-color:transparent; background-clip:text; }
    .subtitle { font-size:.8rem; color:var(--muted); margin-top:4px; font-weight:300; letter-spacing:2px; text-transform:uppercase; }
    .status-bar { display:flex; justify-content:center; gap:8px; margin-top:12px; }
    .pill { display:flex; align-items:center; gap:6px; padding:5px 12px; background:var(--glass); border:1px solid var(--border); border-radius:20px; font-size:.72rem; color:var(--muted); }
    .dot { width:7px; height:7px; border-radius:50%; background:var(--cyan); animation:pulse 2s ease-in-out infinite; }
    @keyframes pulse { 0%,100%{opacity:1;box-shadow:0 0 0 0 rgba(6,214,160,.4)} 50%{opacity:.7;box-shadow:0 0 0 6px rgba(6,214,160,0)} }
    .tabs { display:flex; gap:4px; background:var(--glass); border:1px solid var(--border); border-radius:14px; padding:4px; margin:20px 0; }
    .tab { flex:1; padding:9px 4px; text-align:center; border-radius:11px; font-size:.76rem; font-weight:600; cursor:pointer; transition:all .3s; color:var(--muted); border:none; background:transparent; }
    .tab.active { background:linear-gradient(135deg,var(--blue),var(--purple)); color:#fff; box-shadow:0 4px 15px rgba(67,97,238,.3); }
    .tab:not(.active):hover { background:rgba(255,255,255,.05); color:var(--text); }
    .panel { display:none; } .panel.active { display:block; }
    .card { background:var(--card); border:1px solid var(--border); border-radius:18px; padding:20px; margin-bottom:14px; backdrop-filter:blur(20px); box-shadow:var(--shadow); animation:fi .4s ease-out; }
    @keyframes fi { from{opacity:0;transform:translateY(10px)} to{opacity:1;transform:translateY(0)} }
    .card-title { font-size:.75rem; font-weight:600; color:var(--muted); text-transform:uppercase; letter-spacing:1.5px; margin-bottom:14px; display:flex; align-items:center; gap:8px; }
    .dpad { display:grid; grid-template-areas:'. up .' 'left stop right' '. down .'; grid-template-columns:1fr 1fr 1fr; gap:10px; max-width:280px; margin:0 auto; }
    .db { aspect-ratio:1; border:none; border-radius:16px; font-size:1.6rem; cursor:pointer; display:flex; align-items:center; justify-content:center; transition:all .15s; position:relative; overflow:hidden; }
    .db::after { content:''; position:absolute; inset:0; background:radial-gradient(circle at center,rgba(255,255,255,.1) 0%,transparent 70%); opacity:0; transition:opacity .2s; }
    .db:active::after { opacity:1; } .db:active { transform:scale(.92); } .db:hover { filter:brightness(1.2); }
    .up { grid-area:up;    background:linear-gradient(180deg,#1a6b4a,#0d4a33); color:#06d6a0; }
    .dn { grid-area:down;  background:linear-gradient(0deg,#6b1a2a,#4a0d1e);   color:#ef233c; }
    .lt { grid-area:left;  background:linear-gradient(90deg,#1a3a6b,#0d2a4a);  color:#4361ee; }
    .rt { grid-area:right; background:linear-gradient(270deg,#5a1a6b,#3a0d4a); color:#b14aed; }
    .st { grid-area:stop;  background:linear-gradient(135deg,#4a3300,#332200);  color:#f8e16c; border:2px solid rgba(248,225,108,.3); }
    .mc { display:grid; grid-template-columns:1fr 1fr 1fr; gap:8px; }
    .mb { padding:14px 8px; border:none; border-radius:12px; font-size:.78rem; font-weight:600; cursor:pointer; transition:all .15s; color:#fff; }
    .mb:active { transform:scale(.94); } .mb:hover { filter:brightness(1.2); }
    .fwd { background:linear-gradient(135deg,#064e3b,#059669); }
    .rev { background:linear-gradient(135deg,#4a0d1e,#dc2626); }
    .stp { background:linear-gradient(135deg,#44403c,#78716c); }
    .sc { margin-top:10px; }
    .sl { display:flex; justify-content:space-between; align-items:center; margin-bottom:8px; }
    .sl span { font-size:.8rem; color:var(--muted); }
    .sv { font-size:1.1rem; font-weight:700; color:var(--cyan); }
    input[type=range] { -webkit-appearance:none; appearance:none; width:100%; height:8px; border-radius:4px; background:linear-gradient(90deg,var(--blue),var(--cyan),var(--purple)); outline:none; opacity:.85; transition:opacity .2s; }
    input[type=range]:hover { opacity:1; }
    input[type=range]::-webkit-slider-thumb { -webkit-appearance:none; appearance:none; width:24px; height:24px; border-radius:50%; background:#fff; cursor:pointer; box-shadow:0 2px 8px rgba(0,0,0,.3),0 0 0 3px rgba(67,97,238,.3); }
    .ms { display:flex; gap:10px; margin-top:14px; }
    .mi { flex:1; padding:12px; background:var(--glass); border:1px solid var(--border); border-radius:12px; text-align:center; }
    .mi .lbl { font-size:.7rem; color:var(--muted); text-transform:uppercase; letter-spacing:1px; }
    .mi .val { font-size:.9rem; font-weight:700; margin-top:4px; color:var(--cyan); }
    .val.stopped { color:var(--muted); } .val.forward { color:var(--cyan); } .val.reverse { color:var(--red); }
    .ig { display:grid; grid-template-columns:1fr 1fr; gap:8px; }
    .ii { padding:12px; background:var(--glass); border:1px solid var(--border); border-radius:10px; text-align:center; }
    .ii .lbl { font-size:.65rem; color:var(--muted); text-transform:uppercase; letter-spacing:1px; }
    .ii .val { font-size:.95rem; font-weight:700; margin-top:2px; color:var(--text); }
    .gps-head { display:flex; align-items:center; justify-content:space-between; margin-bottom:14px; }
    .gps-title { font-size:.75rem; font-weight:600; color:var(--muted); text-transform:uppercase; letter-spacing:1.5px; display:flex; align-items:center; gap:8px; }
    .badge { padding:3px 10px; border-radius:20px; font-size:.68rem; font-weight:700; }
    .bfix   { background:rgba(6,214,160,.2);  color:#06d6a0; }
    .bnofix { background:rgba(239,35,60,.2);   color:#ef233c; }
    .gg { display:grid; grid-template-columns:1fr 1fr; gap:8px; }
    .gc { grid-column:1/-1; padding:10px 12px; background:var(--glass); border:1px solid var(--border); border-radius:10px; text-align:center; }
    .gs { padding:10px 12px; background:var(--glass); border:1px solid var(--border); border-radius:10px; text-align:center; }
    .gc .lbl,.gs .lbl { font-size:.65rem; color:var(--muted); text-transform:uppercase; letter-spacing:1px; }
    .gc .val { font-size:.85rem; font-weight:600; margin-top:3px; color:var(--text); font-family:monospace; word-break:break-all; }
    .gs .val { font-size:.9rem; font-weight:700; margin-top:2px; color:var(--text); }
  </style>
</head>
<body>
<div class="container">
  <div class="header">
    <div class="logo">&#x26A1; BRICHS</div>
    <div class="subtitle">Motor Control System</div>
    <div class="status-bar">
      <div class="pill"><div class="dot"></div>WiFi: Brichs_MotorControl</div>
      <div class="pill">&#x1F50B; Activo</div>
    </div>
  </div>

  <div class="tabs">
    <button class="tab active" onclick="switchTab('robot')">&#x1F697; Robot</button>
    <button class="tab" onclick="switchTab('gps')">&#x1F6F0;&#xFE0F; GPS</button>
    <button class="tab" onclick="switchTab('individual')">&#x2699;&#xFE0F; Individual</button>
  </div>

  <div id="panel-robot" class="panel active">
    <div class="card">
      <div class="card-title">&#x1F3AE; Control Direccional</div>
      <div class="dpad">
        <button class="db up" onclick="cmd('avanzar')">&#x25B2;</button>
        <button class="db lt" onclick="cmd('izquierda')">&#x25C4;</button>
        <button class="db st" onclick="cmd('parar')">&#x23F9;</button>
        <button class="db rt" onclick="cmd('derecha')">&#x25BA;</button>
        <button class="db dn" onclick="cmd('retroceder')">&#x25BC;</button>
      </div>
    </div>
  </div>

  <div id="panel-individual" class="panel">
    <div class="card">
      <div class="card-title">&#x1F527; Motor A (Izquierdo)</div>
      <div class="mc">
        <button class="mb fwd" onclick="cmd('a_adelante')">&#x25B2; Adelante</button>
        <button class="mb stp" onclick="cmd('a_parar')">&#x23F9; Parar</button>
        <button class="mb rev" onclick="cmd('a_atras')">&#x25BC; Atr&#xE1;s</button>
      </div>
    </div>
    <div class="card">
      <div class="card-title">&#x1F527; Motor B (Derecho)</div>
      <div class="mc">
        <button class="mb fwd" onclick="cmd('b_adelante')">&#x25B2; Adelante</button>
        <button class="mb stp" onclick="cmd('b_parar')">&#x23F9; Parar</button>
        <button class="mb rev" onclick="cmd('b_atras')">&#x25BC; Atr&#xE1;s</button>
      </div>
    </div>
  </div>

  <div id="panel-gps" class="panel">
    <div class="card">
      <div class="gps-head">
        <div class="gps-title">&#x1F6F0;&#xFE0F; GPS &#x2014; Ubicaci&oacute;n</div>
        <span class="badge bnofix" id="gb">Sin fix</span>
      </div>
      <div class="gg">
        <div class="gc"><div class="lbl">Posici&#xF3;n</div><div class="val" id="gc">Buscando se&#xF1;al...</div></div>
        <div class="gs"><div class="lbl">Sat&#xE9;lites</div><div class="val" id="gs">--</div></div>
        <div class="gs"><div class="lbl">Altitud</div><div class="val" id="ga">-- m</div></div>
        <div class="gs"><div class="lbl">Velocidad</div><div class="val" id="gv">-- km/h</div></div>
        <div class="gs"><div class="lbl">HDOP</div><div class="val" id="gh">--</div></div>
      </div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#x26A1; Velocidad</div>
    <div class="sc">
      <div class="sl"><span>Motor A</span><span class="sv" id="svA">78%</span></div>
      <input type="range" min="0" max="255" value="200" oninput="setSpd('A',this.value)">
    </div>
    <div class="sc" style="margin-top:16px">
      <div class="sl"><span>Motor B</span><span class="sv" id="svB">78%</span></div>
      <input type="range" min="0" max="255" value="200" oninput="setSpd('B',this.value)">
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#x1F4CA; Estado</div>
    <div class="ms">
      <div class="mi"><div class="lbl">Motor A</div><div class="val stopped" id="stA">Detenido</div></div>
      <div class="mi"><div class="lbl">Motor B</div><div class="val stopped" id="stB">Detenido</div></div>
    </div>
  </div>

  <div class="card">
    <div class="card-title">&#x2139;&#xFE0F; Sistema</div>
    <div class="ig">
      <div class="ii"><div class="lbl">IP</div><div class="val">192.168.4.1</div></div>
      <div class="ii"><div class="lbl">Puente H</div><div class="val">L298N</div></div>
      <div class="ii"><div class="lbl">MCU</div><div class="val">ESP32</div></div>
      <div class="ii"><div class="lbl">GPS</div><div class="val">1x ATGM336H</div></div>
    </div>
  </div>
</div>

<script>
  const TABS = {robot:0, individual:1, gps:2};
  function switchTab(t) {
    document.querySelectorAll('.tab').forEach(e=>e.classList.remove('active'));
    document.querySelectorAll('.panel').forEach(e=>e.classList.remove('active'));
    document.querySelectorAll('.tab')[TABS[t]].classList.add('active');
    document.getElementById('panel-'+t).classList.add('active');
  }
  function cmd(a) { fetch('/'+a).then(()=>pollMotors()).catch(e=>console.log(e)); }
  function setSpd(m,v) {
    document.getElementById('sv'+m).textContent = Math.round(v/255*100)+'%';
    fetch('/velocidad?motor='+m+'&valor='+v).catch(e=>console.log(e));
  }
  function pollMotors() {
    fetch('/estado').then(r=>r.json()).then(d=>{
      let A=document.getElementById('stA'), B=document.getElementById('stB');
      A.textContent=d.motorA; B.textContent=d.motorB;
      A.className='val '+cls(d.motorA); B.className='val '+cls(d.motorB);
    }).catch(e=>console.log(e));
  }
  function cls(s) { return s==='Detenido'?'stopped':s==='Adelante'?'forward':'reverse'; }
  function pollGPS() {
    fetch('/gps').then(r=>r.json()).then(d=>{
      let b=document.getElementById('gb'), c=document.getElementById('gc');
      if(d.fix) {
        b.textContent='FIX OK'; b.className='badge bfix';
        c.textContent=d.lat.toFixed(6)+', '+d.lon.toFixed(6);
      } else {
        b.textContent='Sin fix'; b.className='badge bnofix';
        c.textContent='Buscando... ('+d.chars+' chars recibidos)';
      }
      document.getElementById('gs').textContent = d.sats;
      document.getElementById('ga').textContent = d.alt.toFixed(1)+' m';
      document.getElementById('gv').textContent = d.speed.toFixed(1)+' km/h';
      document.getElementById('gh').textContent = d.hdop.toFixed(2);
    }).catch(e=>console.log(e));
  }
  setInterval(pollMotors, 2000);
  setInterval(pollGPS,    1000);
</script>
</body>
</html>
)rawliteral";
  return html;
}

// ═══════════════════════════════════════════════════════════
//  HANDLERS
// ═══════════════════════════════════════════════════════════

void handleRoot()       { server.send(200, "text/html", generarHTML()); }
void handleAvanzar()    { avanzar();        server.send(200, "text/plain", "OK"); Serial.println(">> Avanzar"); }
void handleRetroceder() { retroceder();     server.send(200, "text/plain", "OK"); Serial.println(">> Retroceder"); }
void handleIzquierda()  { girarIzquierda(); server.send(200, "text/plain", "OK"); Serial.println(">> Izquierda"); }
void handleDerecha()    { girarDerecha();   server.send(200, "text/plain", "OK"); Serial.println(">> Derecha"); }
void handleParar()      { detenerTodo();    server.send(200, "text/plain", "OK"); Serial.println(">> Parar"); }
void handleA_adelante() { motorA_adelante(); server.send(200, "text/plain", "OK"); }
void handleA_atras()    { motorA_atras();    server.send(200, "text/plain", "OK"); }
void handleA_parar()    { motorA_parar();    server.send(200, "text/plain", "OK"); }
void handleB_adelante() { motorB_adelante(); server.send(200, "text/plain", "OK"); }
void handleB_atras()    { motorB_atras();    server.send(200, "text/plain", "OK"); }
void handleB_parar()    { motorB_parar();    server.send(200, "text/plain", "OK"); }

void handleVelocidad() {
  String motor = server.arg("motor");
  int valor = constrain(server.arg("valor").toInt(), 0, 255);
  if (motor == "A") { velocidadA = valor; ledcWrite(ENA, velocidadA); }
  else if (motor == "B") { velocidadB = valor; ledcWrite(ENB, velocidadB); }
  server.send(200, "text/plain", "OK");
}

void handleEstado() {
  String json = "{\"motorA\":\"" + estadoMotorA + "\",\"motorB\":\"" + estadoMotorB +
                "\",\"velA\":" + velocidadA + ",\"velB\":" + velocidadB + "}";
  server.send(200, "application/json", json);
}

String gpsToJson(TinyGPSPlus &g) {
  bool fix = g.location.isValid() && g.location.age() < 2000;
  String j = "{";
  j += "\"fix\":"   + String(fix ? "true" : "false")                                    + ",";
  j += "\"lat\":"   + String(fix ? g.location.lat() : 0.0, 6)                           + ",";
  j += "\"lon\":"   + String(fix ? g.location.lng() : 0.0, 6)                           + ",";
  j += "\"sats\":"  + String(g.satellites.isValid() ? (int)g.satellites.value() : 0)    + ",";
  j += "\"alt\":"   + String(g.altitude.isValid()   ? g.altitude.meters()   : 0.0, 1)   + ",";
  j += "\"speed\":" + String(g.speed.isValid()       ? g.speed.kmph()        : 0.0, 1)  + ",";
  j += "\"hdop\":"  + String(g.hdop.isValid()        ? g.hdop.value()/100.0  : 99.99, 2)+ ",";
  j += "\"chars\":" + String(g.charsProcessed());
  j += "}";
  return j;
}

void handleGPS() {
  server.send(200, "application/json", gpsToJson(gps));
}

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println("\n============================");
  Serial.println("  BRICHS Motor+GPS v2.0");
  Serial.println("============================\n");

  // Motores
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION);
  ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION);
  detenerTodo();

  // GPS — TX del módulo -> RX del ESP32, el TX del ESP32 no se conecta (-1)
  serialGPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, -1);
  Serial.println("GPS UART inicializado");
  Serial.println("  GPS: modulo TX -> GPIO 16\n");

  // WiFi AP
  WiFi.softAP(ssid, password);
  Serial.print("Red WiFi: "); Serial.println(ssid);
  Serial.print("IP:       "); Serial.println(WiFi.softAPIP());

  // Rutas
  server.on("/",           handleRoot);
  server.on("/avanzar",    handleAvanzar);
  server.on("/retroceder", handleRetroceder);
  server.on("/izquierda",  handleIzquierda);
  server.on("/derecha",    handleDerecha);
  server.on("/parar",      handleParar);
  server.on("/a_adelante", handleA_adelante);
  server.on("/a_atras",    handleA_atras);
  server.on("/a_parar",    handleA_parar);
  server.on("/b_adelante", handleB_adelante);
  server.on("/b_atras",    handleB_atras);
  server.on("/b_parar",    handleB_parar);
  server.on("/velocidad",  handleVelocidad);
  server.on("/estado",     handleEstado);
  server.on("/gps",        handleGPS);

  server.begin();
  Serial.println("\nServidor iniciado en http://192.168.4.1\n");
}

// ═══════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  while (serialGPS.available()) gps.encode(serialGPS.read());
}
