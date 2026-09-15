#include <Arduino.h>

void setup() {
  Serial.begin(115200);
  
  // Der Core 3.x schaltet den Pin intern passend für RGB frei
  pinMode(RGB_BUILTIN, OUTPUT); 
}

void loop() {
  Serial.println("Sende Signal: LED AN");
  
  // Trick des Core 3.x: rgbLedWrite steuert die Kanäle direkt an
  // Parameter: (Pin, Rot, Grün, Blau) -> Werte von 0 bis 255
  rgbLedWrite(RGB_BUILTIN, 60, 0, 0); // Leichtes Rot senden
  delay(1000);

  Serial.println("Sende Signal: LED AUS");
  rgbLedWrite(RGB_BUILTIN, 0, 0, 0); // Alle Farben aus
  delay(1000);
}
