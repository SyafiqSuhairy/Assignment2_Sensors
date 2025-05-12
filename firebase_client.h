#ifndef FIREBASE_CLIENT_H
#define FIREBASE_CLIENT_H

#include <Firebase_ESP_Client.h>
#include "addons/TokenHelper.h"
#include "addons/RTDBHelper.h"

#include "display_manager.h"
#include "eeprom_storage.h"

#define FIREBASE_PROJECT_URL "https://esp-project-25857-default-rtdb.asia-southeast1.firebasedatabase.app/"
#define FIREBASE_API_KEY "AIzaSyDujFmyj4gC_qSMuYzNalpDuZEa_61adqM"
#define FIREBASE_USER_EMAIL "test1@example.com"
#define FIREBASE_USER_PASS "123456"

FirebaseData dataStream;
FirebaseData metaData;
FirebaseAuth auth;
FirebaseConfig config;

bool firebaseConnected = false;

void initializeFirebase() {
  config.api_key = FIREBASE_API_KEY;
  config.database_url = FIREBASE_PROJECT_URL;

  auth.user.email = FIREBASE_USER_EMAIL;
  auth.user.password = FIREBASE_USER_PASS;

  config.token_status_callback = tokenStatusCallback;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);

  configTime(28800, 0, "pool.ntp.org", "time.nist.gov");
  while (time(nullptr) < 8 * 3600 * 2) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nTime is synced.");

  while (auth.token.uid == "") {
    Serial.print(".");
    delay(300);
  }

  firebaseConnected = true;
  Serial.println("Firebase connection established.");
}

void firebaseDataReceived(FirebaseStream stream) {
  if (stream.dataType() == "string") {
    String message = stream.stringData();
    String lastTime = "-";

    if (Firebase.RTDB.getString(&metaData, "/texts/last_updated")) {
      lastTime = metaData.stringData();
    }

    updateDisplay("Device: " + devid, message, "Time: " + lastTime);
  }
}

void firebaseTimeoutHandler(bool timeout) {
  if (timeout) {
    Serial.println("[Firebase] Stream timeout occurred.");
  }
}

void startFirebaseListening() {
  if (!Firebase.RTDB.beginStream(&dataStream, "/texts/sample_text")) {
    Serial.println("Failed to start Firebase stream.");
    Serial.println(dataStream.errorReason());
    return;
  }

  Firebase.RTDB.setStreamCallback(&dataStream, firebaseDataReceived, firebaseTimeoutHandler);
}

void checkFirebaseToken() {
  if (!Firebase.ready() || Firebase.isTokenExpired()) {
    Serial.println("Renewing Firebase token...");
    Firebase.begin(&config, &auth);
  }
}

#endif
