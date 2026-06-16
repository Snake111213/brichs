/*
 * TEST ULTRA BÁSICO - Solo digitalWrite
 * Si esto NO mueve el motor → problema de HARDWARE
 * Si esto SÍ mueve el motor → problema de SOFTWARE (PWM/WiFi)
 * 
 * CONEXIONES:
 *   ESP32 GPIO27 → L298N IN1
 *   ESP32 GPIO26 → L298N IN2
 *   ESP32 GPIO14 → L298N ENA  (JUMPER QUITADO)
 *   ESP32 GPIO25 → L298N IN3
 *   ESP32 GPIO33 → L298N IN4
 *   ESP32 GPIO32 → L298N ENB  (JUMPER QUITADO)
 *   ESP32 GND    → L298N GND  (OBLIGATORIO)
 *   Batería 7-12V → L298N +12V y GND
 */

#define IN1 27
#define IN2 26
#define ENA 14
#define IN3 25
#define IN4 33
#define ENB 32

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n\n=== TEST ULTRA BASICO ===");
  Serial.println("Solo usa digitalWrite, nada de PWM");
  
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(ENA, OUTPUT);  // <-- Como OUTPUT normal, NO PWM
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);
  pinMode(ENB, OUTPUT);  // <-- Como OUTPUT normal, NO PWM

  // Todo apagado
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(ENB, LOW);

  Serial.println("Pines configurados. Empezando en 2 seg...\n");
  delay(2000);
}

void loop() {
  // ──────────────────────────────────
  //  MOTOR A: Adelante 3 segundos
  // ──────────────────────────────────
  Serial.println(">>> MOTOR A: ADELANTE <<<");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(ENA, HIGH);   // Enable ON (100%)
  delay(3000);

  // Parar
  Serial.println(">>> PARAR <<<");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, LOW);
  delay(2000);

  // ──────────────────────────────────
  //  MOTOR A: Atrás 3 segundos
  // ──────────────────────────────────
  Serial.println(">>> MOTOR A: ATRAS <<<");
  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, HIGH);
  delay(3000);

  // Parar
  Serial.println(">>> PARAR <<<");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, LOW);
  delay(2000);

  // ──────────────────────────────────
  //  MOTOR B: Adelante 3 segundos
  // ──────────────────────────────────
  Serial.println(">>> MOTOR B: ADELANTE <<<");
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  digitalWrite(ENB, HIGH);
  delay(3000);

  // Parar
  Serial.println(">>> PARAR <<<");
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(ENB, LOW);
  delay(2000);

  // ──────────────────────────────────
  //  MOTOR B: Atrás 3 segundos
  // ──────────────────────────────────
  Serial.println(">>> MOTOR B: ATRAS <<<");
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  digitalWrite(ENB, HIGH);
  delay(3000);

  // Parar
  Serial.println(">>> PARAR <<<");
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(ENB, LOW);
  delay(2000);

  // ──────────────────────────────────
  //  AMBOS: Adelante 4 segundos
  // ──────────────────────────────────
  Serial.println(">>> AMBOS MOTORES: ADELANTE <<<");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(ENA, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  digitalWrite(ENB, HIGH);
  delay(4000);

  // Parar todo
  Serial.println(">>> PARAR TODO <<<");
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(ENA, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  digitalWrite(ENB, LOW);

  Serial.println("\n--- Ciclo completado. Repitiendo en 5 seg... ---\n");
  delay(5000);
}
