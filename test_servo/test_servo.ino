/*
 * ============================================================
 *  BRICHS - Test de Servo para ESP32
 * ============================================================
 *  PIN RECOMENDADO: GPIO 18 (o cualquier pin PWM)
 *  LIBRERÍA: ESP32Servo
 * ============================================================
 */

#include <ESP32Servo.h>

Servo myServo;
int servoPin = 18; // Cambia este pin según tu conexión

void setup() {
  Serial.begin(115200);
  
  // Permitir que el temporizador PWM se asigne automáticamente
  ESP32PWM::allocateTimer(0);
  ESP32PWM::allocateTimer(1);
  ESP32PWM::allocateTimer(2);
  ESP32PWM::allocateTimer(3);
  
  myServo.setPeriodHertz(50);    // Frecuencia estándar de 50Hz para servos
  myServo.attach(servoPin, 500, 2400); // Attach con min/max pulsos en microsegundos
  
  Serial.println("Servo inicializado en GPIO " + String(servoPin));
}

void loop() {
  Serial.println("Moviendo a 0 grados");
  myServo.write(0);
  delay(1000);
  
  Serial.println("Moviendo a 90 grados");
  myServo.write(90);
  delay(1000);
  
  Serial.println("Moviendo a 180 grados");
  myServo.write(180);
  delay(1000);
  
  // Barrido suave (Sweep)
  Serial.println("Iniciando barrido...");
  for (int pos = 0; pos <= 180; pos += 1) {
    myServo.write(pos);
    delay(15);
  }
  for (int pos = 180; pos >= 0; pos -= 1) {
    myServo.write(pos);
    delay(15);
  }
}
