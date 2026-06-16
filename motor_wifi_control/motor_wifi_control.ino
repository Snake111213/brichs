/*
 * ============================================================
 *  BRICHS - Control de Motores + GPS + Voz via WiFi - ESP32
 *  Puente H: L298N | GPS: ATGM336H | Mic: INMP441 + Whisper
 *  Modo: AP+STA (AP para control, STA para Whisper)
 * ============================================================
 *  MOTORES (L298N -> ESP32):
 *    IN1=GPIO27 | IN2=GPIO26 | ENA=GPIO14  (Motor A)
 *    IN3=GPIO25 | IN4=GPIO33 | ENB=GPIO32  (Motor B)
 *
 *  GPS (ATGM336H -> ESP32):
 *    GPS: TX -> GPIO 16 (UART2-RX) | VCC -> 3.3V | GND -> GND
 *    (Solo conectar TX del módulo al RX del ESP32, el TX del ESP32 no se usa)
 *
 *  MICRÓFONO (INMP441 -> ESP32):
 *    VCC -> 3.3V | GND -> GND | L/R -> GND
 *    SCK (BCLK) -> GPIO 18
 *    WS  (LRCL) -> GPIO 21
 *    SD  (DOUT) -> GPIO 19
 *
 *  LIBRERÍAS REQUERIDAS: TinyGPS++
 *    Arduino IDE -> Herramientas -> Administrar librerías -> buscar "TinyGPS++"
 * ============================================================
 */

#include <WiFi.h>
#include <WebServer.h>
#include <TinyGPSPlus.h>
#include <HardwareSerial.h>
#include <LittleFS.h>
#include <driver/i2s.h>
#include <HTTPClient.h>

#define LOG_FILE "/ruta.csv"

// ─── WiFi AP (para controlar el robot) ─────────────────────
const char* ap_ssid     = "Brichs_MotorControl";
const char* ap_password = "brichs2026";

// ─── WiFi STA (para conectar al router y enviar audio a Whisper) ──
const char* sta_ssid     = "Karen";
const char* sta_password = "Karen8318";
const char* whisperUrl   = "http://192.168.137.1:5000/transcribe";

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

// ─── PINES MICRÓFONO I2S (INMP441) ────────────────────────
#define I2S_SCK   18   // Bit clock
#define I2S_WS    21   // Word select
#define I2S_SD    19   // Serial data in
#define I2S_PORT  I2S_NUM_1
#define MIC_SR    16000  // Sample rate 16kHz
#define MIC_GAIN  3
#define MAX_SEC   3
#define MAX_SAMP  (MIC_SR * MAX_SEC)   // 48000 muestras
#define MAX_BYTES (MAX_SAMP * 2)       // 96000 bytes (16-bit)

// ─── PWM ───────────────────────────────────────────────────
// Usamos analogWrite() que es compatible con todas las versiones
// del ESP32 Arduino Core (v2.x y v3.x)
#define PWM_FREQ       5000
#define PWM_RESOLUTION 8

// ─── OBJETOS GPS ───────────────────────────────────────────
TinyGPSPlus    gps;
HardwareSerial serialGPS(2);  // UART2

// ─── ESTADO MOTORES ────────────────────────────────────────
int    velocidadA   = 200;
int    velocidadB   = 200;
String estadoMotorA = "Detenido";
String estadoMotorB = "Detenido";

WebServer server(80);

// ─── NAVEGACIÓN Y LOGGING ──────────────────────────────────
bool   modoAuto    = false;
double targetLat   = 0.0;
double targetLon   = 0.0;
unsigned long lastLog = 0;

// ─── MICRÓFONO / VOZ ──────────────────────────────────────
enum MicState { MIC_IDLE, MIC_RECORDING, MIC_PROCESSING, MIC_DONE };
volatile MicState micEstado = MIC_IDLE;
int      numSamples   = 0;
String   micResultado = "";
String   micComando   = "";
uint8_t  *wavBuf;
int32_t  i2sBuf[512];
bool     staConectado = false;

// ═══════════════════════════════════════════════════════════
//  FUNCIONES DE APOYO — MOTORES
// ═══════════════════════════════════════════════════════════

void motorA_adelante() { digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); digitalWrite(ENA,HIGH); estadoMotorA="Adelante"; }
void motorA_atras()    { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  digitalWrite(ENA,HIGH); estadoMotorA="Atrás"; }
void motorA_parar()    { digitalWrite(IN1,LOW);  digitalWrite(IN2,LOW);  digitalWrite(ENA,LOW);  estadoMotorA="Detenido"; }
void motorB_adelante() { digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);  digitalWrite(ENB,HIGH); estadoMotorB="Adelante"; }
void motorB_atras()    { digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); digitalWrite(ENB,HIGH); estadoMotorB="Atrás"; }
void motorB_parar()    { digitalWrite(IN3,LOW);  digitalWrite(IN4,LOW);  digitalWrite(ENB,LOW);  estadoMotorB="Detenido"; }

