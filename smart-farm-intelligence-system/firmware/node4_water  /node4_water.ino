// ============================================================
// SMART FARM INTELLIGENCE SYSTEM
// Node 4 — Water Management + Central Coordinator
// NMIT ECE Final Year Project 2025-26
// File: firmware/node4_water/node4_water.ino
// ============================================================
// This node does two jobs:
// 1. Monitors water level and controls pump
// 2. Acts as central receiver — collects data from all nodes
//
// Sensors:
//   - Water level sensor    → GPIO 36
//   - Flow sensor           → GPIO 27
// Actuators:
//   - Main pump relay       → GPIO 26
//   - Alert buzzer          → GPIO 25
//   - LCD display           → I2C (SDA=21, SCL=22)
// Communication:
//   - WiFi server — receives data from Node 1, 2, 3
//   - Sends combined data to Blynk cloud
// ============================================================

#include <WiFi.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <LiquidCrystal_I2C.h>
#include <BlynkSimpleEsp32.h>

// ---- WiFi + Blynk Credentials ----
const char* WIFI_SSID       = "YOUR_WIFI_NAME";
const char* WIFI_PASSWORD   = "YOUR_WIFI_PASSWORD";
char        BLYNK_AUTH[]    = "YOUR_BLYNK_AUTH_TOKEN";

// ---- Pin Configuration ----
#define WATER_LEVEL_PIN   36
#define FLOW_SENSOR_PIN   27
#define PUMP_RELAY_PIN    26
#define BUZZER_PIN        25

// ---- LCD Setup (16x2) ----
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ---- Web Server (receives data from other nodes) ----
WebServer server(80);

// ---- Thresholds ----
float WATER_LEVEL_MIN   = 20.0;  // refill alert
float WATER_LEVEL_LOW   = 35.0;  // warning level

// ---- Water Sensor Variables ----
float waterLevel        = 0;
float flowRate          = 0;
bool  mainPumpStatus    = false;
int   readingCount      = 0;

// ---- Data received from other nodes ----
float node1_soilMoisture  = 0;
float node1_soilPH        = 0;
String node1_stress       = "unknown";
bool  node1_pump          = false;

float node2_temperature   = 0;
float node2_humidity      = 0;
int   node2_light         = 0;
String node2_stress       = "unknown";

float node3_resistance    = 0;
float node3_leafWetness   = 0;
String node3_stress       = "unknown";

// ---- Combined farm status ----
String farmStatus         = "monitoring";
String overallStress      = "none";

// ============================================================
void setup() {
  Serial.begin(115200);
  pinMode(PUMP_RELAY_PIN, OUTPUT);
  pinMode(BUZZER_PIN,     OUTPUT);
  pinMode(FLOW_SENSOR_PIN,INPUT);
  digitalWrite(PUMP_RELAY_PIN, LOW);
  digitalWrite(BUZZER_PIN,     LOW);

  // LCD init
  lcd.init();
  lcd.backlight();
  lcd.setCursor(0, 0);
  lcd.print("Smart Farm v1.0");
  lcd.setCursor(0, 1);
  lcd.print("Starting...");

  Serial.println("==============================================");
  Serial.println("  Smart Farm — Node 4 Water + Central Unit");
  Serial.println("==============================================");

  connectWiFi();
  startWebServer();

  Blynk.begin(BLYNK_AUTH, WIFI_SSID, WIFI_PASSWORD);

  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("System Ready");
}

// ============================================================
void loop() {
  Blynk.run();
  server.handleClient();

  readingCount++;

  readWaterLevel();
  readFlowRate();
  determineOverallStatus();
  controlMainPump();
  updateLCD();
  sendToBlynk();
  printCentralStatus();

  delay(30000);
}

// ============================================================
void readWaterLevel() {
  int raw    = analogRead(WATER_LEVEL_PIN);
  waterLevel = map(raw, 0, 4095, 0, 100);
  waterLevel = constrain(waterLevel, 0, 100);
  Serial.printf("[Water]  Level: %.1f%%\n", waterLevel);
}

// ============================================================
void readFlowRate() {
  // Flow sensor pulse counting simplified
  int pulses = 0;
  unsigned long start = millis();
  while (millis() - start < 1000) {
    if (digitalRead(FLOW_SENSOR_PIN) == HIGH) pulses++;
    delay(1);
  }
  flowRate = pulses / 7.5;  // Convert to L/min
  Serial.printf("[Flow]   Rate: %.2f L/min\n", flowRate);
}

// ============================================================
void determineOverallStatus() {
  // Combine stress from all nodes
  if (node3_stress == "water_heat_stress" || node3_stress == "disease_risk") {
    overallStress = node3_stress;
    farmStatus    = "critical";
  } else if (node1_stress == "water_stress") {
    overallStress = "water_stress";
    farmStatus    = "alert";
  } else if (node2_stress == "heat_stress") {
    overallStress = "heat_stress";
    farmStatus    = "alert";
  } else if (node2_stress == "disease_risk" || node3_stress == "fungal_risk") {
    overallStress = "disease_risk";
    farmStatus    = "warning";
  } else if (waterLevel < WATER_LEVEL_MIN) {
    overallStress = "low_water";
    farmStatus    = "warning";
  } else {
    overallStress = "none";
    farmStatus    = "normal";
  }

  Serial.printf("[Status] Farm: %s  Stress: %s\n",
                farmStatus.c_str(), overallStress.c_str());
}

