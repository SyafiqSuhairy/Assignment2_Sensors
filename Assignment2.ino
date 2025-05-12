#include "display_manager.h"
#include "eeprom_storage.h"
#include "firebase_client.h"
#include "web_interface.h"
#include "wifi_connection.h"

void setup() {
  Serial.begin(115200);

  initDisplay();
  showBootScreen();
  loadFromEEPROM();
  pinMode(0, INPUT_PULLUP);

  if (digitalRead(0) == LOW) {
    beginWebInterface();
  } else {
    if (connectToWiFi()) {
      initializeFirebase();
      startFirebaseListening();
    } else {
      beginWebInterface(); 
    }
  }
}

void loop() {
  if (isConfigPortalActive()) {  
    manageWebRequests(); 
  }
  checkFirebaseToken();
}

