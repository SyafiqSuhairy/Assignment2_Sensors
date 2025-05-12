// EEPROM Handler – Saving & Loading WiFi + Device ID
#ifndef EEPROM_STORAGE_H
#define EEPROM_STORAGE_H

#include <EEPROM.h>

String savedSSID = "";
String savedPassword = "";
String savedDeviceID = "";

void wipeEEPROM();  // Pre-declare

void loadFromEEPROM() {
  EEPROM.begin(512);
  Serial.println("Loading stored data...");

  savedSSID = "";
  savedPassword = "";
  savedDeviceID = "";

  for (int i = 0; i < 20; i++) {
    char c = char(EEPROM.read(i));
    if (c != 0) savedSSID += c;
  }
  for (int i = 20; i < 40; i++) {
    char c = char(EEPROM.read(i));
    if (c != 0) savedPassword += c;
  }
  for (int i = 40; i < 60; i++) {
    char c = char(EEPROM.read(i));
    if (c != 0) savedDeviceID += c;
  }

  EEPROM.end();

  Serial.println("WiFi SSID: " + savedSSID);
  Serial.println("WiFi Password: " + savedPassword);
  Serial.println("Device ID: " + savedDeviceID);
}

void saveToEEPROM(String ssid, String pass, String deviceID) {
  wipeEEPROM();
  EEPROM.begin(512);
  Serial.println("Saving credentials...");

  for (int i = 0; i < ssid.length(); i++) EEPROM.write(i, ssid[i]);
  EEPROM.write(ssid.length(), 0);
  for (int i = 0; i < pass.length(); i++) EEPROM.write(20 + i, pass[i]);
  EEPROM.write(20 + pass.length(), 0);
  for (int i = 0; i < deviceID.length(); i++) EEPROM.write(40 + i, deviceID[i]);
  EEPROM.write(40 + deviceID.length(), 0);

  EEPROM.commit();
  EEPROM.end();

  Serial.println("Saved to EEPROM");
}

void wipeEEPROM() {
  EEPROM.begin(512);
  for (int i = 0; i < 512; i++) EEPROM.write(i, 0);
  EEPROM.commit();
  EEPROM.end();

  Serial.println("EEPROM wiped clean");
}

#endif
