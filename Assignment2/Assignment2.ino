// SyafiqNet: WiFi Configuration Manager for ESP32
// Created June 2024
// 
// System Overview:
// 1. Reads network configuration from persistent storage (EEPROM)
// 2. Attempts to connect using stored credentials
// 3. If connection fails or boot button pressed, creates configuration portal
// 4. Portal allows user to select network and provide credentials
// 5. Configuration is saved and device restarts with new settings
//
// Special Features:
// - Live network scanner shows available networks and signal strength
// - Clean responsive UI with mobile device support
// - LED status indicator shows operating mode
// - One-click network reset option
// - Secure password masking in UI and logs
// ------------------------------------------------------

#include <EEPROM.h>
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPClient.h>
#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#define PORTAL_NAME "SyafiqNet"    // Portal network name
#define PORTAL_PASS ""             // Portal password (empty = open network)
#define STATUS_LED 2               // Status LED pin
#define CONFIG_BTN 0               // Config button pin
#define MEMORY_SIZE 512            // EEPROM allocation size

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 32
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// Firebase config
#define API_KEY "AIzaSyDujFmyj4gC_qSMuYzNalpDuZEa_61adqM"
#define DATABASE_URL "https://esp-project-25857-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define USER_EMAIL "test1@example.com"
#define USER_PASSWORD "123456"

WebServer webPortal(80);           // Web server on standard HTTP port

// Configuration storage variables
String networkSSID = "";
String networkPass = "";
String deviceName = "";
String webContent = "";
bool portalMode = false;           // Operating in portal/AP mode?
bool scanFinished = false;         // Network scan completed?
String networkList = "<p>Scanning available networks...</p>";
String message = "";
int16_t xPos = SCREEN_WIDTH;

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

// Display message on OLED screen
void showOLEDMessage(String msg) {
  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 10);
  display.print(msg);
  display.display();
}

// Initialization function
void setup() {
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  
  Serial.begin(115200);
  Serial.println("\n\n=== SyafiqNet WiFi Manager Starting ===");
  
  // Initialize OLED display
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println("OLED initialization failed");
    while (1);
  }
  
  showOLEDMessage("Starting...");
  delay(1000);
  
  // Load saved configuration
  loadConfig();  

  // Enter config mode if no network configured
  if (networkSSID.length() == 0) {
    Serial.println("No network configured, starting setup portal");
    showOLEDMessage("No WiFi. AP Mode");
    startPortal();
    return;
  }

  delay(3000);  // Button press window
  pinMode(CONFIG_BTN, INPUT_PULLUP);

  // Force config mode if button pressed
  if (digitalRead(CONFIG_BTN) == 0) {
    Serial.println("Setup button pressed, starting portal");
    showOLEDMessage("Button pressed\nAP Mode");
    portalMode = true;
    startPortal();
  } else {
    // Try connecting to WiFi using stored credentials
    showOLEDMessage("Connecting WiFi...");
    Serial.println("Attempting to connect to: " + networkSSID);
    if (connectWiFi()) {
      Serial.println("Successfully connected to network!");
      Serial.println("Device IP: " + WiFi.localIP().toString());
      showOLEDMessage("WiFi Connected\nIP: " + WiFi.localIP().toString());
      delay(3000); // Show IP for 3 seconds
      initializeFirebase();
      showOLEDMessage("Firebase Ready!");
    } else {
      Serial.println("Connection failed. Starting setup portal.");
      showOLEDMessage("WiFi Failed\nAP Mode");
      startPortal();
      portalMode = true;
    }
  }
}

