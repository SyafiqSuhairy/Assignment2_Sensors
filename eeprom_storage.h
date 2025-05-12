// EEPROM Handler – Saving & Loading WiFi + Device ID
#ifndef EEPROM_STORAGE_H
#define EEPROM_STORAGE_H

#include <EEPROM.h>

// Global variables accessible across files
extern String ssid;
extern String pass;
extern String devid;

// Initialize the global variables
String ssid = "";
String pass = "";
String devid = "";

// Function declarations
void writeCredentialsToEEPROM(String ssid, String pass, String deviceID);
void clearEEPROM();
void loadFromEEPROM();

void loadFromEEPROM() {
  EEPROM.begin(512);
  Serial.println("Loading stored data...");

  ssid = "";
  pass = "";
  devid = "";

  for (int i = 0; i < 20; i++) {
    char c = char(EEPROM.read(i));
    if (c != 0) ssid += c;
  }
  for (int i = 20; i < 40; i++) {
    char c = char(EEPROM.read(i));
    if (c != 0) pass += c;
  }
  for (int i = 40; i < 60; i++) {
    char c = char(EEPROM.read(i));
    if (c != 0) devid += c;
  }

  EEPROM.end();

  Serial.println("WiFi SSID: " + ssid);
  Serial.println("WiFi Password: " + pass);
  Serial.println("Device ID: " + devid);
}

void writeCredentialsToEEPROM(String newSSID, String newPASS, String newID) {
  clearEEPROM();
  EEPROM.begin(512);
  Serial.println("Saving credentials...");

  for (int i = 0; i < newSSID.length(); i++) EEPROM.write(i, newSSID[i]);
  EEPROM.write(newSSID.length(), 0);
  for (int i = 0; i < newPASS.length(); i++) EEPROM.write(20 + i, newPASS[i]);
  EEPROM.write(20 + newPASS.length(), 0);
  for (int i = 0; i < newID.length(); i++) EEPROM.write(40 + i, newID[i]);
  EEPROM.write(40 + newID.length(), 0);

  EEPROM.commit();
  EEPROM.end();

  // Update global variables
  ssid = newSSID;
  pass = newPASS;
  devid = newID;

  Serial.println("Saved to EEPROM");
}

void clearEEPROM() {
  EEPROM.begin(512);
  for (int i = 0; i < 512; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  EEPROM.end();

  // Clear global variables
  ssid = "";
  pass = "";
  devid = "";

  Serial.println("EEPROM cleared");
}

#endif