void avanzar()        { motorA_adelante(); motorB_adelante(); }
void retroceder()     { motorA_atras();    motorB_atras(); }
void girarIzquierda() { motorA_parar();    motorB_adelante(); }
void girarDerecha()   { motorA_adelante(); motorB_parar(); }
void detenerTodo()    { motorA_parar();    motorB_parar(); }

// ═══════════════════════════════════════════════════════════
//  FUNCIONES DE APOYO — GPS
// ═══════════════════════════════════════════════════════════

void registrarPosicion() {
  if (!gps.location.isValid()) return;
  File file = LittleFS.open(LOG_FILE, FILE_APPEND);
  if (!file) return;
  file.print(millis()); file.print(",");
  file.print(gps.location.lat(), 6); file.print(",");
  file.println(gps.location.lng(), 6);
  file.close();
}

void procesarNavegacion() {
  if (!modoAuto || !gps.location.isValid()) return;

  double dist = TinyGPSPlus::distanceBetween(gps.location.lat(), gps.location.lng(), targetLat, targetLon);
  double cursoHacia = TinyGPSPlus::courseTo(gps.location.lat(), gps.location.lng(), targetLat, targetLon);
  
  if (dist < 3.0) { // Llegamos (umbral 3 metros)
    detenerTodo();
    modoAuto = false;
    Serial.println(">> Destino alcanzado!");
    return;
  }

  // Lógica de giro básica basada en el rumbo del GPS
  // Nota: El GPS solo da rumbo confiable si el robot se está moviendo.
  if (gps.course.isValid() && gps.speed.kmph() > 1.0) {
    double dif = cursoHacia - gps.course.deg();
    if (dif > 180) dif -= 360;
    if (dif < -180) dif += 360;

    if (dif > 20)      girarDerecha();
    else if (dif < -20) girarIzquierda();
    else                avanzar();
  } else {
    avanzar(); // Si no hay rumbo, avanzamos un poco para obtener uno
  }
}

// ═══════════════════════════════════════════════════════════
//  FUNCIONES DE APOYO — VOZ
// ═══════════════════════════════════════════════════════════

void procesarComandoVoz(String texto) {
  texto.toLowerCase();
  micComando = "";
  
  if (texto.indexOf("adelante") >= 0 || texto.indexOf("avanza") >= 0) {
    avanzar();
    micComando = "avanzar";
    Serial.println(">> VOZ: Avanzar");
  }
  else if (texto.indexOf("atrás") >= 0 || texto.indexOf("atras") >= 0 || texto.indexOf("retrocede") >= 0) {
    retroceder();
    micComando = "retroceder";
    Serial.println(">> VOZ: Retroceder");
  }
  else if (texto.indexOf("izquierda") >= 0) {
    girarIzquierda();
    micComando = "izquierda";
    Serial.println(">> VOZ: Izquierda");
  }
  else if (texto.indexOf("derecha") >= 0) {
    girarDerecha();
    micComando = "derecha";
    Serial.println(">> VOZ: Derecha");
  }
  else if (texto.indexOf("para") >= 0 || texto.indexOf("detén") >= 0 || texto.indexOf("deten") >= 0 || texto.indexOf("stop") >= 0) {
    detenerTodo();
    micComando = "parar";
    Serial.println(">> VOZ: Parar");
  }
  else {
    Serial.println(">> VOZ: Comando no reconocido: " + texto);
  }
}

void enviarAWhisper() {
  if (numSamples < MIC_SR / 4) {  // Menos de 0.25 segundos = muy corto
    micResultado = "";
    micEstado = MIC_DONE;
    Serial.println("[MIC] Grabación muy corta, ignorando");
    return;
  }

  // Construir header WAV
  uint32_t ds = numSamples * 2;
  uint32_t fs = ds + 36;
  uint8_t hdr[44] = {
    'R','I','F','F',
    (uint8_t)(fs),(uint8_t)(fs>>8),(uint8_t)(fs>>16),(uint8_t)(fs>>24),
    'W','A','V','E','f','m','t',' ',
    16,0,0,0, 1,0, 1,0,
    (uint8_t)(MIC_SR),(uint8_t)(MIC_SR>>8),(uint8_t)(MIC_SR>>16),(uint8_t)(MIC_SR>>24),
    (uint8_t)(MIC_SR*2),(uint8_t)((MIC_SR*2)>>8),(uint8_t)((MIC_SR*2)>>16),(uint8_t)((MIC_SR*2)>>24),
    2,0, 16,0,
    'd','a','t','a',
    (uint8_t)(ds),(uint8_t)(ds>>8),(uint8_t)(ds>>16),(uint8_t)(ds>>24)
  };
  memcpy(wavBuf, hdr, 44);

  // Verificar conexión STA
  if (WiFi.status() != WL_CONNECTED) {
    micResultado = "[Error: WiFi STA desconectado]";
    micEstado = MIC_DONE;
    Serial.println("[MIC] Error: WiFi STA no conectado");
    return;
  }

  // Enviar a Whisper
  Serial.println("[MIC] Enviando " + String(44 + ds) + " bytes a Whisper...");
  HTTPClient http;
  http.begin(whisperUrl);
  http.setTimeout(30000);
  http.addHeader("Content-Type", "audio/wav");

  int code = http.POST(wavBuf, 44 + ds);
  if (code > 0) {
    micResultado = http.getString();
    micResultado.trim();
    if (micResultado.length() > 1) {
      Serial.println("[MIC] Whisper: \"" + micResultado + "\"");
      procesarComandoVoz(micResultado);
    } else {
      Serial.println("[MIC] Whisper: sin texto detectado");
      micResultado = "";
    }
  } else {
    micResultado = "[Error HTTP: " + String(code) + "]";
    Serial.println("[MIC] Error HTTP: " + String(code));
  }
  http.end();
  micEstado = MIC_DONE;
}

