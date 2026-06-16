/*
 * ============================================================
 *  TEST DIAGNÓSTICO REAL DE MOTORES - L298N + ESP32
 * ============================================================
 *  Este sketch prueba paso a paso CADA pin de motor
 *  para identificar exactamente qué no funciona.
 *
 *  CONEXIONES (igual que motor_wifi_control):
 *    IN1=GPIO27 | IN2=GPIO26 | ENA=GPIO14  (Motor A)
 *    IN3=GPIO25 | IN4=GPIO33 | ENB=GPIO32  (Motor B)
 *
 *  IMPORTANTE:
 *  - L298N debe tener alimentación externa (7-12V)
 *  - Jumpers ENA/ENB del L298N deben estar QUITADOS
 *    (porque controlamos velocidad por software)
 *  - GND del ESP32 conectado a GND del L298N
 *
 *  Abre el Monitor Serie a 115200 y sigue las instrucciones.
 * ============================================================
 */

// Mismos pines que tu proyecto principal
#define IN1 27
#define IN2 26
#define ENA 14
#define IN3 25
#define IN4 33
#define ENB 32

#define PWM_FREQ       5000
#define PWM_RESOLUTION 8

int testNum = 0;

void printSeparator() {
  Serial.println("─────────────────────────────────────────");
}

void waitSerial(const char* msg) {
  Serial.println();
  printSeparator();
  Serial.print(">> ");
  Serial.println(msg);
  Serial.println("   Escribe cualquier cosa y Enter para continuar...");
  printSeparator();
  while (!Serial.available()) delay(10);
  while (Serial.available()) Serial.read();  // limpiar buffer
  delay(300);
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("\n\n");
  Serial.println("╔══════════════════════════════════════════════╗");
  Serial.println("║  TEST DIAGNÓSTICO REAL DE MOTORES            ║");
  Serial.println("║  L298N + ESP32                               ║");
  Serial.println("╚══════════════════════════════════════════════╝");
  Serial.println();
  Serial.println("Pines configurados:");
  Serial.println("  Motor A: IN1=GPIO27, IN2=GPIO26, ENA=GPIO14");
  Serial.println("  Motor B: IN3=GPIO25, IN4=GPIO33, ENB=GPIO32");
  Serial.println();

  // ─── Configurar pines de dirección ───
  pinMode(IN1, OUTPUT);
  pinMode(IN2, OUTPUT);
  pinMode(IN3, OUTPUT);
  pinMode(IN4, OUTPUT);

  // Poner todo en LOW
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);

  Serial.println("[OK] Pines de dirección configurados (OUTPUT)");

  // ─── Configurar PWM para ENA y ENB ───
  // Intentar con la API v3.x (ledcAttach)
  // Si no compila, tu versión es v2.x y hay que cambiar
  bool pwmOk = true;

  if (ledcAttach(ENA, PWM_FREQ, PWM_RESOLUTION)) {
    Serial.println("[OK] PWM ENA (GPIO14) configurado - ledcAttach OK");
  } else {
    Serial.println("[!!] FALLO PWM ENA (GPIO14) - ledcAttach falló!");
    pwmOk = false;
  }

  if (ledcAttach(ENB, PWM_FREQ, PWM_RESOLUTION)) {
    Serial.println("[OK] PWM ENB (GPIO32) configurado - ledcAttach OK");
  } else {
    Serial.println("[!!] FALLO PWM ENB (GPIO32) - ledcAttach falló!");
    pwmOk = false;
  }

  // Poner velocidad a 0
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);

  Serial.println();
  if (pwmOk) {
    Serial.println("✅ PWM inicializado correctamente");
  } else {
    Serial.println("❌ ERROR en PWM - los motores NO van a girar");
    Serial.println("   Puede que necesites otra versión del ESP32 Core");
  }

  Serial.println();
  Serial.println("Heap libre: " + String(ESP.getFreeHeap()) + " bytes");

  waitSerial("Listo para empezar las pruebas. ¿Continuamos?");
}

