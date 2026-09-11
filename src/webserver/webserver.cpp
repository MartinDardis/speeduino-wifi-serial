// webserver.cpp — HTTP routes and WebSocket traffic broadcast
//
// Purpose: implement the web-based configuration UI and live traffic monitor.
//
// HTTP route summary:
//   GET  /               → serve SPIFFS static files (index.htm, restart.htm, etc.)
//   GET  /heap           → return free heap in bytes (diagnostics)
//   ANY  /set            → apply and persist settings (see handleSetting)
//   GET  /loadRootState  → return WiFi connection state and per-port busy flags
//   GET  /loadRootData   → return all current settings for the web UI to render
//   ANY  /loadConsoleText → placeholder; always returns "OK"
//   ANY  /restart        → serve restart page and reboot
//   WS   /ws/traffic     → stream live traffic events as JSON to the browser
//
// /loadRootData response format (one field per line):
//   key|value|type\n
//   type is one of: input (text field), chk (checkbox), div (read-only label),
//   button (triggers a client-side action).
//
// Why /set accepts both GET and POST:
//   The web UI originally used GET for simplicity. POST was added later for
//   fields that may exceed URL length limits (long SSID / password). Both
//   paths write to the same global variables so behavior is identical.
//
// Why WebSocket for traffic instead of polling:
//   Traffic can arrive continuously at up to hundreds of frames per second.
//   A polling endpoint would either miss frames or hammer the server. The
//   WebSocket lets the bridge task push each frame as it arrives with minimal
//   overhead.
#include "../globals.h"
#include "webserver.h"
#include "../uart/uart.h"
#include "../nvs/nvs.h"
#include <SPIFFS.h>
#include <ElegantOTA.h>

// Extract the No-th (0-indexed) whitespace-delimited token from Text.
String Extract(int No, String Text) {
  String res = "";
  Text.trim();
  int len = Text.length();
  int from = 0, to = 0;
  for (int i = 0; i < (No + 1); i++) {
    while ((Text.charAt(to) == ' ') && (to < len)) to++;
    from = to;
    while ((Text.charAt(to) != ' ') && (to < len)) to++;
  }
  res = Text.substring(from, to);
  res.trim();
  return res;
}

