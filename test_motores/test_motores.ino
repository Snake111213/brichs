/*
 * TEST CON LED INTEGRADO
 * 
 * Conecta IN1 del L298N → GPIO 2 del ESP32 (pin del LED azul)
 * Conecta IN2 del L298N → GND del ESP32
 * Jumpers ENA/ENB puestos en el L298N
 * Motor en OUT1/OUT2
 * 
 * Si el LED azul parpadea PERO el motor no gira:
 *   → El cable del ESP32 al L298N no llega bien
 * 
 * Si el LED azul parpadea Y el motor gira:
 *   → Antes estabas conectando al pin equivocado
 */

#define LED_PIN  2   // LED azul integrado del ESP32

void setup() {
  Serial.begin(115200);
  delay(1000);
  Serial.println("\n=================================");
  Serial.println("  TEST CON LED - GPIO 2");
  Serial.println("  El LED azul debe parpadear");
  Serial.println("=================================\n");
  
  pinMode(LED_PIN, OUTPUT);
}

void loop() {
  Serial.println(">> LED ON  (motor deberia girar)");
  digitalWrite(LED_PIN, HIGH);
  delay(3000);

  Serial.println(">> LED OFF (motor deberia parar)");
  digitalWrite(LED_PIN, LOW);
  delay(2000);
}
