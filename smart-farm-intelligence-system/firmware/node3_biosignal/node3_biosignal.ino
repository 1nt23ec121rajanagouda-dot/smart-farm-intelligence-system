// ============================================================
// SMART FARM INTELLIGENCE SYSTEM
// Node 3 — Plant Bio-Signal Monitoring (UNIQUE INNOVATION)
// NMIT ECE Final Year Project 2025-26
// File: firmware/node3_biosignal/node3_biosignal.ino
// ============================================================
// This is the most innovative node in the system.
// It monitors the plant ITSELF — not just the environment.
//
// How it works:
//   Two copper electrodes are attached to a plant leaf/stem.
//   The plant tissue acts as a variable resistor.
//   When the plant is healthy  → stable resistance ~320Ω
//   When the plant is stressed → resistance changes
//   ESP32 reads this via a voltage divider circuit.
//
// Circuit:
//   3.3V → [10kΩ resistor] → [Analog Pin 34] → [Plant] → GND
//
// Sensors:
//   - Plant electrode 1     → GPIO 34 (analog)
//   - Leaf wetness sensor   → GPIO 35 (analog)
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---- WiFi Credentials ----
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* CENTRAL_IP    = "http://192.168.1.100/receive";

// ---- Pin Configuration ----
#define ELECTRODE_PIN     34   // Plant bio-signal
#define LEAF_WETNESS_PIN  35   // Leaf wetness sensor
#define LED_PIN            2   // Status LED
#define BUZZER_PIN        27   // Alert buzzer

// ---- Known Resistor in Voltage Divider ----
#define KNOWN_RESISTOR    10000.0  // 10kΩ

// ---- Thresholds (learned by AI engine) ----
float BASELINE_RESISTANCE     = 320.0;  // healthy plant Ω
float STRESS_THRESHOLD_HIGH   = 450.0;  // above = water/heat stress
float STRESS_THRESHOLD_LOW    = 250.0;  // below = disease risk
float LEAF_WET_THRESHOLD      = 60.0;   // above = disease risk

// ---- Variables ----
float plantResistance   = 0;
float leafWetness       = 0;
float baselineAvg       = 0;
String stressType       = "none";
String stressLevel      = "none";
int   readingCount      = 0;
bool  baselineSet       = false;

// ---- Baseline calibration buffer ----
#define BASELINE_SAMPLES  10
float baselineBuffer[BASELINE_SAMPLES];
int   baselineIndex = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN,    OUTPUT);
  pinMode(BUZZER_PIN, OUTPUT);
  digitalWrite(LED_PIN,    LOW);
  digitalWrite(BUZZER_PIN, LOW);

  Serial.println("==============================================");
  Serial.println("  Smart Farm — Node 3 Plant Bio-Signal Monitor");
  Serial.println("  UNIQUE INNOVATION: Monitors the plant itself");
  Serial.println("==============================================");

  Serial.println("\n[Setup] Calibrating baseline...");
  calibrateBaseline();

  connectWiFi();
}

// ============================================================
void loop() {
  readingCount++;
  Serial.printf("\n--- Reading #%d ---\n", readingCount);

  readPlantResistance();
  readLeafWetness();
  detectBioStress();
  triggerAlert();
  printStatus();

  if (WiFi.status() == WL_CONNECTED) {
    sendDataToCentral();
  } else {
    connectWiFi();
  }

  // Update baseline every 20 readings
  if (readingCount % 20 == 0) {
    updateBaseline();
  }

  delay(30000);
}

// ============================================================
void calibrateBaseline() {
  Serial.println("[Calibration] Reading baseline resistance...");
  float sum = 0;
  for (int i = 0; i < BASELINE_SAMPLES; i++) {
    int raw = analogRead(ELECTRODE_PIN);
    float voltage  = raw * (3.3 / 4095.0);
    float resistance = 0;
    if (voltage > 0 && voltage < 3.3) {
      resistance = KNOWN_RESISTOR * voltage / (3.3 - voltage);
    }
    sum += resistance;
    baselineBuffer[i] = resistance;
    delay(200);
  }
  baselineAvg  = sum / BASELINE_SAMPLES;
  baselineSet  = true;
  BASELINE_RESISTANCE = baselineAvg;
  Serial.printf("[Calibration] Baseline set: %.1f Ω\n", baselineAvg);
}