void handleSetting(AsyncWebServerRequest* request) {
  int params = request->params();
  for (int i = 0; i < params; i++) {
    const AsyncWebParameter* p = request->getParam(i);
    if (p->isFile()) {
      if (Debug_COM != NULL) Debug_COM->printf("_FILE[%s]: %s, size: %u\n",
        p->name().c_str(), p->value().c_str(), p->size());
    } else if (p->isPost()) {
      if (Debug_COM != NULL) Debug_COM->printf("_POST[%s]: %s\n",
        p->name().c_str(), p->value().c_str());

      if (p->name() == "WiFi_ssid") WiFi_ssid = p->value();
      if (p->name() == "WiFi_pw") { if (p->value() != "********") WiFi_pw = p->value(); }
      if (p->name() == "AP_pw") AP_pw = p->value();
      if (p->name() == "receiverName") {
        receiverName = p->value();
        if (Debug_COM != NULL) { Debug_COM->print("receiverName is now: "); Debug_COM->println(receiverName); }
      }
      if (p->name() == "iWiFiSearchTime") iWiFiSearchTime = atoi(p->value().c_str());
      if (p->name() == "iWiFiTxPower")    iWiFiTxPower    = atoi(p->value().c_str());
      if (p->name() == "TCPPort0") { COM[0].Port = atol(p->value().c_str()); if (Debug_COM != NULL) Debug_COM->println(COM[0].Port); InitSerial(0); }
      if (p->name() == "TCPPort1") { COM[1].Port = atol(p->value().c_str()); if (Debug_COM != NULL) Debug_COM->println(COM[1].Port); InitSerial(1); }
      if (p->name() == "TCPPort2") { COM[2].Port = atol(p->value().c_str()); if (Debug_COM != NULL) Debug_COM->println(COM[2].Port); InitSerial(2); }

      for (int n = 0; n < NUM_HW_COM; n++) {
        if (p->name() == ("NMEAPass" + String(n))) {
          if (Debug_COM != NULL) Debug_COM->println(p->value().c_str());
          COM[n].NoNMEAPass = 0;
          for (int j = 0; j < MAX_NO_FILTER; j++) {
            COM[n].NMEAPass[j] = Extract(j, p->value());
            if (COM[n].NMEAPass[j].length() > 0) {
              COM[n].NoNMEAPass = j + 1;
              COM[n].NMEAPass[j].trim();
              if (Debug_COM != NULL) {
                Debug_COM->print("\n Pass "); Debug_COM->print(n);
                Debug_COM->print(" Sentence:"); Debug_COM->print(j); Debug_COM->print(" : ");
                Debug_COM->println(COM[n].NMEAPass[j]);
              }
            }
          }
          if (Debug_COM != NULL) Debug_COM->println(COM[n].NoNMEAPass);
        }
        if (p->name() == ("NMEABlock" + String(n))) {
          if (Debug_COM != NULL) Debug_COM->println(p->value().c_str());
          COM[n].NoNMEABlock = 0;
          for (int j = 0; j < MAX_NO_FILTER; j++) {
            COM[n].NMEABlock[j] = Extract(j, p->value());
            if (COM[n].NMEABlock[j].length() > 0) {
              COM[n].NoNMEABlock = j + 1;
              COM[n].NMEABlock[j].trim();
              if (Debug_COM != NULL) {
                Debug_COM->print("\n Block "); Debug_COM->print(n);
                Debug_COM->print(" Sentence:"); Debug_COM->print(j); Debug_COM->print(" : ");
                Debug_COM->println(COM[n].NMEABlock[j]);
              }
            }
          }
          if (Debug_COM != NULL) Debug_COM->println(COM[n].NoNMEABlock);
        }
      }
    } else {
      if (Debug_COM != NULL) Debug_COM->printf("_GET[%s]: %s\n",
        p->name().c_str(), p->value().c_str());

      if (p->name() == "bCloseAP")        openAP          = (p->value() == "0");
      if (p->name() == "WiFi_ssid")       WiFi_ssid       = p->value();
      if (p->name() == "WiFi_pw")         WiFi_pw         = p->value();
      if (p->name() == "AP_pw")           AP_pw           = p->value();
      if (p->name() == "receiverName") {
        receiverName = p->value();
        if (Debug_COM != NULL) { Debug_COM->print("receiverName is now: "); Debug_COM->println(receiverName); }
      }
      if (p->name() == "iWiFiSearchTime") {
        iWiFiSearchTime = atoi(p->value().c_str());
        if (Debug_COM != NULL) { Debug_COM->print("iWiFiSearchTime is now: "); Debug_COM->println(iWiFiSearchTime); }
      }
      if (p->name() == "iWiFiTxPower") {
        iWiFiTxPower = atoi(p->value().c_str());
        if (Debug_COM != NULL) { Debug_COM->print("iWiFiTxPower is now: "); Debug_COM->println(iWiFiTxPower); }
      }
      if (p->name() == "COM0Baudrate") { COM[0].Baudrate = atoi(p->value().c_str()); InitSerial(0); }
      if (p->name() == "COM1Baudrate") { COM[1].Baudrate = atoi(p->value().c_str()); InitSerial(1); }
      if (p->name() == "COM2Baudrate") { COM[2].Baudrate = atoi(p->value().c_str()); InitSerial(2); }
      if (p->name() == "COM0Data")     { COM[0].Data = atoi(p->value().c_str()); InitSerial(0); }
      if (p->name() == "COM1Data")     { COM[1].Data = atoi(p->value().c_str()); InitSerial(1); }
      if (p->name() == "COM2Data")     { COM[2].Data = atoi(p->value().c_str()); if (Debug_COM != NULL) Debug_COM->println(COM[0].Data); InitSerial(2); }
      if (p->name() == "COM0Stop")     { COM[0].Stop = atoi(p->value().c_str()); InitSerial(0); }
      if (p->name() == "COM1Stop")     { COM[1].Stop = atoi(p->value().c_str()); InitSerial(1); }
      if (p->name() == "COM2Stop")     { COM[2].Stop = atoi(p->value().c_str()); InitSerial(2); }
      if (p->name() == "COM0Parity")   { COM[0].Parity = (char)p->value().charAt(0); InitSerial(0); }
      if (p->name() == "COM1Parity")   { COM[1].Parity = (char)p->value().charAt(0); InitSerial(1); }
      if (p->name() == "COM2Parity")   { COM[2].Parity = (char)p->value().charAt(0); InitSerial(2); }

      for (int n = 0; n < NUM_HW_COM; n++) {
        for (int x = 0; x < NUM_HW_COM; x++) {
          String key = "COM" + String(n) + "xCOM" + String(x);
          if (p->name() == key) COM[n].xCOM[x] = (p->value() != "0");
        }
      }
    }
  }
  if (Debug_COM != NULL) Debug_COM->println("save Config");
  saveConfig();
  String ret = "Saved "; ret += params; ret += " parameter.";
  request->send(HTTP_STATUS_OK, "text/plain", ret);
}

