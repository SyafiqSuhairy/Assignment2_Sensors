#ifndef WEB_INTERFACE_H
#define WEB_INTERFACE_H

#include <WebServer.h>
#include "eeprom_storage.h"

WebServer configServer(80);

// Forward declarations
void setupConfigHandlers();
void serveWiFiConfigPage();
void applyNewSettings();
void clearStoredSettings();

void beginWebInterface() {
  setupConfigHandlers();
  configServer.begin();
  Serial.println("Access Point Web Interface started");
}

void manageWebRequests() {
  configServer.handleClient();
}

void setupConfigHandlers() {
  configServer.on("/", serveWiFiConfigPage);

  configServer.on("/apply", applyNewSettings);

  configServer.on("/reset", clearStoredSettings);
}

void serveWiFiConfigPage() {
  String page = "<!DOCTYPE html><html><head><title>WiFi Setup</title>";
  page += "<style>body{font-family:sans-serif;}input{width:100%;padding:8px;margin:5px 0;}label{font-weight:bold;}button{padding:8px;background:#007BFF;color:#fff;border:none;cursor:pointer;}</style>";
  page += "</head><body><h2>ESP32 WiFi Configuration</h2><hr>";
  page += "<p><strong>Saved WiFi:</strong><br>SSID: " + ssid + "<br>Password: " + pass + "<br>Device ID: " + devid + "</p><hr>";
  page += "<form action='/apply' method='get'>";
  page += "<label>New SSID:</label><input type='text' name='ssid'><br>";
  page += "<label>New Password:</label><input type='password' name='password'><br>";
  page += "<label>Device ID:</label><input type='text' name='devid'><br>";
  page += "<button type='submit'>Save Settings</button></form><br>";
  page += "<form action='/reset' method='get'><button type='submit' style='background:red;'>Clear EEPROM</button></form>";
  page += "</body></html>";

  configServer.send(200, "text/html", page);
}

void applyNewSettings() {
  String newSSID = configServer.arg("ssid");
  String newPASS = configServer.arg("password");
  String newID = configServer.arg("devid");

  writeCredentialsToEEPROM(newSSID, newPASS, newID);

  configServer.send(200, "text/html", "✅ Credentials saved. Rebooting...");
  delay(2000);
  ESP.restart();
}

void clearStoredSettings() {
  clearEEPROM();
  configServer.send(200, "text/html", "🧹 EEPROM cleared. Restarting...");
  delay(2000);
  ESP.restart();
}

#endif
