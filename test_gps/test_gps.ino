#include <TinyGPSPlus.h>
#include <HardwareSerial.h>

TinyGPSPlus gps;
HardwareSerial serialGPS(1);

#define GPS_RX 32   // Aquí conectas el TX del GPS

void setup() {
  Serial.begin(115200);
  delay(1000);

  Serial.println("TEST GPS ATGM336H - 1 modulo");
  Serial.println("GPS TX -> GPIO32");
  Serial.println("Esperando datos NMEA...");

  serialGPS.begin(9600, SERIAL_8N1, GPS_RX, -1);
}

void loop() {
  while (serialGPS.available()) {
    gps.encode(serialGPS.read());
  }

  static unsigned long lastPrint = 0;

  if (millis() - lastPrint >= 3000) {
    lastPrint = millis();

    Serial.print("Chars recibidos: ");
    Serial.print(gps.charsProcessed());
    Serial.print(" | ");

    if (gps.charsProcessed() < 10) {
      Serial.println("SIN DATOS - revisa que TX del GPS vaya a GPIO32");
      return;
    }

    if (gps.location.isValid()) {
      Serial.print("FIX OK | Lat: ");
      Serial.print(gps.location.lat(), 6);
      Serial.print(" | Lon: ");
      Serial.print(gps.location.lng(), 6);
      Serial.print(" | Sats: ");
      Serial.println(gps.satellites.isValid() ? gps.satellites.value() : 0);
    } else {
      Serial.println("Sin fix - llevar a exteriores y esperar");
    }
  }
}