// webserver.h — HTTP and WebSocket interface
//
// Purpose: expose the web server setup function and the helpers that other
//   modules call to interact with the web layer.
//
// Responsibilities:
//   - setup_webserver(): register all HTTP routes and start the server.
//   - handleSetting(): parse and apply settings from the /set endpoint.
//   - handleRestart(): serve the restart page and trigger ESP.restart().
//   - broadcastTrafficEvent(): push a traffic record to all WebSocket clients.
//   - wsCleanupClients(): remove dead WebSocket connections (called from loop()).
//   - Extract(): utility for parsing space-separated token lists (used for NMEA filters).
//
// Does NOT own: SPIFFS mounting, NVS persistence, UART initialization.
#pragma once
#include <ESPAsyncWebServer.h>

// Initialize and start the AsyncWebServer on port 80.
// Registers all HTTP routes, the /ws/traffic WebSocket, and ElegantOTA.
// Must be called after SPIFFS is mounted and WiFi is up.
void setup_webserver(void*);

// Handle the /set endpoint (GET or POST). Reads all parameters from the
// request, updates global variables in memory, and calls saveConfig().
void handleSetting(AsyncWebServerRequest* request);

// Serve /restart.htm, disconnect WiFi, and call ESP.restart().
void handleRestart(AsyncWebServerRequest* request);

// Broadcast one traffic record to all connected WebSocket clients.
// JSON format: {"d":"COM1 → TCP:8881","l":10,"t":12345,"h":"00 01 02 03..."}
//   d = direction string, l = byte count, t = millis() timestamp, h = hex dump.
// Safe to call from the bridge task (core 1); AsyncWebServer handles the
// cross-core delivery internally.
void broadcastTrafficEvent(const char* direction, const uint8_t* buf, uint16_t len);

// Remove disconnected WebSocket clients. Must be called from loop() (core 0)
// because AsyncWebServer client cleanup is not thread-safe from other cores.
void wsCleanupClients();

// Return the No-th whitespace-delimited token from Text (0-indexed).
// Used to split NMEA filter strings like "GGA RMC GSV" into individual IDs.
String Extract(int No, String Text);
