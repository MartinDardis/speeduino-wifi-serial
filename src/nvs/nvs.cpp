// nvs.cpp — Non-volatile storage persistence
//
// Purpose: implement saveConfig() and loadConfig() using the ESP32 Arduino
//   Preferences library (a thin wrapper over the ESP-IDF NVS partition).
//
// NVS namespace: "LK8000Bridge" (16-char max, inherited from original project).
//
// Key naming conventions:
//   - Per-port UART settings: "COM0Baudrate", "COM1Data", "COM2Parity", etc.
//   - TCP ports: "TCPPort0", "TCPPort1", "TCPPort2".
//   - Cross-routing matrix: "COM0xCOM1", "COM0xCOM2", … (9 keys for 3×3 matrix).
//   - NMEA filters: "NMEAPass0002" = port 0, filter slot 2 (printf "%02u%02u").
//     The zero-padded format avoids key collisions between single- and double-digit indices.
#include "../globals.h"
#include "nvs.h"

void saveConfig() {
  char key[32];
  preferences.begin("LK8000Bridge", false); // false = read-write

  preferences.putBool("bLoginEnable",    bLoginEnable);
  preferences.putString("login_username", login_username);
  preferences.putString("login_password", login_password);

  preferences.putLong("COM0Baudrate", COM[0].Baudrate);
  preferences.putLong("COM1Baudrate", COM[1].Baudrate);
  preferences.putLong("COM2Baudrate", COM[2].Baudrate);

  preferences.putInt("TCPPort0", COM[0].Port);
  preferences.putInt("TCPPort1", COM[1].Port);
  preferences.putInt("TCPPort2", COM[2].Port);

  preferences.putLong("COM0Data", COM[0].Data);
  preferences.putLong("COM1Data", COM[1].Data);
  preferences.putLong("COM2Data", COM[2].Data);

  preferences.putLong("COM0Stop", COM[0].Stop);
  preferences.putLong("COM1Stop", COM[1].Stop);
  preferences.putLong("COM2Stop", COM[2].Stop);

  preferences.putChar("COM0Parity", COM[0].Parity);
  preferences.putChar("COM1Parity", COM[1].Parity);
  preferences.putChar("COM2Parity", COM[2].Parity);

  for (int i = 0; i < NUM_HW_COM; i++) {
    for (int j = 0; j < NUM_HW_COM; j++) {
      snprintf(key, sizeof(key), "COM%dxCOM%d", i, j);
      preferences.putBool(key, COM[i].xCOM[j]);
    }
  }

  for (int i = 0; i < NUM_HW_COM; i++) {
    for (int j = 0; j < MAX_NO_FILTER; j++) {
      snprintf(key, sizeof(key), "NMEAPass%02u%02u", i, j);
      preferences.putString(key, COM[i].NMEAPass[j]);
      snprintf(key, sizeof(key), "NMEABlock%02u%02u", i, j);
      preferences.putString(key, COM[i].NMEABlock[j]);
    }
  }

  preferences.putString("receiverName",    receiverName);
  preferences.putBool("openAP",            openAP);
  preferences.putString("AP_pw",           AP_pw);
  preferences.putString("WiFi_ssid",       WiFi_ssid);
  preferences.putString("WiFi_pw",         WiFi_pw);
  preferences.putInt("iWiFiSearchTime",    iWiFiSearchTime);
  preferences.putInt("iWiFiTxPower",       iWiFiTxPower);

  preferences.end();
}

void loadConfig() {
  char key[32];
  preferences.begin("LK8000Bridge", true); // true = read-only

  bLoginEnable   = preferences.getBool("bLoginEnable",    false);
  login_username = preferences.getString("login_username", "");
  login_password = preferences.getString("login_password", "");

  COM[0].Baudrate = preferences.getLong("COM0Baudrate", 115200);
  COM[1].Baudrate = preferences.getLong("COM1Baudrate",  19200);
  COM[2].Baudrate = preferences.getLong("COM2Baudrate",  19200);

  COM[0].Port = preferences.getInt("TCPPort0", 8880);
  COM[1].Port = preferences.getInt("TCPPort1", 8881);
  COM[2].Port = preferences.getInt("TCPPort2", 8882);

  COM[0].Data = preferences.getLong("COM0Data", 8);
  COM[1].Data = preferences.getLong("COM1Data", 8);
  COM[2].Data = preferences.getLong("COM2Data", 8);

  COM[0].Stop = preferences.getLong("COM0Stop", 1);
  COM[1].Stop = preferences.getLong("COM1Stop", 1);
  COM[2].Stop = preferences.getLong("COM2Stop", 1);

  COM[0].Parity = preferences.getChar("COM0Parity", 'N');
  COM[1].Parity = preferences.getChar("COM1Parity", 'N');
  COM[2].Parity = preferences.getChar("COM2Parity", 'N');

  for (int i = 0; i < NUM_HW_COM; i++) {
    for (int j = 0; j < NUM_HW_COM; j++) {
      snprintf(key, sizeof(key), "COM%dxCOM%d", i, j);
      COM[i].xCOM[j] = preferences.getBool(key, false);
    }
  }

  for (int i = 0; i < NUM_HW_COM; i++) {
    COM[i].NoNMEAPass  = 0;
    COM[i].NoNMEABlock = 0;
    for (int j = 0; j < MAX_NO_FILTER; j++) {
      snprintf(key, sizeof(key), "NMEAPass%02u%02u", i, j);
      COM[i].NMEAPass[j] = preferences.getString(key, "");
      if (COM[i].NMEAPass[j].length() > 0) COM[i].NoNMEAPass = j + 1;

      snprintf(key, sizeof(key), "NMEABlock%02u%02u", i, j);
      COM[i].NMEABlock[j] = preferences.getString(key, "");
      if (COM[i].NMEABlock[j].length() > 0) COM[i].NoNMEABlock = j + 1;
    }
  }

  receiverName    = preferences.getString("receiverName",    "SpeeduinoWiFi");
  openAP          = preferences.getBool("openAP",            true);
  AP_pw           = preferences.getString("AP_pw",           "12345678");
  WiFi_ssid       = preferences.getString("WiFi_ssid",       "");
  WiFi_pw         = preferences.getString("WiFi_pw",         "");
  iWiFiSearchTime = preferences.getInt("iWiFiSearchTime",    0);
  iWiFiTxPower    = preferences.getInt("iWiFiTxPower",       20);

  preferences.end();
}