// ============================================================
void controlMainPump() {
  if (waterLevel < WATER_LEVEL_MIN) {
    // Emergency — tank too low
    if (mainPumpStatus) {
      digitalWrite(PUMP_RELAY_PIN, LOW);
      mainPumpStatus = false;
      Serial.println("[!] PUMP OFF — water tank critically low");
      soundAlert(3);
    }
  } else if (node1_pump && !mainPumpStatus && waterLevel > WATER_LEVEL_LOW) {
    digitalWrite(PUMP_RELAY_PIN, HIGH);
    mainPumpStatus = true;
    Serial.println("[PUMP] Main pump ON");
  } else if (!node1_pump && mainPumpStatus) {
    digitalWrite(PUMP_RELAY_PIN, LOW);
    mainPumpStatus = false;
    Serial.println("[PUMP] Main pump OFF");
  }
}

// ============================================================
void updateLCD() {
  lcd.clear();
  // Line 1: soil and temp
  lcd.setCursor(0, 0);
  lcd.printf("S:%d%% T:%.0fC",
             (int)node1_soilMoisture,
             node2_temperature);
  // Line 2: farm status
  lcd.setCursor(0, 1);
  if (overallStress == "none") {
    lcd.print("Farm: Normal    ");
  } else {
    lcd.print(overallStress.substring(0, 16));
  }
}

// ============================================================
void sendToBlynk() {
  // Virtual pins for Blynk dashboard
  Blynk.virtualWrite(V0, node1_soilMoisture);   // Soil moisture
  Blynk.virtualWrite(V1, node2_temperature);    // Temperature
  Blynk.virtualWrite(V2, node2_humidity);       // Humidity
  Blynk.virtualWrite(V3, node2_light);          // Light
  Blynk.virtualWrite(V4, node3_resistance);     // Plant resistance
  Blynk.virtualWrite(V5, waterLevel);           // Water level
  Blynk.virtualWrite(V6, mainPumpStatus ? 1:0); // Pump status
  Blynk.virtualWrite(V7, overallStress);        // Stress type
  Blynk.virtualWrite(V8, node1_soilPH);         // Soil pH
  Blynk.virtualWrite(V9, node3_leafWetness);    // Leaf wetness
}

// ============================================================
// Web Server — receives JSON from other nodes
// ============================================================
void startWebServer() {
  server.on("/receive", HTTP_POST, []() {
    if (server.hasArg("plain")) {
      String body = server.arg("plain");
      StaticJsonDocument<256> doc;
      DeserializationError err = deserializeJson(doc, body);

      if (!err) {
        String nodeName = doc["node"].as<String>();

        if (nodeName == "node1_soil") {
          node1_soilMoisture  = doc["soil_moisture"];
          node1_soilPH        = doc["soil_ph"];
          node1_stress        = doc["stress_type"].as<String>();
          node1_pump          = doc["pump_status"];
          Serial.println("[Received] Node 1 data updated");
        }
        else if (nodeName == "node2_environment") {
          node2_temperature   = doc["temperature"];
          node2_humidity      = doc["humidity"];
          node2_light         = doc["light_level"];
          node2_stress        = doc["stress_type"].as<String>();
          Serial.println("[Received] Node 2 data updated");
        }
        else if (nodeName == "node3_biosignal") {
          node3_resistance    = doc["plant_resistance"];
          node3_leafWetness   = doc["leaf_wetness"];
          node3_stress        = doc["stress_type"].as<String>();
          Serial.println("[Received] Node 3 data updated");
        }

        server.send(200, "application/json", "{\"status\":\"ok\"}");
      } else {
        server.send(400, "application/json", "{\"error\":\"bad json\"}");
      }
    }
  });

  server.begin();
  Serial.println("[Server] Central receiver started on port 80");
}

// ============================================================
void printCentralStatus() {
  Serial.println("============================================");
  Serial.println("  CENTRAL FARM STATUS");
  Serial.println("--------------------------------------------");
  Serial.printf("  Node 1 — Soil     : %.1f%%  pH:%.1f  %s\n",
                node1_soilMoisture, node1_soilPH, node1_stress.c_str());
  Serial.printf("  Node 2 — Env      : %.1f°C  %.1f%%  %s\n",
                node2_temperature, node2_humidity, node2_stress.c_str());
  Serial.printf("  Node 3 — Bio      : %.1fΩ  Leaf:%.1f%%  %s\n",
                node3_resistance, node3_leafWetness, node3_stress.c_str());
  Serial.printf("  Node 4 — Water    : %.1f%%  Flow:%.2fL/m\n",
                waterLevel, flowRate);
  Serial.println("--------------------------------------------");
  Serial.printf("  Overall Status    : %s\n",  farmStatus.c_str());
  Serial.printf("  Overall Stress    : %s\n",  overallStress.c_str());
  Serial.printf("  Main Pump         : %s\n",  mainPumpStatus ? "ON" : "OFF");
  Serial.println("============================================");
}

// ============================================================
void soundAlert(int times) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(300);
    digitalWrite(BUZZER_PIN, LOW);
    delay(300);
  }
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
    Serial.println("[WiFi] Other nodes should send data to this IP");
  } else {
    Serial.println("\n[WiFi] Failed.");
  }
}
