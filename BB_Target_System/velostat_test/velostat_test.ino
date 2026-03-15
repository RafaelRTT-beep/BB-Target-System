// ============================================
// Velostat Simpele Test
// ============================================
// Test 1 rij + 1 kolom om te checken of je
// velostat signaal geeft. Geen WiFi, geen matrix.
//
// Bedrading:
//   GPIO 4  → rij-strip (kopertape bovenkant)
//   GPIO 36 → kolom-strip (kopertape onderkant)
//   10kΩ weerstand van GPIO 36 naar GND
//
// Open Serial Monitor op 115200 baud
// en druk/tik op de velostat.
// ============================================

#define ROW_PIN   4     // Output naar rij-strip
#define COL_PIN   36    // ADC input van kolom-strip
#define BAUD      115200

int baseline = 0;

void setup() {
  Serial.begin(BAUD);
  delay(500);
  Serial.println("\n=== Velostat Simpele Test ===");
  Serial.println("Druk op de velostat en kijk naar de waarden.\n");

  pinMode(ROW_PIN, OUTPUT);
  digitalWrite(ROW_PIN, LOW);

  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);

  // Kalibreer baseline (niet aanraken!)
  Serial.println("Kalibreren... niet aanraken!");
  delay(1000);
  long sum = 0;
  for (int i = 0; i < 50; i++) {
    digitalWrite(ROW_PIN, HIGH);
    delayMicroseconds(100);
    sum += analogRead(COL_PIN);
    digitalWrite(ROW_PIN, LOW);
    delay(10);
  }
  baseline = sum / 50;
  Serial.printf("Baseline: %d\n\n", baseline);
  Serial.println("RAW\tBASELINE\tVERSCHIL");
  Serial.println("---\t--------\t--------");
}

void loop() {
  digitalWrite(ROW_PIN, HIGH);
  delayMicroseconds(100);
  int raw = analogRead(COL_PIN);
  digitalWrite(ROW_PIN, LOW);

  int diff = raw - baseline;
  if (diff < 0) diff = 0;

  // Simpele balk visualisatie
  char bar[51];
  int barLen = min(diff / 2, 50);
  for (int i = 0; i < barLen; i++) bar[i] = '#';
  bar[barLen] = '\0';

  Serial.printf("%d\t%d\t\t%d\t%s\n", raw, baseline, diff, bar);

  delay(100);  // 10x per seconde
}
