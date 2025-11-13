// ================================================================
// Project: Turn on the lamp at Fajr Adhan
// Board: ESP32
// Environment: PlatformIO
// ================================================================

// 1. Include the Arduino framework (required in PlatformIO)
#include <Arduino.h>

// 2. Include the necessary libraries
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "time.h"

// ----------------------------------------------------------------
//  3. Settings that need to be modified (same as before)
// ----------------------------------------------------------------
const char* ssid = "YOUR_WIFI_SSID";         //  (Enter your Wi-Fi name)
const char* password = "YOUR_WIFI_PASSWORD"; //  (Enter your Wi-Fi password)
String city = "Safi";                       //  (Enter your city)
String country = "morocco";             //  (Enter your country)
const long gmtOffset_sec = 3600;            //  (GMT +1)
const int daylightOffset_sec = 0;
const int RELAY_PIN = 23;                    //  (Relay pin)
const int LIGHT_DURATION_MINUTES = 30;       //  (Lamp ON duration in minutes)
// ----------------------------------------------------------------

// (Other global variables...)
int fajrHour = -1;
int fajrMinute = -1;
bool isLightOn = false;
unsigned long lightOnTime = 0;

// (Function declarations — Best practice in C++)
void setupWiFi();
void syncTime();
void fetchPrayerTimes();
void checkTimeAndControlLight();

// ================================================================
// Setup Function
// ================================================================
void setup() {
  Serial.begin(115200); 
  Serial.println("\n[+] Starting system (PlatformIO)...");

  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, LOW); 

  setupWiFi();
  syncTime();
  fetchPrayerTimes();
}

// ================================================================
// Main Loop Function
// ================================================================
void loop() {
  checkTimeAndControlLight();

  struct tm timeinfo;
  getLocalTime(&timeinfo);
  if (timeinfo.tm_hour == 0 && timeinfo.tm_min == 1) {
    Serial.println("[+] Midnight detected, updating prayer times...");
    fetchPrayerTimes();
    delay(65000); 
  }

  delay(10000); 
}

// ================================================================
// Helper Functions (same as previous, unchanged)
// ================================================================

// (Wi-Fi connection function)
void setupWiFi() {
  Serial.print("[+] Connecting to network: ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);
  int attempt = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    attempt++;
    if (attempt > 20) {
      Serial.println("\n[!] Connection failed. Restarting...");
      ESP.restart();
    }
  }
  Serial.println("\n[✔] Connected to Wi-Fi!");
  Serial.print("    IP Address: ");
  Serial.println(WiFi.localIP());
}

// (Time synchronization function)
void syncTime() {
  Serial.println("[+] Syncing time (NTP)...");
  configTime(gmtOffset_sec, daylightOffset_sec, "pool.ntp.org");
  
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[!] Failed to get time. Restarting...");
    delay(1000);
    ESP.restart();
  }
  Serial.println("[✔] Time synchronized successfully.");
  Serial.println(&timeinfo, "    Current time: %A, %B %d %Y %H:%M:%S");
}

// (Fetch prayer times function)
void fetchPrayerTimes() {
  Serial.println("[+] Fetching prayer times...");
  
  String urlCity = city;
  urlCity.replace(" ", "%20");
  String urlCountry = country;
  urlCountry.replace(" ", "%20");

  String apiUrl = "http://api.aladhan.com/v1/timingsByCity?city=" + urlCity + "&country=" + urlCountry + "&method=2";

  HTTPClient http;
  http.begin(apiUrl);
  int httpCode = http.GET();

  if (httpCode > 0) { 
    String payload = http.getString();
    
    // (Using DynamicJsonDocument compatible with ArduinoJson v6)
    DynamicJsonDocument doc(2048); 
    DeserializationError error = deserializeJson(doc, payload);

    if (error) {
      Serial.print("[!] JSON parsing failed: ");
      Serial.println(error.c_str());
    } else {
      const char* fajrTimeStr = doc["data"]["timings"]["Fajr"]; 
      sscanf(fajrTimeStr, "%d:%d", &fajrHour, &fajrMinute);

      Serial.println("[✔] Prayer times fetched successfully.");
      Serial.print("    Fajr time: ");
      Serial.printf("%02d:%02d\n", fajrHour, fajrMinute);
    }
  } else {
    Serial.print("[!] API connection failed, error code: ");
    Serial.println(httpCode);
  }
  http.end();
}

// (Lamp control function)
void checkTimeAndControlLight() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    Serial.println("[!] Failed to get local time.");
    return;
  }
  int currentHour = timeinfo.tm_hour;
  int currentMinute = timeinfo.tm_min;

  if (fajrHour == -1) {
    Serial.println("[!] Prayer times not yet available, waiting...");
    return;
  }

  // 1. Lamp ON logic
  if (!isLightOn && (currentHour == fajrHour) && (currentMinute == fajrMinute)) {
    Serial.println("===================================");
    Serial.println("   It’s Fajr time! Turning on the lamp.");
    Serial.println("===================================");
    
    digitalWrite(RELAY_PIN, HIGH); 
    isLightOn = true;
    lightOnTime = millis();
  }

  // 2. Lamp OFF logic
  if (isLightOn) {
    unsigned long timeElapsed = millis() - lightOnTime;
    unsigned long durationMs = LIGHT_DURATION_MINUTES * 60 * 1000UL;

    if (timeElapsed >= durationMs) {
      Serial.println("===================================");
      Serial.println("   Duration ended. Turning off the lamp.");
      Serial.println("===================================");
      
      digitalWrite(RELAY_PIN, LOW);
      isLightOn = false;
    }
  }
}
