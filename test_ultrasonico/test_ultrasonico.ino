/*
 * ============================================================
 *  BRICHS - Test de Sensor Ultrasónico (HC-SR04) para ESP32
 * ============================================================
 *  PINES RECOMENDADOS:
 *    Trig: GPIO 5
 *    Echo: GPIO 18
 *  DISTANCIA DE ALERTA: 10 cm
 * ============================================================
 */

#define TRIG_PIN 5
#define ECHO_PIN 18

void setup() {
  Serial.begin(115200);
  pinMode(TRIG_PIN, OUTPUT);
  pinMode(ECHO_PIN, INPUT);
  
  Serial.println("Sensor Ultrasónico Inicializado");
  Serial.println("Alerta configurada a 10 cm");
}

void loop() {
  // Limpiar el pin Trig
  digitalWrite(TRIG_PIN, LOW);
  delayMicroseconds(2);
  
  // Lanzar el pulso de 10 microsegundos
  digitalWrite(TRIG_PIN, HIGH);
  delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  // Leer el tiempo del pulso de retorno (en microsegundos)
  long duracion = pulseIn(ECHO_PIN, HIGH);
  
  // Calcular distancia en cm
  // Velocidad del sonido = 343 m/s -> 0.0343 cm/us
  // Distancia = (tiempo * velocidad) / 2 (ida y vuelta)
  float distancia = duracion * 0.0343 / 2;
  
  // Mostrar en el Monitor Serial
  Serial.print("Distancia: ");
  Serial.print(distancia);
  Serial.print(" cm");
  
  // Detectar si hay algo a 10cm o menos
  if (distancia > 0 && distancia <= 10) {
    Serial.println("  <<< ¡DETECCION A 10CM! >>>");
  } else {
    Serial.println("");
  }
  
  delay(200); // Pequeña pausa para no saturar el Serial
}