// Initialize Firebase connection
void initializeFirebase() {
  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  auth.user.email = USER_EMAIL;
  auth.user.password = USER_PASSWORD;

  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  Serial.println("Authenticating with Firebase...");
  while (auth.token.uid == "") {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nFirebase Ready!");
  
  // Setup Firebase listeners for device control
  setupFirebaseListeners();
}

// Set up Firebase listeners for device control
void setupFirebaseListeners() {
  if (Firebase.ready()) {
    // Set initial LED state
    if (Firebase.RTDB.getBool(&fbdo, "/device/led")) {
      bool ledState = fbdo.boolData();
      digitalWrite(STATUS_LED, ledState ? HIGH : LOW);
      Serial.println("Initial LED state set to: " + String(ledState ? "ON" : "OFF"));
    }
    
    // Create a display text node if it doesn't exist
    if (!Firebase.RTDB.getString(&fbdo, "/display/text")) {
      Firebase.RTDB.setString(&fbdo, "/display/text", "Welcome to SyafiqNet!");
      Serial.println("Created initial display text in Firebase");
    }
  }
}

// Check for device control commands from Firebase
void checkDeviceCommands() {
  if (!Firebase.ready()) return;
  
  // Check LED state
  if (Firebase.RTDB.getBool(&fbdo, "/device/led")) {
    bool ledState = fbdo.boolData();
    digitalWrite(STATUS_LED, ledState ? HIGH : LOW);
    Serial.println("LED state changed to: " + String(ledState ? "ON" : "OFF"));
  }
  
  // Check for restart command
  if (Firebase.RTDB.getBool(&fbdo, "/device/restart")) {
    if (fbdo.boolData()) {
      Serial.println("Restart command received from Firebase");
      // Clear the restart flag
      Firebase.RTDB.setBool(&fbdo, "/device/restart", false);
      showOLEDMessage("Restarting...");
      delay(2000);
      ESP.restart();
    }
  }
  
  // Check for reset command
  if (Firebase.RTDB.getBool(&fbdo, "/device/reset")) {
    if (fbdo.boolData()) {
      Serial.println("Reset command received from Firebase");
      // Clear the reset flag
      Firebase.RTDB.setBool(&fbdo, "/device/reset", false);
      showOLEDMessage("Resetting config...");
      delay(2000);
      clearConfig();
      ESP.restart();
    }
  }
}

// Read data from Firebase
String readFirebaseData(String path) {
  if (Firebase.ready()) {
    if (Firebase.RTDB.getString(&fbdo, path)) {
      return fbdo.stringData();
    } else {
      Serial.println("Failed to read from Firebase: " + fbdo.errorReason());
      return "Error";
    }
  }
  return "Firebase not ready";
}

// Main program loop
void loop() {
  if (portalMode) {
    webPortal.handleClient();

    // Handle WiFi scanning
    if (!scanFinished) {
      int networks = WiFi.scanComplete();
      if (networks == -2) {
        Serial.println("Initiating network scan...");
        WiFi.scanNetworks(true);
      } else if (networks >= 0) {
        Serial.println("Scan complete, found " + String(networks) + " networks");
        networkList = "<table class='networks'>";
        networkList += "<tr><th>Network Name</th><th>Signal</th><th>Security</th></tr>";
        for (int i = 0; i < networks; ++i) {
          String securityType = "";
          switch(WiFi.encryptionType(i)) {
            case WIFI_AUTH_OPEN: securityType = "Open"; break;
            case WIFI_AUTH_WEP: securityType = "WEP"; break;
            case WIFI_AUTH_WPA_PSK: securityType = "WPA"; break;
            case WIFI_AUTH_WPA2_PSK: securityType = "WPA2"; break;
            case WIFI_AUTH_WPA_WPA2_PSK: securityType = "WPA/WPA2"; break;
            default: securityType = "Unknown";
          }
          networkList += "<tr><td>" + WiFi.SSID(i) + "</td><td>" + 
                           String(WiFi.RSSI(i)) + " dBm</td><td>" + 
                           securityType + "</td></tr>";
        }
        networkList += "</table>";
        scanFinished = true;
        WiFi.scanDelete();
      }
    }
  } else {
    static unsigned long lastFirebaseRead = 0;
    static unsigned long lastScrollTime = 0;
    static unsigned long lastStatusUpdate = 0;
    static unsigned long lastCommandCheck = 0;
    static int16_t messageWidth = 0;
    static int wifiReconnectAttempts = 0;
    static const int MAX_RECONNECT_ATTEMPTS = 5;  // Maximum WiFi reconnection attempts before switching to AP mode

    // Check WiFi connection and attempt to reconnect if disconnected
    if (WiFi.status() != WL_CONNECTED) {
      showOLEDMessage("WiFi Disconnected!\nReconnecting...");
      Serial.println("WiFi disconnected. Attempting to reconnect...");
      
      if (WiFi.reconnect()) {
        Serial.println("Reconnect initiated");
      } else {
        Serial.println("Failed to initiate reconnect");
      }
      
      wifiReconnectAttempts++;
      Serial.println("Reconnect attempt: " + String(wifiReconnectAttempts));
      
      // If multiple reconnect attempts fail, switch to AP mode
      if (wifiReconnectAttempts >= MAX_RECONNECT_ATTEMPTS) {
        Serial.println("Max reconnect attempts reached. Switching to AP mode.");
        showOLEDMessage("WiFi Failed\nSwitching to AP Mode");
        delay(2000);
        startPortal();
        portalMode = true;
        wifiReconnectAttempts = 0;
        return;
      }
      
      delay(1000);  // Brief delay before continuing
    } else {
      wifiReconnectAttempts = 0;  // Reset the counter if connected
    }

    // Check for device commands from Firebase every 2 seconds
    if (millis() - lastCommandCheck >= 2000) {
      lastCommandCheck = millis();
      checkDeviceCommands();
    }

    // Read message from Firebase every 5 seconds
    if (millis() - lastFirebaseRead >= 5000) {
      lastFirebaseRead = millis();
      
      if (Firebase.ready()) {
        Serial.println("Reading Firebase data from path: /display/text");
        String displayText = readFirebaseData("/display/text");
        Serial.println("Text from Firebase: " + displayText);

        if (displayText != "Error" && displayText.length() > 0) {
          message = displayText;
          xPos = SCREEN_WIDTH;
          messageWidth = message.length() * 6; // Adjust based on font size
        }
      } else {
        Serial.println("Firebase not ready");
      }
    }

    // Update scrolling text on OLED display
    if (millis() - lastScrollTime >= 50) { // Slightly slower scroll for better readability
      lastScrollTime = millis();
      display.clearDisplay();
      
      // Display connection status at top
      display.setTextSize(1);
      display.setTextColor(SSD1306_WHITE);
      display.setCursor(0, 0);
      display.print("Status: ");
      display.print(WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected");
      
      // Display device info
      display.setCursor(0, 10);
      display.print("ID: ");
      display.print(deviceName);
      
      // Scrolling Firebase message
      display.setCursor(xPos, 22);
      display.print(message);
      
      display.display();
      
      // Scroll logic
      xPos--;
      if (xPos < -messageWidth) {
        xPos = SCREEN_WIDTH;
      }
    }
  }
}

// Load configuration from EEPROM
void loadConfig() {
  EEPROM.begin(MEMORY_SIZE);
  Serial.println("Loading saved configuration...");
  networkSSID = networkPass = deviceName = "";

  // Read network name
  for (int i = 0; i < 20; i++) {
    char ch = EEPROM.read(i);
    if (isPrintable(ch)) networkSSID += ch;
  }
  // Read network password
  for (int i = 20; i < 40; i++) {
    char ch = EEPROM.read(i);
    if (isPrintable(ch)) networkPass += ch;
  }
  // Read device name
  for (int i = 40; i < 60; i++) {
    char ch = EEPROM.read(i);
    if (isPrintable(ch)) deviceName += ch;
  }

  // Log configuration (hide password)
  Serial.println("Configured Network: " + networkSSID);
  Serial.print("Password: ");
  Serial.println(networkPass.length() > 0 ? "********" : "[not set]");
  Serial.println("Device Name: " + deviceName);

  EEPROM.end();
}

// Start configuration portal
void startPortal() {
  digitalWrite(STATUS_LED, HIGH);  // LED on in portal mode
  Serial.println("Starting configuration portal");
  portalMode = true;
  
  WiFi.mode(WIFI_AP_STA);  
  WiFi.softAP(PORTAL_NAME, PORTAL_PASS);
  
  Serial.println("Portal active. SSID: " + String(PORTAL_NAME));
  Serial.println("Portal Address: " + WiFi.softAPIP().toString());
  Serial.println("Connect to network '" + String(PORTAL_NAME) + "' then visit http://" + WiFi.softAPIP().toString());
  
  showOLEDMessage("AP Mode\n" + WiFi.softAPIP().toString());
  WiFi.scanNetworks(true);
  setupWebServer();
}

// Initialize the web server
void setupWebServer() {
  createWebRoutes();
  webPortal.begin();
  Serial.println("Configuration web portal ready");
}

// Define web server routes and UI
void createWebRoutes() {
  // Main configuration page
  webPortal.on("/", []() {
    String passwordDisplay = networkPass.length() > 0 ? "********" : "[not set]";
    webContent = R"rawliteral(
      <!DOCTYPE html>
      <html>
      <head>
        <meta name="viewport" content="width=device-width, initial-scale=1">
        <title>SyafiqNet WiFi Manager</title>
        <style>
          body { 
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; 
            text-align: center; 
            padding: 20px; 
            background: #f0f2f5; 
            color: #333;
          }
          h1 { 
            color: #2c3e50; 
            border-bottom: 2px solid #3498db;
            padding-bottom: 10px;
            max-width: 600px;
            margin: 0 auto 20px;
          }
          form { 
            background: white; 
            padding: 25px; 
            border-radius: 12px; 
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            display: inline-block; 
            max-width: 400px;
            margin: 20px auto;
          }
          .info-panel {
            background: white;
            padding: 15px;
            border-radius: 12px;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            max-width: 400px;
            margin: 20px auto;
            text-align: left;
          }
          input, label { 
            display: block; 
            margin: 10px auto; 
            width: 80%; 
          }
          input[type="text"], input[type="password"] {
            padding: 12px; 
            border: 1px solid #ddd; 
            border-radius: 6px;
            font-size: 16px;
          }
          .connect-btn {
            background-color: #3498db; 
            color: white; 
            padding: 12px 24px; 
            border: none;
            border-radius: 6px; 
            cursor: pointer; 
            font-size: 16px;
            font-weight: bold;
            transition: background 0.3s;
          }
          .connect-btn:hover {
            background-color: #2980b9;
          }
          .reset-btn {
            background-color: #e74c3c; 
            color: white; 
            padding: 8px 16px; 
            border: none;
            border-radius: 6px; 
            cursor: pointer; 
            font-size: 14px;
            margin-top: 20px;
            transition: background 0.3s;
          }
          .reset-btn:hover {
            background-color: #c0392b;
          }
          .networks { 
            margin: 20px auto; 
            width: 100%; 
            border-collapse: collapse;
            background: white;
            box-shadow: 0 2px 10px rgba(0,0,0,0.1);
            border-radius: 8px;
            overflow: hidden;
          }
          .networks th, .networks td { 
            padding: 12px; 
            text-align: left; 
            border-bottom: 1px solid #ddd;
          }
          .networks th {
            background-color: #3498db;
            color: white;
          }
          .networks tr:hover {
            background-color: #f5f5f5;
          }
        </style>
      </head>
      <body>
        <h1>SyafiqNet WiFi Manager</h1>
        
        <div class="info-panel">
          <h3>Current Configuration</h3>
          <p><strong>Network:</strong> )rawliteral" + networkSSID + R"rawliteral(</p>
          <p><strong>Password:</strong> )rawliteral" + passwordDisplay + R"rawliteral(</p>
          <p><strong>Device Name:</strong> )rawliteral" + deviceName + R"rawliteral(</p>
        </div>
        
        <form method='get' action='saveconfig'>
          <h3>Update Configuration</h3>
          <label>WiFi Network:</label>
          <input type='text' name='ssid' placeholder="Enter network name" required>
          <label>Password:</label>
          <input type='password' name='password' placeholder="Enter network password">
          <label>Device Name:</label>
          <input type='text' name='deviceid' placeholder="Enter a name for this device" required>
          <input class='connect-btn' type='submit' value='Connect &amp; Save'>
        </form>
        
        <p><a href="/reset"><button class="reset-btn">Reset All Settings</button></a></p>
        
        <h3>Available Networks</h3>
    )rawliteral" + networkList + R"rawliteral(
      </body>
      </html>
    )rawliteral";

    webPortal.send(200, "text/html", webContent);
  });

  // Save configuration
  webPortal.on("/saveconfig", []() {
    String newSSID = webPortal.arg("ssid");
    String newPass = webPortal.arg("password");
    String newName = webPortal.arg("deviceid");
    
    // Validate input
    if (newSSID.length() == 0 || newName.length() == 0) {
      webPortal.send(400, "text/html", "<h2>Error: Network name and device name are required</h2><p><a href='/'>Go back</a></p>");
      return;
    }
    
    saveConfig(newSSID, newPass, newName);
    webPortal.send(200, "text/html", "<h2>Configuration Saved</h2><p>Your device will now restart and attempt to connect to the network.</p>");
    delay(2000);
    digitalWrite(STATUS_LED, LOW);
    ESP.restart();
  });

  // Reset configuration
  webPortal.on("/reset", []() {
    clearConfig();
    webPortal.send(200, "text/html", "<h2>All Settings Reset</h2><p>The device will restart in configuration mode.</p>");
    delay(2000);
    ESP.restart();
  });
}

// Save configuration to EEPROM
void saveConfig(String ssid, String password, String name) {
  clearConfig();
  EEPROM.begin(MEMORY_SIZE);
  Serial.println("Saving new configuration...");
  
  // Save network name (max 20 chars)
  for (int i = 0; i < 20; i++) EEPROM.write(i, (i < ssid.length()) ? ssid[i] : 0);
  
  // Save password (max 20 chars)
  for (int i = 0; i < 20; i++) EEPROM.write(20 + i, (i < password.length()) ? password[i] : 0);
  
  // Save device name (max 20 chars)
  for (int i = 0; i < 20; i++) EEPROM.write(40 + i, (i < name.length()) ? name[i] : 0);
  
  EEPROM.commit();
  EEPROM.end();
  
  Serial.println("Configuration saved successfully:");
  Serial.println("Network: " + ssid);
  Serial.print("Password: ");
  Serial.println(password.length() > 0 ? "********" : "[not set]");
  Serial.println("Device Name: " + name);
}

// Clear configuration from EEPROM
void clearConfig() {
  EEPROM.begin(MEMORY_SIZE);
  Serial.println("Clearing all saved configuration...");
  for (int i = 0; i < 60; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  EEPROM.end();
  Serial.println("Configuration reset complete");
}

// Connect to WiFi using saved credentials
boolean connectWiFi() {
  WiFi.softAPdisconnect();
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(networkSSID.c_str(), networkPass.c_str());
  
  int attempts = 0;
  Serial.print("Connecting");
  while (attempts < 25) {  // 25 second timeout
    if (WiFi.status() == WL_CONNECTED) {
      WiFi.setAutoReconnect(true);
      WiFi.persistent(true);
      return true;
    }
    Serial.print(".");
    delay(1000);
    attempts++;
  }
  Serial.println("\nConnection attempt timed out");
  return false;
}