void handleRestart(AsyncWebServerRequest* request) {
  request->send(SPIFFS, "/restart.htm");
  delay(200);
  WiFi.disconnect();
  delay(200);
  ESP.restart();
}

static AsyncWebSocket ws("/ws/traffic");

static void onWsEvent(AsyncWebSocket* server, AsyncWebSocketClient* client,
                      AwsEventType type, void* arg, uint8_t* data, size_t len) {
  if (type == WS_EVT_CONNECT) client->ping(); // probe the client on connect
}

void broadcastTrafficEvent(const char* direction, const uint8_t* buf, uint16_t len) {
  if (ws.count() == 0 || len == 0) return;
  static char msg[900];
  int pos = snprintf(msg, sizeof(msg),
    "{\"d\":\"%s\",\"l\":%u,\"t\":%lu,\"h\":\"", direction, len, millis());
  for (uint16_t i = 0; i < len && pos < (int)sizeof(msg) - 5; i++)
    pos += snprintf(msg + pos, sizeof(msg) - pos, "%02X ", buf[i]);
  if (pos > 0 && msg[pos - 1] == ' ') pos--;
  snprintf(msg + pos, sizeof(msg) - pos, "\"}");
  ws.textAll(msg);
}

void wsCleanupClients() {
  ws.cleanupClients();
}

