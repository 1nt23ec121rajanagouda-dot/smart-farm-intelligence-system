// ============================================================
// SMART FARM INTELLIGENCE SYSTEM
// Node 2 — Environment Monitoring
// NMIT ECE Final Year Project 2025-26
// File: firmware/node2_environment/node2_environment.ino
// ============================================================
// Sensors:
//   - DHT22 temp/humidity   → GPIO 4
//   - LDR light sensor      → GPIO 32
//   - Rain sensor           → GPIO 33
// Communication:
//   - WiFi → sends data to central node
// ============================================================

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <DHT.h>

// ---- WiFi Credentials ----
const char* WIFI_SSID     = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";
const char* CENTRAL_IP    = "http://192.168.1.100/receive";

// ---- Pin Configuration ----
#define DHT_PIN       4
#define DHT_TYPE      DHT22
#define LDR_PIN       32
#define RAIN_PIN      33
#define LED_PIN        2

// ---- Sensor Object ----
DHT dht(DHT_PIN, DHT_TYPE);

// ---- Learned Thresholds ----
float TEMP_HIGH_THRESHOLD   = 38.0;  // heat stress
float TEMP_LOW_THRESHOLD    = 10.0;  // cold stress
float HUMIDITY_HIGH         = 92.0;  // disease risk
float HUMIDITY_LOW          = 20.0;  // drought risk
int   LIGHT_LOW_THRESHOLD   = 200;   // low light stress

// ---- Variables ----
float temperature   = 0;
float humidity      = 0;
int   lightLevel    = 0;
bool  isRaining     = false;
String stressType   = "none";
String envCondition = "normal";
int   readingCount  = 0;

// ============================================================
void setup() {
  Serial.begin(115200);
  dht.begin();
  pinMode(RAIN_PIN, INPUT);
  pinMode(LED_PIN,  OUTPUT);

  Serial.println("==========================================");
  Serial.println("  Smart Farm — Node 2 Environment Monitor");
  Serial.println("==========================================");

  connectWiFi();
}

// ============================================================
void loop() {
  readingCount++;
  Serial.printf("\n--- Reading #%d ---\n", readingCount);

  readDHT22();
  readLightSensor();
  readRainSensor();
  detectEnvironmentStress();
  printStatus();

  if (WiFi.status() == WL_CONNECTED) {
    sendDataToCentral();
  } else {
    connectWiFi();
  }

  delay(30000);
}

// ============================================================
void readDHT22() {
  temperature = dht.readTemperature();
  humidity    = dht.readHumidity();

  if (isnan(temperature) || isnan(humidity)) {
    Serial.println("[DHT22] Read failed. Retrying...");
    delay(2000);
    temperature = dht.readTemperature();
    humidity    = dht.readHumidity();
  }

  Serial.printf("[DHT22]  Temp: %.1f°C   Humidity: %.1f%%\n",
                temperature, humidity);
}

// ============================================================
void readLightSensor() {
  int rawValue = analogRead(LDR_PIN);
  // Convert to lux approximation
  lightLevel   = map(rawValue, 0, 4095, 0, 1000);
  Serial.printf("[LDR]    Light: %d lux\n", lightLevel);
}

// ============================================================
void readRainSensor() {
  int rainRaw = analogRead(RAIN_PIN);
  isRaining   = (rainRaw < 1000);
  Serial.printf("[Rain]   %s (raw: %d)\n",
                isRaining ? "RAINING" : "No rain", rainRaw);
}

// ============================================================
void detectEnvironmentStress() {
  if (temperature > TEMP_HIGH_THRESHOLD) {
    stressType   = "heat_stress";
    envCondition = "critical";
    Serial.println("[!] HEAT STRESS — temperature too high");
    digitalWrite(LED_PIN, HIGH);
  } else if (temperature < TEMP_LOW_THRESHOLD) {
    stressType   = "cold_stress";
    envCondition = "warning";
    Serial.println("[!] COLD STRESS — temperature too low");
  } else if (humidity > HUMIDITY_HIGH && !isRaining) {
    stressType   = "disease_risk";
    envCondition = "warning";
    Serial.println("[!] DISEASE RISK — humidity too high");
  } else if (humidity < HUMIDITY_LOW) {
    stressType   = "drought_risk";
    envCondition = "warning";
    Serial.println("[!] DROUGHT RISK — humidity too low");
  } else if (lightLevel < LIGHT_LOW_THRESHOLD && !isRaining) {
    stressType   = "light_stress";
    envCondition = "mild";
    Serial.println("[~] LIGHT STRESS — insufficient sunlight");
  } else {
    stressType   = "none";
    envCondition = "normal";
    digitalWrite(LED_PIN, LOW);
    Serial.println("[OK] Environment normal");
  }
}

// ============================================================
void printStatus() {
  Serial.println("------------------------------------");
  Serial.printf("Temperature  : %.1f°C\n",  temperature);
  Serial.printf("Humidity     : %.1f%%\n",  humidity);
  Serial.printf("Light Level  : %d lux\n",  lightLevel);
  Serial.printf("Raining      : %s\n",      isRaining ? "Yes" : "No");
  Serial.printf("Stress Type  : %s\n",      stressType.c_str());
  Serial.printf("Condition    : %s\n",      envCondition.c_str());
  Serial.println("------------------------------------");
}

// ============================================================
void sendDataToCentral() {
  HTTPClient http;
  http.begin(CENTRAL_IP);
  http.addHeader("Content-Type", "application/json");

  StaticJsonDocument<256> doc;
  doc["node"]         = "node2_environment";
  doc["temperature"]  = temperature;
  doc["humidity"]     = humidity;
  doc["light_level"]  = lightLevel;
  doc["is_raining"]   = isRaining;
  doc["stress_type"]  = stressType;
  doc["condition"]    = envCondition;

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