void loop() {
  testNum++;

  switch(testNum) {
    case 1:
      test_1_direccion_pins();
      break;
    case 2:
      test_2_pwm_enable();
      break;
    case 3:
      test_3_motorA_adelante();
      break;
    case 4:
      test_4_motorA_atras();
      break;
    case 5:
      test_5_motorB_adelante();
      break;
    case 6:
      test_6_motorB_atras();
      break;
    case 7:
      test_7_ambos_adelante();
      break;
    case 8:
      test_8_velocidad_variable();
      break;
    default:
      Serial.println("\n\n╔══════════════════════════════════════════════╗");
      Serial.println("║  TODAS LAS PRUEBAS COMPLETADAS               ║");
      Serial.println("╚══════════════════════════════════════════════╝");
      Serial.println("\nSi ningún motor giró, revisa:");
      Serial.println("  1. Alimentación del L298N (7-12V)");
      Serial.println("  2. GND compartido ESP32 <-> L298N");
      Serial.println("  3. Jumpers ENA/ENB quitados del L298N");
      Serial.println("  4. Cables de señal (IN1-IN4, ENA, ENB)");
      Serial.println("  5. Motor conectado a OUT1/OUT2 o OUT3/OUT4");
      Serial.println();
      Serial.println("Si un motor giró pero otro no:");
      Serial.println("  → Problema en ese canal del L298N o su cableado");
      Serial.println();
      Serial.println("Reinicia el ESP32 para repetir las pruebas.");
      while(true) delay(1000);  // Parar aquí
  }
}

// ═══════════════════════════════════════════════════════════
//  TEST 1: Probar pines de dirección individualmente
// ═══════════════════════════════════════════════════════════
void test_1_direccion_pins() {
  Serial.println("\n══ TEST 1: PINES DE DIRECCIÓN (sin PWM) ══");
  Serial.println("Cada pin se pondrá en HIGH 2 seg.");
  Serial.println("Puedes medir con multímetro si llega ~3.3V al L298N.");
  Serial.println();

  const int pines[] = {IN1, IN2, IN3, IN4};
  const char* nombres[] = {"IN1 (GPIO27)", "IN2 (GPIO26)", "IN3 (GPIO25)", "IN4 (GPIO33)"};

  for (int i = 0; i < 4; i++) {
    Serial.print("  → ");
    Serial.print(nombres[i]);
    Serial.print(" = HIGH ... ");
    digitalWrite(pines[i], HIGH);
    delay(2000);
    digitalWrite(pines[i], LOW);
    Serial.println("LOW");
    delay(500);
  }

  Serial.println("\n✅ Test 1 completado.");
  Serial.println("   ¿Mediste voltaje en los pines? Deben dar ~3.3V");
  waitSerial("Continuar al Test 2 (PWM Enable)");
}

// ═══════════════════════════════════════════════════════════
//  TEST 2: Probar pines ENA/ENB con PWM
// ═══════════════════════════════════════════════════════════
void test_2_pwm_enable() {
  Serial.println("\n══ TEST 2: PWM EN ENA/ENB ══");
  Serial.println("Sin dirección activa (motores no girarán aún).");
  Serial.println("Solo verificamos que el PWM genera señal.\n");

  Serial.print("  → ENA (GPIO14) PWM 100% por 2s ... ");
  ledcWrite(ENA, 255);
  delay(2000);
  ledcWrite(ENA, 0);
  Serial.println("OFF");

  delay(500);

  Serial.print("  → ENB (GPIO32) PWM 100% por 2s ... ");
  ledcWrite(ENB, 255);
  delay(2000);
  ledcWrite(ENB, 0);
  Serial.println("OFF");

  Serial.println("\n✅ Test 2 completado.");
  Serial.println("   Si mides con osciloscopio/multímetro en ENA/ENB,");
  Serial.println("   deberías ver ~3.3V durante los pulsos.");
  waitSerial("Continuar al Test 3 (Motor A Adelante)");
}

// ═══════════════════════════════════════════════════════════
//  TEST 3: Motor A Adelante
// ═══════════════════════════════════════════════════════════
void test_3_motorA_adelante() {
  Serial.println("\n══ TEST 3: MOTOR A - ADELANTE ══");
  Serial.println("IN1=LOW, IN2=HIGH, ENA=200 (78%)");
  Serial.println("El Motor A debería GIRAR 3 segundos.\n");

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  ledcWrite(ENA, 200);

  Serial.println("  >>> MOTOR A ENCENDIDO - ¿GIRA? <<<");
  delay(3000);

  // Parar
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(ENA, 0);

  Serial.println("  >>> MOTOR A APAGADO <<<");
  Serial.println("\n  ¿El motor A giró?");
  Serial.println("  SÍ → Genial, Motor A funciona");
  Serial.println("  NO → Problema en Motor A o sus cables");
  waitSerial("Continuar al Test 4 (Motor A Atrás)");
}

