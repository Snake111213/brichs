/*
 * BRICHS MÍNIMO - Solo WiFi + Motores
 * Sin GPS, sin micrófono, sin nada más
 * Para diagnosticar si WiFi interfiere con motores
 */

#include <WiFi.h>
#include <WebServer.h>

const char* ap_ssid     = "Brichs_MotorControl";
const char* ap_password = "brichs2026";

#define IN1 27
#define IN2 26
#define ENA 14
#define IN3 25
#define IN4 33
#define ENB 32

WebServer server(80);
String estadoA = "Detenido";
String estadoB = "Detenido";

void motorA_fwd() { digitalWrite(IN1,LOW);  digitalWrite(IN2,HIGH); digitalWrite(ENA,HIGH); estadoA="Adelante"; }
void motorA_rev() { digitalWrite(IN1,HIGH); digitalWrite(IN2,LOW);  digitalWrite(ENA,HIGH); estadoA="Atras"; }
void motorA_stp() { digitalWrite(IN1,LOW);  digitalWrite(IN2,LOW);  digitalWrite(ENA,LOW);  estadoA="Detenido"; }
void motorB_fwd() { digitalWrite(IN3,HIGH); digitalWrite(IN4,LOW);  digitalWrite(ENB,HIGH); estadoB="Adelante"; }
void motorB_rev() { digitalWrite(IN3,LOW);  digitalWrite(IN4,HIGH); digitalWrite(ENB,HIGH); estadoB="Atras"; }
void motorB_stp() { digitalWrite(IN3,LOW);  digitalWrite(IN4,LOW);  digitalWrite(ENB,LOW);  estadoB="Detenido"; }

void avanzar()  { motorA_fwd(); motorB_fwd(); Serial.println(">> Avanzar"); }
void retro()    { motorA_rev(); motorB_rev(); Serial.println(">> Retroceder"); }
void izq()      { motorA_stp(); motorB_fwd(); Serial.println(">> Izquierda"); }
void der()      { motorA_fwd(); motorB_stp(); Serial.println(">> Derecha"); }
void parar()    { motorA_stp(); motorB_stp(); Serial.println(">> Parar"); }

const char PAGINA[] PROGMEM = R"rawliteral(<!DOCTYPE html>
<html><head><meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0,user-scalable=no">
<title>Brichs</title>
<style>
*{margin:0;padding:0;box-sizing:border-box}
body{font-family:sans-serif;background:#0a0e1a;color:#e2e8f0;min-height:100vh;display:flex;justify-content:center}
.c{max-width:400px;width:100%;padding:20px}
h1{text-align:center;font-size:1.8rem;margin:20px 0;background:linear-gradient(135deg,#06d6a0,#4361ee,#7209b7);-webkit-background-clip:text;-webkit-text-fill-color:transparent}
.info{text-align:center;font-size:.7rem;color:#94a3b8;margin-bottom:20px}
.dpad{display:grid;grid-template-areas:'. up .' 'left stop right' '. down .';grid-template-columns:1fr 1fr 1fr;gap:10px;max-width:280px;margin:0 auto 20px}
.db{aspect-ratio:1;border:none;border-radius:16px;font-size:1.8rem;cursor:pointer;display:flex;align-items:center;justify-content:center}
.db:active{transform:scale(.9)}
.up{grid-area:up;background:#0d4a33;color:#06d6a0}
.dn{grid-area:down;background:#4a0d1e;color:#ef233c}
.lt{grid-area:left;background:#0d2a4a;color:#4361ee}
.rt{grid-area:right;background:#3a0d4a;color:#b14aed}
.st{grid-area:stop;background:#332200;color:#f8e16c;border:2px solid rgba(248,225,108,.3)}
.estado{text-align:center;padding:15px;background:rgba(15,23,42,.85);border:1px solid rgba(255,255,255,.08);border-radius:12px;margin-top:10px}
.estado span{font-weight:700;color:#06d6a0}
#log{margin-top:15px;padding:10px;background:rgba(0,0,0,.3);border-radius:8px;font-size:.7rem;color:#94a3b8;max-height:150px;overflow-y:auto}
</style></head><body>
<div class="c">
<h1>&#x26A1; BRICHS</h1>
<div class="info">WiFi + Motores (Sin GPS/Mic)</div>
<div class="dpad">
<button class="db up" onclick="go('avanzar')">&#x25B2;</button>
<button class="db lt" onclick="go('izquierda')">&#x25C4;</button>
<button class="db st" onclick="go('parar')">&#x23F9;</button>
<button class="db rt" onclick="go('derecha')">&#x25BA;</button>
<button class="db dn" onclick="go('retroceder')">&#x25BC;</button>
</div>
<div class="estado">Motor A: <span id="mA">Detenido</span> | Motor B: <span id="mB">Detenido</span></div>
<div id="log"></div>
</div>
<script>
function go(a){
  var l=document.getElementById('log');
  l.innerHTML='<div>> '+a+'...</div>'+l.innerHTML;
  fetch('/'+a).then(function(r){return r.text()}).then(function(t){
    l.innerHTML='<div style="color:#06d6a0">> '+a+': '+t+'</div>'+l.innerHTML;
    upd();
  }).catch(function(e){
    l.innerHTML='<div style="color:#ef233c">> ERROR: '+e+'</div>'+l.innerHTML;
  });
}
function upd(){
  fetch('/estado').then(function(r){return r.json()}).then(function(d){
    document.getElementById('mA').textContent=d.a;
    document.getElementById('mB').textContent=d.b;
  }).catch(function(){});
}
setInterval(upd,3000);
</script></body></html>)rawliteral";

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n============================");
  Serial.println("  BRICHS MINIMO v1.0");
  Serial.println("  Solo WiFi + Motores");
  Serial.println("============================\n");

  // Motores
  pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  pinMode(ENA, OUTPUT); pinMode(ENB, OUTPUT);
  parar();
  Serial.println("[OK] Motores (digitalWrite)");

  // WiFi solo AP (sin STA para no bloquear)
  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);
  Serial.println("[OK] AP: " + String(ap_ssid));
  Serial.println("[OK] IP: " + WiFi.softAPIP().toString());

  // Rutas
  server.on("/", []() {
    server.send_P(200, "text/html", PAGINA);
    Serial.println("[WEB] Pagina enviada");
  });
  server.on("/avanzar",    []() { avanzar(); server.send(200,"text/plain","OK"); });
  server.on("/retroceder", []() { retro();   server.send(200,"text/plain","OK"); });
  server.on("/izquierda",  []() { izq();     server.send(200,"text/plain","OK"); });
  server.on("/derecha",    []() { der();     server.send(200,"text/plain","OK"); });
  server.on("/parar",      []() { parar();   server.send(200,"text/plain","OK"); });
  server.on("/estado",     []() {
    server.send(200,"application/json","{\"a\":\""+estadoA+"\",\"b\":\""+estadoB+"\"}");
  });

  server.begin();
  Serial.println("[OK] Servidor en http://192.168.4.1");
  Serial.println("[OK] Heap libre: " + String(ESP.getFreeHeap()) + " bytes\n");
  Serial.println("Conectate a WiFi 'Brichs_MotorControl' pass: brichs2026");
  Serial.println("Abre http://192.168.4.1 en el navegador\n");
}

void loop() {
  server.handleClient();
}