void setup_webserver(void*) {
  // Static files from SPIFFS (index.htm, restart.htm, traffic.htm).
  server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");

  server.on("/heap", HTTP_GET, [](AsyncWebServerRequest* request) {
    request->send(HTTP_STATUS_OK, "text/plain", String(ESP.getFreeHeap()));
  });

  server.on("/set", HTTP_ANY, handleSetting);

  server.onNotFound([](AsyncWebServerRequest* request) {
    Serial.printf("NOT_FOUND: ");
    if      (request->method() == HTTP_GET)     Serial.printf("GET");
    else if (request->method() == HTTP_POST)    Serial.printf("POST");
    else if (request->method() == HTTP_DELETE)  Serial.printf("DELETE");
    else if (request->method() == HTTP_PUT)     Serial.printf("PUT");
    else if (request->method() == HTTP_PATCH)   Serial.printf("PATCH");
    else if (request->method() == HTTP_HEAD)    Serial.printf("HEAD");
    else if (request->method() == HTTP_OPTIONS) Serial.printf("OPTIONS");
    else                                        Serial.printf("UNKNOWN");
    Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());
    if (request->contentLength()) {
      Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
      Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
    }
    int headers = request->headers();
    for (int i = 0; i < headers; i++) {
      const AsyncWebHeader* h = request->getHeader(i);
      Serial.printf("_HEADER[%s]: %s\n", h->name().c_str(), h->value().c_str());
    }
    int params = request->params();
    for (int i = 0; i < params; i++) {
      const AsyncWebParameter* p = request->getParam(i);
      if (p->isFile())      Serial.printf("_FILE[%s]: %s, size: %u\n", p->name().c_str(), p->value().c_str(), p->size());
      else if (p->isPost()) Serial.printf("_POST[%s]: %s\n", p->name().c_str(), p->value().c_str());
      else                  Serial.printf("_GET[%s]: %s\n",  p->name().c_str(), p->value().c_str());
    }
    request->send(404);
  });

  // Returns current WiFi state and per-port connection status.
  // Polled by the web UI on page load to update status badges.
  server.on("/loadRootState", HTTP_GET, [](AsyncWebServerRequest* request) {
    String values = "";
    values += "net_state|WiFi: "; values += WiFiClientState; values += "|div\n";
    for (int n = 0; n < NUM_HW_COM; n++) {
      bool busy = false;
      for (int i = 0; i < MAX_NMEA_CLIENTS && !busy; i++)
        busy = hwclient[n][i] && hwclient[n][i].connected();
      values += "port"; values += n; values += "_busy|";
      values += busy ? "1" : "0"; values += "|busy\n";
    }
    request->send(HTTP_STATUS_OK, "text/plain", values);
  });

  // Returns all configurable settings in "key|value|type\n" format.
  // The web UI iterates these lines and updates matching DOM elements by id.
  server.on("/loadRootData", HTTP_GET, [](AsyncWebServerRequest* request) {
    char key[32];
    String values = "";
    values += "receiverName|"; values += receiverName; values += "|input\n";
    values += "WiFi_ssid|";    values += WiFi_ssid;    values += "|input\n";
    values += "bCloseAP|";     values += openAP ? "" : "checked"; values += "|chk\n";

    for (int i = 0; i < NUM_HW_COM; i++) {
      for (int j = 0; j < NUM_HW_COM; j++) {
        snprintf(key, sizeof(key), "COM%dxCOM%d|", i, j);
        values += key;
        values += COM[i].xCOM[j] ? "checked" : "";
        values += "|chk\n";
      }
    }

    values += "net_state|";    values += WiFiClientState; values += "|div\n";
    values += "firmware_rev|Firmware: "; values += VERSION; values += "|div\n";
    values += "COM0Baudrate|"; values += COM[0].Baudrate; values += "|input\n";
    values += "COM1Baudrate|"; values += COM[1].Baudrate; values += "|input\n";
    values += "COM2Baudrate|"; values += COM[2].Baudrate; values += "|input\n";
    values += "TCPPort0|";     values += COM[0].Port;     values += "|input\n";
    values += "TCPPort1|";     values += COM[1].Port;     values += "|input\n";
    values += "TCPPort2|";     values += COM[2].Port;     values += "|input\n";
    values += "COM0Data|";     values += COM[0].Data;     values += "|input\n";
    values += "COM1Data|";     values += COM[1].Data;     values += "|input\n";
    values += "COM2Data|";     values += COM[2].Data;     values += "|input\n";
    values += "COM0Stop|";     values += COM[0].Stop;     values += "|input\n";
    values += "COM1Stop|";     values += COM[1].Stop;     values += "|input\n";
    values += "COM2Stop|";     values += COM[2].Stop;     values += "|input\n";
    values += "COM0Parity|";   values += COM[0].Parity;   values += "|input\n";
    values += "COM1Parity|";   values += COM[1].Parity;   values += "|input\n";
    values += "COM2Parity|";   values += COM[2].Parity;   values += "|input\n";

    for (int i = 0; i < NUM_HW_COM; i++) {
      snprintf(key, sizeof(key), "NMEAPass%u|", i);
      values += key;
      for (int j = 0; j < MAX_NO_FILTER; j++)
        if (COM[i].NMEAPass[j].length() > 0) values += COM[i].NMEAPass[j] + " ";
      values += "|input\n";

      snprintf(key, sizeof(key), "NMEABlock%u|", i);
      values += key;
      for (int j = 0; j < MAX_NO_FILTER; j++)
        if (COM[i].NMEABlock[j].length() > 0) values += COM[i].NMEABlock[j] + " ";
      values += "|input\n";
    }

    values += "headline|Serial Bridge - "; values += receiverName; values += "|div\n";
    values += "AP_pw|";           values += AP_pw;           values += "|input\n";
    values += "iWiFiSearchTime|"; values += iWiFiSearchTime; values += "|input\n";
    values += "iWiFiTxPower|";    values += iWiFiTxPower;    values += "|input\n";
    values += "updateView|click|button";
    request->send(HTTP_STATUS_OK, "text/plain", values);
  });

  server.on("/loadConsoleText", HTTP_ANY, [](AsyncWebServerRequest* request) {
    request->send(HTTP_STATUS_OK, "text/plain", "OK");
  });

  server.on("/restart", HTTP_ANY, handleRestart);

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);

  if (bLoginEnable)
    ElegantOTA.begin(&server, login_username.c_str(), login_password.c_str());
  else
    ElegantOTA.begin(&server);

  server.begin();
}