// ═══════════════════════════════════════════════════════════
//  TEST 4: Motor A Atrás
// ═══════════════════════════════════════════════════════════
void test_4_motorA_atras() {
  Serial.println("\n══ TEST 4: MOTOR A - ATRÁS ══");
  Serial.println("IN1=HIGH, IN2=LOW, ENA=200 (78%)");
  Serial.println("El Motor A debería GIRAR EN SENTIDO CONTRARIO.\n");

  digitalWrite(IN1, HIGH);
  digitalWrite(IN2, LOW);
  ledcWrite(ENA, 200);

  Serial.println("  >>> MOTOR A ENCENDIDO (REVERSA) - ¿GIRA? <<<");
  delay(3000);

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  ledcWrite(ENA, 0);

  Serial.println("  >>> MOTOR A APAGADO <<<");
  waitSerial("Continuar al Test 5 (Motor B Adelante)");
}

// ═══════════════════════════════════════════════════════════
//  TEST 5: Motor B Adelante
// ═══════════════════════════════════════════════════════════
void test_5_motorB_adelante() {
  Serial.println("\n══ TEST 5: MOTOR B - ADELANTE ══");
  Serial.println("IN3=HIGH, IN4=LOW, ENB=200 (78%)");
  Serial.println("El Motor B debería GIRAR 3 segundos.\n");

  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  ledcWrite(ENB, 200);

  Serial.println("  >>> MOTOR B ENCENDIDO - ¿GIRA? <<<");
  delay(3000);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  ledcWrite(ENB, 0);

  Serial.println("  >>> MOTOR B APAGADO <<<");
  waitSerial("Continuar al Test 6 (Motor B Atrás)");
}

// ═══════════════════════════════════════════════════════════
//  TEST 6: Motor B Atrás
// ═══════════════════════════════════════════════════════════
void test_6_motorB_atras() {
  Serial.println("\n══ TEST 6: MOTOR B - ATRÁS ══");
  Serial.println("IN3=LOW, IN4=HIGH, ENB=200 (78%)");
  Serial.println("El Motor B debería GIRAR EN SENTIDO CONTRARIO.\n");

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, HIGH);
  ledcWrite(ENB, 200);

  Serial.println("  >>> MOTOR B ENCENDIDO (REVERSA) - ¿GIRA? <<<");
  delay(3000);

  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  ledcWrite(ENB, 0);

  Serial.println("  >>> MOTOR B APAGADO <<<");
  waitSerial("Continuar al Test 7 (Ambos motores)");
}

// ═══════════════════════════════════════════════════════════
//  TEST 7: Ambos motores adelante
// ═══════════════════════════════════════════════════════════
void test_7_ambos_adelante() {
  Serial.println("\n══ TEST 7: AMBOS MOTORES - ADELANTE ══");
  Serial.println("Motor A: IN1=LOW, IN2=HIGH");
  Serial.println("Motor B: IN3=HIGH, IN4=LOW");
  Serial.println("ENA=200, ENB=200\n");

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);
  digitalWrite(IN3, HIGH);
  digitalWrite(IN4, LOW);
  ledcWrite(ENA, 200);
  ledcWrite(ENB, 200);

  Serial.println("  >>> AMBOS MOTORES ENCENDIDOS <<<");
  Serial.println("  El vehículo debería avanzar recto.");
  delay(4000);

  // Parar todo
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW);
  digitalWrite(IN4, LOW);
  ledcWrite(ENA, 0);
  ledcWrite(ENB, 0);

  Serial.println("  >>> MOTORES APAGADOS <<<");
  waitSerial("Continuar al Test 8 (Velocidad variable)");
}

// ═══════════════════════════════════════════════════════════
//  TEST 8: Rampa de velocidad
// ═══════════════════════════════════════════════════════════
void test_8_velocidad_variable() {
  Serial.println("\n══ TEST 8: RAMPA DE VELOCIDAD ══");
  Serial.println("Motor A sube de 0 a 255 gradualmente.\n");

  digitalWrite(IN1, LOW);
  digitalWrite(IN2, HIGH);

  for (int v = 0; v <= 255; v += 5) {
    ledcWrite(ENA, v);
    Serial.println("  Velocidad: " + String(v) + "/255 (" + String(v * 100 / 255) + "%)");
    delay(200);
  }

  // Parar
  ledcWrite(ENA, 0);
  digitalWrite(IN1, LOW);
  digitalWrite(IN2, LOW);

  Serial.println("\n  >>> MOTOR A APAGADO <<<");
  Serial.println("  ¿Escuchaste el motor acelerar gradualmente?");
  Serial.println("  SÍ → El PWM funciona correctamente");
  Serial.println("  NO pero giró a máxima → El PWM no genera señal variable");
  waitSerial("Finalizar pruebas");
}
