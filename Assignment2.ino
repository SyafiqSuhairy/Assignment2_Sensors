#include "display_manager.h"
#include "eeprom_storage.h"
#include "firebase_client.h"
#include "web_interface.h"
#include "wifi_connection.h"

bool firebaseEnabled = false;  // Flag to track if Firebase should be used

void handleSerialCommands() {
  if (Serial.available()) {
    String command = Serial.readStringUntil('\n');
    command.trim();
    
    if (command == "clear" || command == "CLEAR") {
      Serial.println("Clearing stored credentials...");
      clearEEPROM();
      Serial.println("Credentials cleared! Please restart the device.");
      delay(1000);
      ESP.restart();
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println("\n\nESP32 WiFi Configuration");
  Serial.println("Type 'clear' to erase stored credentials");

  initDisplay();
  showBootScreen();
  loadFromEEPROM();
  pinMode(0, INPUT_PULLUP);

  // Check if credentials are empty
  if (ssid.length() == 0 || pass.length() == 0) {
    Serial.println("No credentials found - Starting configuration portal");
    beginWebInterface();
    return;  // Skip the rest of setup
  }

  if (digitalRead(0) == LOW) {
    Serial.println("Button pressed - Starting configuration portal");
    beginWebInterface();
  } else {
    if (connectToWiFi()) {
      initializeFirebase();
      startFirebaseListening();
      firebaseEnabled = true;  // Set flag to use Firebase
    } else {
      Serial.println("WiFi connection failed - Starting configuration portal");
      beginWebInterface(); 
    }
  }
}

void loop() {
  handleSerialCommands();  // Check for serial commands
  
  if (isConfigPortalActive()) {  
    manageWebRequests(); 
  } else if (firebaseEnabled) {
    // Only check Firebase if we're successfully connected
    checkFirebaseToken();
  }
}

