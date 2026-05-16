// ============================================================
// SMART FARM INTELLIGENCE SYSTEM
// Node 1 — Soil Monitoring
// NMIT ECE Final Year Project 2025-26
// File: firmware/node1_soil/node1_soil.ino
// ============================================================
// Sensors:
//   - Soil moisture sensor  → GPIO 34
//   - Soil pH sensor        → GPIO 35 (analog)
// Actuators:
//   - Relay (water pump)    → GPIO 26
//   - LED indicator         → GPIO 2
// Communication:
//   - WiFi → sends data to central node via HTTP
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ---- WiFi Credentials ----
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// ---- Central Node IP (Node 4 acts as central receiver) ----
const char* CENTRAL_IP    = "http://192.168.1.100/receive";

// ---- Pin Configuration ----
#define SOIL_MOISTURE_PIN   34
#define SOIL_PH_PIN         35
#define RELAY_PIN           26
#define LED_PIN              2

// ---- Learned Thresholds (updated by AI engine) ----
float SOIL_DRY_THRESHOLD  = 48.0;   // pump ON below this
float SOIL_WET_THRESHOLD  = 72.0;   // pump OFF above this
float PH_LOW_THRESHOLD    = 5.5;    // acidic alert
float PH_HIGH_THRESHOLD   = 7.5;    // alkaline alert

// ---- Variables ----
float soilMoisture  = 0;
float soilPH        = 0;
bool  pumpStatus    = false;
String stressType   = "none";
int   readingCount  = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(RELAY_PIN, OUTPUT);
  pinMode(LED_PIN,   OUTPUT);
  digitalWrite(RELAY_PIN, LOW);
  digitalWrite(LED_PIN,   LOW);

  Serial.println("====================================");
  Serial.println("  Smart Farm — Node 1 Soil Monitor");
  Serial.println("====================================");

  connectWiFi();
}

// ============================================================
void loop() {
  readingCount++;
  Serial.printf("\n--- Reading #%d ---\n", readingCount);

  readSoilMoisture();
  readSoilPH();
  detectStress();
  controlPump();
  printStatus();

  // Send data every reading
  if (WiFi.status() == WL_CONNECTED) {
    sendDataToCentral();
  } else {
    Serial.println("[!] WiFi disconnected. Reconnecting...");
    connectWiFi();
  }

  delay(30000); // Read every 30 seconds
}

// ============================================================
void readSoilMoisture() {
  int rawValue    = analogRead(SOIL_MOISTURE_PIN);
  soilMoisture    = map(rawValue, 4095, 0, 0, 100);
  soilMoisture    = constrain(soilMoisture, 0, 100);
  Serial.printf("[Soil Moisture] Raw: %d  Percent: %.1f%%\n",
                rawValue, soilMoisture);
}

// ============================================================
void readSoilPH() {
  int rawValue  = analogRead(SOIL_PH_PIN);
  // Convert ADC to pH (calibrated formula)
  soilPH        = 3.5 * (rawValue / 4095.0) * 3.3 + 3.5;
  soilPH        = constrain(soilPH, 0, 14);
  Serial.printf("[Soil pH]       Raw: %d  pH: %.2f\n",
                rawValue, soilPH);
}

// ============================================================
void detectStress() {
  if (soilMoisture < SOIL_DRY_THRESHOLD) {
    stressType = "water_stress";
    Serial.println("[!] WATER STRESS DETECTED");
  } else if (soilPH < PH_LOW_THRESHOLD) {
    stressType = "ph_acidic";
    Serial.println("[!] SOIL ACIDIC — pH TOO LOW");
  } else if (soilPH > PH_HIGH_THRESHOLD) {
    stressType = "ph_alkaline";
    Serial.println("[!] SOIL ALKALINE — pH TOO HIGH");
  } else if (soilMoisture < SOIL_DRY_THRESHOLD + 10) {
    stressType = "mild_stress";
    Serial.println("[~] Mild stress — moisture getting low");
  } else {
    stressType = "none";
    Serial.println("[OK] Soil condition normal");
  }
}

// ============================================================
void controlPump() {
  if (stressType == "water_stress" && !pumpStatus) {
    digitalWrite(RELAY_PIN, HIGH);
    digitalWrite(LED_PIN,   HIGH);
    pumpStatus = true;
    Serial.println("[PUMP] Turned ON — soil dry");
  } else if (soilMoisture >= SOIL_WET_THRESHOLD && pumpStatus) {
    digitalWrite(RELAY_PIN, LOW);
    digitalWrite(LED_PIN,   LOW);
    pumpStatus = false;
    Serial.println("[PUMP] Turned OFF — soil wet enough");
  }
}

// ============================================================
void printStatus() {
  Serial.println("------------------------------------");
  Serial.printf("Soil Moisture : %.1f%%\n",  soilMoisture);
  Serial.printf("Soil pH       : %.2f\n",    soilPH);
  Serial.printf("Pump Status   : %s\n",      pumpStatus ? "ON" : "OFF");
  Serial.printf("Stress Type   : %s\n",      stressType.c_str());
  Serial.printf("Threshold     : %.1f%%\n",  SOIL_DRY_THRESHOLD);
  Serial.println("------------------------------------");
}

// ============================================================
void sendDataToCentral() {
  HTTPClient http;
  http.begin(CENTRAL_IP);
  http.addHeader("Content-Type", "application/json");

  // Build JSON payload
  StaticJsonDocument<256> doc;
  doc["node"]           = "node1_soil";
  doc["soil_moisture"]  = soilMoisture;
  doc["soil_ph"]        = soilPH;
  doc["pump_status"]    = pumpStatus;
  doc["stress_type"]    = stressType;
  doc["threshold"]      = SOIL_DRY_THRESHOLD;

  String payload;
  serializeJson(doc, payload);

  int responseCode = http.POST(payload);
  if (responseCode > 0) {
    Serial.printf("[WiFi] Data sent. Response: %d\n", responseCode);
  } else {
    Serial.printf("[WiFi] Send failed. Error: %d\n", responseCode);
  }
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
    Serial.printf("[WiFi] IP Address: %s\n", WiFi.localIP().toString().c_str());
  } else {
    Serial.println("\n[WiFi] Connection failed. Running offline.");
  }
}