// ═══════════════════════════════════════════════════════════
//  HTML — Enviado en trozos para no agotar la RAM
// ═══════════════════════════════════════════════════════════

// CSS almacenado en flash (PROGMEM) — no ocupa RAM
const char HTML_CSS[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html lang="es"><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=no">
<title>Brichs</title><style>
*{margin:0;padding:0;box-sizing:border-box}
:root{--bg:#0a0e1a;--card:rgba(15,23,42,.85);--glass:rgba(255,255,255,.04);--cyan:#06d6a0;--blue:#4361ee;--purple:#7209b7;--red:#ef233c;--yellow:#f8e16c;--text:#e2e8f0;--muted:#94a3b8;--border:rgba(255,255,255,.08);--shadow:0 8px 32px rgba(0,0,0,.4)}
body{font-family:sans-serif;background:var(--bg);color:var(--text);min-height:100vh}
.ctr{max-width:480px;margin:0 auto;padding:16px}
.hdr{text-align:center;padding:20px 0 12px}
.logo{font-size:2rem;font-weight:800;background:linear-gradient(135deg,var(--cyan),var(--blue),var(--purple));-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.sub{font-size:.75rem;color:var(--muted);margin-top:4px;letter-spacing:2px;text-transform:uppercase}
.sb{display:flex;justify-content:center;gap:6px;margin-top:10px;flex-wrap:wrap}
.pill{display:flex;align-items:center;gap:5px;padding:4px 10px;background:var(--glass);border:1px solid var(--border);border-radius:20px;font-size:.7rem;color:var(--muted)}
.dot{width:7px;height:7px;border-radius:50%;background:var(--cyan);animation:pulse 2s infinite}
.dot-off{width:7px;height:7px;border-radius:50%;background:var(--red)}
@keyframes pulse{0%,100%{opacity:1}50%{opacity:.5}}
.tabs{display:flex;gap:3px;background:var(--glass);border:1px solid var(--border);border-radius:12px;padding:3px;margin:16px 0}
.tab{flex:1;padding:8px 2px;text-align:center;border-radius:10px;font-size:.72rem;font-weight:600;cursor:pointer;color:var(--muted);border:none;background:transparent}
.tab.active{background:linear-gradient(135deg,var(--blue),var(--purple));color:#fff}
.pn{display:none}.pn.active{display:block}
.card{background:var(--card);border:1px solid var(--border);border-radius:16px;padding:18px;margin-bottom:12px;backdrop-filter:blur(20px);box-shadow:var(--shadow)}
.ct{font-size:.72rem;font-weight:600;color:var(--muted);text-transform:uppercase;letter-spacing:1.5px;margin-bottom:12px}
.dpad{display:grid;grid-template-areas:'. up .' 'left stop right' '. down .';grid-template-columns:1fr 1fr 1fr;gap:8px;max-width:260px;margin:0 auto}
.db{aspect-ratio:1;border:none;border-radius:14px;font-size:1.5rem;cursor:pointer;display:flex;align-items:center;justify-content:center;transition:transform .1s}
.db:active{transform:scale(.9)}
.up{grid-area:up;background:#0d4a33;color:#06d6a0}
.dn{grid-area:down;background:#4a0d1e;color:#ef233c}
.lt{grid-area:left;background:#0d2a4a;color:#4361ee}
.rt{grid-area:right;background:#3a0d4a;color:#b14aed}
.st{grid-area:stop;background:#332200;color:#f8e16c;border:2px solid rgba(248,225,108,.3)}
.mc{display:grid;grid-template-columns:1fr 1fr 1fr;gap:6px}
.mb{padding:12px 6px;border:none;border-radius:10px;font-size:.75rem;font-weight:600;cursor:pointer;color:#fff}
.mb:active{transform:scale(.94)}
.fwd{background:#059669}.rev{background:#dc2626}.stp{background:#78716c}
.sc{margin-top:8px}
.sl{display:flex;justify-content:space-between;align-items:center;margin-bottom:6px}
.sl span{font-size:.78rem;color:var(--muted)}
.sv{font-size:1rem;font-weight:700;color:var(--cyan)}
input[type=range]{-webkit-appearance:none;width:100%;height:6px;border-radius:3px;background:linear-gradient(90deg,var(--blue),var(--cyan));outline:none}
input[type=range]::-webkit-slider-thumb{-webkit-appearance:none;width:22px;height:22px;border-radius:50%;background:#fff;cursor:pointer}
.ms{display:flex;gap:8px;margin-top:12px}
.mi{flex:1;padding:10px;background:var(--glass);border:1px solid var(--border);border-radius:10px;text-align:center}
.mi .lbl{font-size:.68rem;color:var(--muted);text-transform:uppercase}
.mi .val{font-size:.85rem;font-weight:700;margin-top:3px;color:var(--cyan)}
.val.stopped{color:var(--muted)}.val.forward{color:var(--cyan)}.val.reverse{color:var(--red)}
.ig{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.ii{padding:10px;background:var(--glass);border:1px solid var(--border);border-radius:8px;text-align:center}
.ii .lbl{font-size:.62rem;color:var(--muted);text-transform:uppercase}
.ii .val{font-size:.85rem;font-weight:700;margin-top:2px}
.gg{display:grid;grid-template-columns:1fr 1fr;gap:6px}
.gc{grid-column:1/-1;padding:8px 10px;background:var(--glass);border:1px solid var(--border);border-radius:8px;text-align:center}
.gs{padding:8px 10px;background:var(--glass);border:1px solid var(--border);border-radius:8px;text-align:center}
.gc .lbl,.gs .lbl{font-size:.62rem;color:var(--muted);text-transform:uppercase}
.gc .val{font-size:.8rem;font-weight:600;margin-top:2px;font-family:monospace}
.gs .val{font-size:.85rem;font-weight:700;margin-top:2px}
.badge{padding:3px 8px;border-radius:20px;font-size:.65rem;font-weight:700}
.bfix{background:rgba(6,214,160,.2);color:#06d6a0}
.bnofix{background:rgba(239,35,60,.2);color:#ef233c}
.vw{display:flex;flex-direction:column;align-items:center;text-align:center}
.vd{font-size:.8rem;color:var(--muted);margin-bottom:16px;line-height:1.4}
.vo{position:relative;width:100px;height:100px;margin:0 auto}
.vp{position:absolute;inset:-8px;border-radius:50%;background:var(--cyan);opacity:0;transition:all .3s}
.vp.active{opacity:.15;animation:vpulse 1.2s infinite}
@keyframes vpulse{0%,100%{transform:scale(1)}50%{transform:scale(1.15)}}
.vb{position:relative;width:100%;height:100%;border-radius:50%;border:3px solid var(--border);background:var(--card);color:var(--cyan);font-size:2.5rem;cursor:pointer;display:flex;align-items:center;justify-content:center;outline:none}
.vb.rec{border-color:var(--cyan);background:rgba(6,214,160,.1);transform:scale(1.05)}
.vb.proc{border-color:var(--yellow);color:var(--yellow)}
.vs{margin-top:14px;font-weight:600;font-size:.9rem;color:var(--muted);min-height:22px}
.vs.on{color:var(--cyan)}.vs.wrk{color:var(--yellow)}
.vr{margin-top:8px;font-size:1rem;font-style:italic;min-height:26px}
.vc{margin-top:4px;font-size:.8rem;font-weight:600;min-height:20px}
.vc.ok{color:var(--cyan)}.vc.no{color:var(--red)}
.vwn{margin-top:10px;padding:6px 10px;background:rgba(239,35,60,.1);border:1px solid rgba(239,35,60,.2);border-radius:6px;font-size:.7rem;color:var(--red);display:none}
</style></head><body><div class="ctr">
<div class="hdr"><div class="logo">&#x26A1; BRICHS</div>
<div class="sub">Motor + GPS + Voice</div>
<div class="sb"><div class="pill"><div class="dot"></div>AP: Brichs</div>
<div class="pill"><div class="dot-off" id="sd"></div>STA: <span id="ss">)rawliteral";

const char HTML_BODY[] PROGMEM = R"rawliteral(</span></div></div></div>
<div class="tabs">
<button class="tab active" onclick="sw('robot')">&#x1F697; Robot</button>
<button class="tab" onclick="sw('gps')">&#x1F6F0; GPS</button>
<button class="tab" onclick="sw('nav')">&#x1F3AF; Nav</button>
<button class="tab" onclick="sw('ind')">&#x2699; Ind</button>
<button class="tab" onclick="sw('voice')">&#x1F399; Voz</button>
</div>
<div id="p-robot" class="pn active"><div class="card"><div class="ct">&#x1F3AE; Control</div>
<div class="dpad">
<button class="db up" onclick="cmd('avanzar')">&#x25B2;</button>
<button class="db lt" onclick="cmd('izquierda')">&#x25C4;</button>
<button class="db st" onclick="cmd('parar')">&#x23F9;</button>
<button class="db rt" onclick="cmd('derecha')">&#x25BA;</button>
<button class="db dn" onclick="cmd('retroceder')">&#x25BC;</button>
</div></div></div>
<div id="p-ind" class="pn"><div class="card"><div class="ct">&#x1F527; Motor A</div>
<div class="mc">
<button class="mb fwd" onclick="cmd('a_adelante')">&#x25B2; Adel</button>
<button class="mb stp" onclick="cmd('a_parar')">&#x23F9; Stop</button>
<button class="mb rev" onclick="cmd('a_atras')">&#x25BC; Atr</button>
</div></div>
<div class="card"><div class="ct">&#x1F527; Motor B</div>
<div class="mc">
<button class="mb fwd" onclick="cmd('b_adelante')">&#x25B2; Adel</button>
<button class="mb stp" onclick="cmd('b_parar')">&#x23F9; Stop</button>
<button class="mb rev" onclick="cmd('b_atras')">&#x25BC; Atr</button>
</div></div></div>
<div id="p-voice" class="pn"><div class="card"><div class="vw">
<div class="ct" style="justify-content:center">&#x1F399; Voz (INMP441)</div>
<div class="vd">Mant&#xE9;n el bot&#xF3;n y di:<br><em>adelante, atr&#xE1;s, izquierda, derecha, para</em></div>
<div class="vo"><div class="vp" id="vp"></div>
<button class="vb" id="vb"><span class="vic">&#x1F399;</span></button></div>
<div class="vs" id="vst">Toca para hablar</div>
<div class="vr" id="vrs"></div><div class="vc" id="vcm"></div>
<div class="vwn" id="vwn">&#x26A0; STA desconectado</div>
</div></div></div>
<div id="p-gps" class="pn"><div class="card">
<div style="display:flex;align-items:center;justify-content:space-between;margin-bottom:12px">
<div class="ct" style="margin:0">&#x1F6F0; GPS</div>
<span class="badge bnofix" id="gb">Sin fix</span></div>
<div class="gg">
<div class="gc"><div class="lbl">Pos</div><div class="val" id="gc">Buscando...</div></div>
<div class="gs"><div class="lbl">Sats</div><div class="val" id="gs">--</div></div>
<div class="gs"><div class="lbl">Alt</div><div class="val" id="ga">--</div></div>
<div class="gs"><div class="lbl">Vel</div><div class="val" id="gv">--</div></div>
<div class="gs"><div class="lbl">HDOP</div><div class="val" id="gh">--</div></div>
</div></div>
<div class="card"><div class="ct">&#x1F4C2; Log</div>
<button class="mb fwd" style="width:100%" onclick="window.open('/descargar-log')">&#x2935; Descargar CSV</button>
<button class="mb rev" style="width:100%;margin-top:6px" onclick="fetch('/borrar-log')">&#x1F5D1; Borrar</button>
</div></div>
<div id="p-nav" class="pn"><div class="card"><div class="ct">&#x1F3AF; Destino</div>
<div style="display:flex;flex-direction:column;gap:10px">
<div><div class="lbl" style="font-size:.55rem;color:var(--muted);margin-bottom:3px">LAT</div>
<input type="text" id="tLat" placeholder="19.4326" style="width:100%;padding:8px;background:var(--glass);border:1px solid var(--border);color:white;border-radius:6px"></div>
<div><div class="lbl" style="font-size:.55rem;color:var(--muted);margin-bottom:3px">LON</div>
<input type="text" id="tLon" placeholder="-99.1332" style="width:100%;padding:8px;background:var(--glass);border:1px solid var(--border);color:white;border-radius:6px"></div>
<button id="btnA" class="mb fwd" onclick="togAuto()" style="width:100%;padding:12px">&#x25B6; Iniciar Nav</button>
</div></div></div>
<div class="card"><div class="ct">&#x26A1; Velocidad</div>
<div class="sc"><div class="sl"><span>Motor A</span><span class="sv" id="svA">78%</span></div>
<input type="range" min="0" max="255" value="200" oninput="setSpd('A',this.value)"></div>
<div class="sc" style="margin-top:12px"><div class="sl"><span>Motor B</span><span class="sv" id="svB">78%</span></div>
<input type="range" min="0" max="255" value="200" oninput="setSpd('B',this.value)"></div></div>
<div class="card"><div class="ct">&#x1F4CA; Estado</div>
<div class="ms">
<div class="mi"><div class="lbl">Motor A</div><div class="val stopped" id="stA">Detenido</div></div>
<div class="mi"><div class="lbl">Motor B</div><div class="val stopped" id="stB">Detenido</div></div>
</div></div>
<div class="card"><div class="ct">&#x2139; Sistema</div>
<div class="ig">
<div class="ii"><div class="lbl">AP</div><div class="val">192.168.4.1</div></div>
<div class="ii"><div class="lbl">Driver</div><div class="val">L298N</div></div>
<div class="ii"><div class="lbl">MCU</div><div class="val">ESP32</div></div>
<div class="ii"><div class="lbl">GPS</div><div class="val">ATGM336H</div></div>
<div class="ii"><div class="lbl">Mic</div><div class="val">INMP441</div></div>
<div class="ii"><div class="lbl">STA</div><div class="val" id="siP">)rawliteral";

const char HTML_SCRIPT[] PROGMEM = R"rawliteral(</div></div></div></div></div>
<script>
var T={robot:0,gps:1,nav:2,ind:3,voice:4},am=false,mo=false,mp=null;
function sw(t){
document.querySelectorAll('.tab').forEach(function(e){e.classList.remove('active')});
document.querySelectorAll('.pn').forEach(function(e){e.classList.remove('active')});
document.querySelectorAll('.tab')[T[t]].classList.add('active');
document.getElementById('p-'+t).classList.add('active');
}
function cmd(a){fetch('/'+a).then(function(){pm()}).catch(function(){});}
function setSpd(m,v){
document.getElementById('sv'+m).textContent=Math.round(v/255*100)+'%';
fetch('/velocidad?motor='+m+'&valor='+v).catch(function(){});
}
function pm(){
fetch('/estado').then(function(r){return r.json()}).then(function(d){
var A=document.getElementById('stA'),B=document.getElementById('stB');
A.textContent=d.motorA;B.textContent=d.motorB;
A.className='val '+(d.motorA==='Detenido'?'stopped':d.motorA==='Adelante'?'forward':'reverse');
B.className='val '+(d.motorB==='Detenido'?'stopped':d.motorB==='Adelante'?'forward':'reverse');
if(d.sta!==undefined){
var dot=document.getElementById('sd'),st=document.getElementById('ss');
if(d.sta){dot.className='dot';st.textContent='OK';}
else{dot.className='dot-off';st.textContent='Off';}
}
}).catch(function(){});
}
function pg(){
fetch('/gps').then(function(r){return r.json()}).then(function(d){
var b=document.getElementById('gb'),c=document.getElementById('gc');
if(d.fix){b.textContent='FIX';b.className='badge bfix';c.textContent=d.lat.toFixed(6)+','+d.lon.toFixed(6);}
else{b.textContent='Sin fix';b.className='badge bnofix';c.textContent='Buscando...';}
document.getElementById('gs').textContent=d.sats;
document.getElementById('ga').textContent=d.alt.toFixed(1)+'m';
document.getElementById('gv').textContent=d.speed.toFixed(1)+'km/h';
document.getElementById('gh').textContent=d.hdop.toFixed(2);
}).catch(function(){});
}
function togAuto(){
if(!am){var la=document.getElementById('tLat').value,lo=document.getElementById('tLon').value;
fetch('/iniciar-auto?lat='+la+'&lon='+lo).then(function(){am=true;
document.getElementById('btnA').textContent='Stop';document.getElementById('btnA').className='mb rev';});}
else{fetch('/parar').then(function(){am=false;
document.getElementById('btnA').textContent='Iniciar Nav';document.getElementById('btnA').className='mb fwd';});}
}
var vb=document.getElementById('vb'),vp=document.getElementById('vp'),
vst=document.getElementById('vst'),vrs=document.getElementById('vrs'),
vcm=document.getElementById('vcm'),vwn=document.getElementById('vwn');
function ms(){if(mo)return;mo=true;vb.className='vb rec';vp.classList.add('active');
vst.textContent='Escuchando...';vst.className='vs on';vrs.textContent='';vcm.textContent='';
vwn.style.display='none';fetch('/mic-start').catch(function(){});}
function mt(){if(!mo)return;mo=false;vb.className='vb proc';vp.classList.remove('active');
vst.textContent='Procesando...';vst.className='vs wrk';
fetch('/mic-stop').then(function(){mp=setInterval(mc,400);}).catch(me);}
function mc(){fetch('/mic-result').then(function(r){return r.json()}).then(function(d){
if(d.state==='wait')return;clearInterval(mp);mp=null;vb.className='vb';vst.className='vs';
if(d.text&&d.text.length>1){vrs.textContent='"'+d.text+'"';
if(d.cmd){vcm.textContent='OK: '+d.cmd;vcm.className='vc ok';}
else{vcm.textContent='No reconocido';vcm.className='vc no';}
vst.textContent='Listo';}
else if(d.text&&d.text.indexOf('[Error')===0){vrs.textContent=d.text;vst.textContent='Error';
if(d.text.indexOf('STA')>=0)vwn.style.display='block';}
else{vrs.textContent='Sin voz';vst.textContent='Intenta de nuevo';}
}).catch(me);}
function me(){clearInterval(mp);mp=null;vb.className='vb';vst.textContent='Error';vst.className='vs';mo=false;}
if(vb){vb.addEventListener('mousedown',ms);vb.addEventListener('mouseup',mt);
vb.addEventListener('mouseleave',function(){if(mo)mt();});
vb.addEventListener('touchstart',function(e){e.preventDefault();ms();});
vb.addEventListener('touchend',function(e){e.preventDefault();mt();});
vb.addEventListener('touchcancel',mt);}
setInterval(pm,2000);setInterval(pg,1500);
</script></body></html>)rawliteral";

// Envía el HTML en trozos desde flash — usa ~0 bytes de heap
void handleRoot() {
  String staStatus = staConectado ? "OK" : "Off";
  String staIP = staConectado ? WiFi.localIP().toString() : "--";

  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/html", "");
  server.sendContent_P(HTML_CSS);
  server.sendContent(staStatus);
  server.sendContent_P(HTML_BODY);
  server.sendContent(staIP);
  server.sendContent_P(HTML_SCRIPT);
  server.client().stop();
  Serial.println("[WEB] Pagina enviada (chunked) | Heap: " + String(ESP.getFreeHeap()));
}
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
  // Velocidad variable deshabilitada - usando digitalWrite
  // Solo guardamos el valor para mostrarlo en la UI
  if (motor == "A") { velocidadA = valor; }
  else if (motor == "B") { velocidadB = valor; }
  server.send(200, "text/plain", "OK");
}

void handleEstado() {
  String json = "{\"motorA\":\"" + estadoMotorA + "\",\"motorB\":\"" + estadoMotorB +
                "\",\"velA\":" + velocidadA + ",\"velB\":" + velocidadB +
                ",\"sta\":" + String(staConectado ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

// ═══════════════════════════════════════════════════════════
//  HANDLERS — GPS
// ═══════════════════════════════════════════════════════════

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
  String json = gpsToJson(gps);
  json.remove(json.length()-1); // quitar }
  json += ",\"auto\":" + String(modoAuto ? "true" : "false") + "}";
  server.send(200, "application/json", json);
}

void handleIniciarAuto() {
  targetLat = server.arg("lat").toDouble();
  targetLon = server.arg("lon").toDouble();
  modoAuto = true;
  server.send(200, "text/plain", "OK");
  Serial.println(">> Navegacion Iniciada a: " + String(targetLat,6) + "," + String(targetLon,6));
}

void handleDescargarLog() {
  if (!LittleFS.exists(LOG_FILE)) { server.send(404, "text/plain", "No hay log"); return; }
  File file = LittleFS.open(LOG_FILE, "r");
  server.streamFile(file, "text/csv");
  file.close();
}

void handleBorrarLog() {
  LittleFS.remove(LOG_FILE);
  server.send(200, "text/plain", "Borrado");
}

// ═══════════════════════════════════════════════════════════
//  HANDLERS — MICRÓFONO / VOZ
// ═══════════════════════════════════════════════════════════

void handleMicStart() {
  micEstado = MIC_RECORDING;
  numSamples = 0;
  micResultado = "";
  micComando = "";
  i2s_zero_dma_buffer(I2S_PORT);
  server.send(200, "text/plain", "OK");
  Serial.println("[MIC] Grabando...");
}

void handleMicStop() {
  if (micEstado == MIC_RECORDING) {
    micEstado = MIC_PROCESSING;  // loop() se encarga de enviar
    Serial.println("[MIC] Detenido. Muestras: " + String(numSamples));
  }
  server.send(200, "text/plain", "OK");
}

void handleMicResult() {
  if (micEstado == MIC_DONE) {
    String json = "{\"state\":\"done\",\"text\":\"" + micResultado + "\"";
    if (micComando.length() > 0) {
      json += ",\"cmd\":\"" + micComando + "\"";
    }
    json += "}";
    server.send(200, "application/json", json);
    micEstado = MIC_IDLE;
  } else {
    server.send(200, "application/json", "{\"state\":\"wait\"}");
  }
}

// ═══════════════════════════════════════════════════════════
//  SETUP
// ═══════════════════════════════════════════════════════════

void setup() {
  Serial.begin(115200);
  Serial.println("\n============================");
  Serial.println("  BRICHS Motor+GPS+Voz v3.0");
  Serial.println("============================\n");

  // ─── Motores ──────────────────────────────────────────────
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT);
  pinMode(ENB, OUTPUT);
  detenerTodo();
  Serial.println("[OK] Motores inicializados (digitalWrite)");

  // ─── LittleFS ─────────────────────────────────────────────
  if(!LittleFS.begin(true)) Serial.println("[!!] Error LittleFS");
  else Serial.println("[OK] LittleFS Montado");

  // ─── GPS ──────────────────────────────────────────────────
  serialGPS.begin(GPS_BAUD, SERIAL_8N1, GPS_RX, -1);
  Serial.println("[OK] GPS UART inicializado (GPIO 16)");

  // ─── I2S Micrófono ────────────────────────────────────────
  /*
  i2s_config_t i2s_cfg = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = MIC_SR,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 8,
    .dma_buf_len = 512,
    .use_apll = true
  };
  i2s_pin_config_t i2s_pins = {
    .bck_io_num = I2S_SCK, .ws_io_num = I2S_WS,
    .data_out_num = -1, .data_in_num = I2S_SD
  };
  i2s_driver_install(I2S_PORT, &i2s_cfg, 0, NULL);
  i2s_set_pin(I2S_PORT, &i2s_pins);
  Serial.println("[OK] I2S Micrófono inicializado (SCK=18, WS=21, SD=19)");

  // Descartar ruido inicial del micrófono
  int32_t dummyBuf[256]; size_t dummyRead;
  for (int i = 0; i < 5; i++) i2s_read(I2S_PORT, dummyBuf, sizeof(dummyBuf), &dummyRead, portMAX_DELAY);
  */

  // ─── Buffer WAV ───────────────────────────────────────────
  wavBuf = (uint8_t*)malloc(44 + MAX_BYTES);
  if (!wavBuf) {
    Serial.println("[!!] Sin RAM para buffer WAV!");
    // Continuar sin micrófono en vez de reiniciar
  } else {
    Serial.println("[OK] Buffer WAV asignado (" + String(44 + MAX_BYTES) + " bytes)");
  }

  // ─── WiFi AP + STA ────────────────────────────────────────
  WiFi.mode(WIFI_AP_STA);

  // Iniciar AP
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("[OK] AP: " + String(ap_ssid) + " | IP: " + WiFi.softAPIP().toString());

  // Conectar a router (STA) para Whisper
  Serial.print("[..] Conectando a router '" + String(sta_ssid) + "'");
  WiFi.begin(sta_ssid, sta_password);
  int intentos = 0;
  while (WiFi.status() != WL_CONNECTED && intentos < 40) {
    delay(500);
    Serial.print(".");
    intentos++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    staConectado = true;
    Serial.println("\n[OK] STA conectado | IP: " + WiFi.localIP().toString());
    Serial.println("     Whisper URL: " + String(whisperUrl));
  } else {
    staConectado = false;
    Serial.println("\n[!!] STA: No se pudo conectar al router");
    Serial.println("     El control por voz no estará disponible");
    Serial.println("     Los motores, GPS y control manual funcionan normalmente");
  }

  // ─── Rutas del WebServer ──────────────────────────────────
  // Motor control
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
  // GPS
  server.on("/gps",          handleGPS);
  server.on("/iniciar-auto", handleIniciarAuto);
  server.on("/descargar-log", handleDescargarLog);
  server.on("/borrar-log",   handleBorrarLog);
  // Micrófono / Voz
  server.on("/mic-start",  handleMicStart);
  server.on("/mic-stop",   handleMicStop);
  server.on("/mic-result", handleMicResult);

  server.begin();
  Serial.println("\n[OK] Servidor iniciado en http://192.168.4.1");
  Serial.println("     Heap libre: " + String(ESP.getFreeHeap()) + " bytes\n");
}

// ═══════════════════════════════════════════════════════════
//  LOOP
// ═══════════════════════════════════════════════════════════

void loop() {
  server.handleClient();
  while (serialGPS.available()) gps.encode(serialGPS.read());

  // Navegación autónoma
  procesarNavegacion();

  // Logging GPS cada 5 segundos
  if (millis() - lastLog > 5000) {
    lastLog = millis();
    registrarPosicion();
    // Verificar reconexión STA periódicamente
    staConectado = (WiFi.status() == WL_CONNECTED);
  }

  // ─── MICRÓFONO: Grabando ──────────────────────────────────
  if (micEstado == MIC_RECORDING && wavBuf && numSamples < MAX_SAMP) {
    int16_t *audio = (int16_t*)(wavBuf + 44);
    size_t br = 0;
    int toRead = min(512, MAX_SAMP - numSamples);

    i2s_read(I2S_PORT, i2sBuf, toRead * 4, &br, 50);
    int got = br / 4;

    for (int i = 0; i < got && numSamples < MAX_SAMP; i++) {
      int32_t s = (i2sBuf[i] >> 14) * MIC_GAIN;
      if (s > 32767) s = 32767;
      if (s < -32768) s = -32768;
      audio[numSamples++] = (int16_t)s;
    }
  }

  // ─── MICRÓFONO: Procesando (enviar a Whisper) ─────────────
  if (micEstado == MIC_PROCESSING) {
    enviarAWhisper();
  }
}
