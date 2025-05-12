#ifndef WIFI_CONNECTION_H
#define WIFI_CONNECTION_H

#include <WiFi.h>

extern String ssid, pass;
void beginWebInterface();  // Web interface forward declaration

bool fallbackAP = false;  // Status flag

bool tryWiFiConnection() {
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid.c_str(), pass.c_str());

  Serial.println("Attempting WiFi connection...");
  for (int attempts = 0; attempts < 50; attempts++) {
    if (WiFi.status() == WL_CONNECTED) {
      Serial.println("✅ Connected to WiFi!");
      Serial.print("Local IP: ");
      Serial.println(WiFi.localIP());
      WiFi.setAutoReconnect(true);
      WiFi.persistent(true);
      return true;
    }
    Serial.print(".");
    delay(400);
  }
  Serial.println("\n❌ Connection failed.");
  return false;
}

void activateAccessPoint() {
  const char* apSSID = "ESP32_Config_Portal";
  const char* apPASS = "";  // Open AP
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSSID, apPASS);
  Serial.println("\n📡 AP Mode started.");
  Serial.print("Connect to WiFi: ");
  Serial.println(apSSID);
  Serial.print("Access Web at: ");
  Serial.println(WiFi.softAPIP());

  fallbackAP = true;
  beginWebInterface();
}

bool apModeStatus() {
  return fallbackAP;
}

#endif