// ============================================================
void readPlantResistance() {
  // Take average of 5 readings to reduce noise
  float sum = 0;
  for (int i = 0; i < 5; i++) {
    int raw = analogRead(ELECTRODE_PIN);
    float voltage = raw * (3.3 / 4095.0);
    if (voltage > 0.1 && voltage < 3.2) {
      sum += KNOWN_RESISTOR * voltage / (3.3 - voltage);
    } else {
      sum += BASELINE_RESISTANCE;
    }
    delay(50);
  }
  plantResistance = sum / 5.0;
  Serial.printf("[Electrode]  Plant resistance: %.1f Ω  (baseline: %.1f Ω)\n",
                plantResistance, BASELINE_RESISTANCE);
}

// ============================================================
void readLeafWetness() {
  int raw    = analogRead(LEAF_WETNESS_PIN);
  leafWetness = map(raw, 4095, 0, 0, 100);
  leafWetness = constrain(leafWetness, 0, 100);
  Serial.printf("[Leaf]       Leaf wetness: %.1f%%\n", leafWetness);
}

// ============================================================
void detectBioStress() {
  float deviation = plantResistance - BASELINE_RESISTANCE;
  float deviationPct = (deviation / BASELINE_RESISTANCE) * 100;

  Serial.printf("[Analysis]   Deviation from baseline: %.1f Ω  (%.1f%%)\n",
                deviation, deviationPct);

  if (plantResistance > STRESS_THRESHOLD_HIGH) {
    // High resistance = water or heat stress
    stressType  = "water_heat_stress";
    stressLevel = "high";
    Serial.println("[!] HIGH STRESS — resistance too high");
  } else if (plantResistance < STRESS_THRESHOLD_LOW) {
    // Low resistance = disease risk (high moisture in tissue)
    stressType  = "disease_risk";
    stressLevel = "high";
    Serial.println("[!] DISEASE RISK — resistance too low");
  } else if (leafWetness > LEAF_WET_THRESHOLD) {
    // Wet leaf for long time = fungal infection risk
    stressType  = "fungal_risk";
    stressLevel = "medium";
    Serial.println("[~] FUNGAL RISK — leaf too wet");
  } else if (abs(deviationPct) > 15) {
    // Any deviation above 15% = mild stress
    stressType  = "mild_stress";
    stressLevel = "low";
    Serial.println("[~] MILD STRESS — bio-signal changing");
  } else {
    stressType  = "none";
    stressLevel = "none";
    Serial.println("[OK] Plant bio-signal normal");
  }
}

// ============================================================
void triggerAlert() {
  if (stressLevel == "high") {
    // Fast blink + buzzer for high stress
    for (int i = 0; i < 3; i++) {
      digitalWrite(LED_PIN,    HIGH);
      digitalWrite(BUZZER_PIN, HIGH);
      delay(200);
      digitalWrite(LED_PIN,    LOW);
      digitalWrite(BUZZER_PIN, LOW);
      delay(200);
    }
  } else if (stressLevel == "medium") {
    // Slow blink for medium stress
    digitalWrite(LED_PIN, HIGH);
    delay(500);
    digitalWrite(LED_PIN, LOW);
  } else {
    digitalWrite(LED_PIN, LOW);
  }
}

// ============================================================
void updateBaseline() {
  // Slowly update baseline using moving average
  // Only update if reading is close to normal
  if (stressType == "none") {
    baselineAvg = (baselineAvg * 0.9) + (plantResistance * 0.1);
    Serial.printf("[Learning]   Baseline updated: %.1f Ω\n", baselineAvg);
  }
}

// ============================================================
void printStatus() {
  Serial.println("------------------------------------");
  Serial.printf("Plant Resistance : %.1f Ω\n",  plantResistance);
  Serial.printf("Baseline         : %.1f Ω\n",  baselineAvg);
  Serial.printf("Leaf Wetness     : %.1f%%\n",   leafWetness);
  Serial.printf("Stress Type      : %s\n",       stressType.c_str());
  Serial.printf("Stress Level     : %s\n",       stressLevel.c_str());
  Serial.printf("Reading Count    : %d\n",       readingCount);
  Serial.println("------------------------------------");
}

// ============================================================
void sendDataToCentral() {
  HTTPClient http;
  http.begin(CENTRAL_IP);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["node"]               = "node3_biosignal";
  doc["plant_resistance"]   = plantResistance;
  doc["baseline"]           = baselineAvg;
  doc["leaf_wetness"]       = leafWetness;
  doc["stress_type"]        = stressType;
  doc["stress_level"]       = stressLevel;

  String payload;
  serializeJson(doc, payload);

  int responseCode = http.POST(payload);
  Serial.printf("[WiFi] Response: %d\n", responseCode);
  http.end();
}

// ============================================================
void connectWiFi() {
  Serial.print("[WiFi] Connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\n[WiFi] Connected!");
    Serial.printf("[WiFi] IP: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Failed. Running offline.");
  }
}
