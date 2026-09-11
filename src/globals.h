// globals.h — Shared application state
//
// Purpose: single include that gives any translation unit access to all
//   globally-shared variables. Acts as the "glue" header; it transitively
//   pulls in config.h (constants) and uart.h (UARTParam struct).
//
// Design note: all variables declared here are *defined* in config.cpp.
//   No module should introduce additional shared mutable state outside of
//   this file. If new shared state is needed, add it here and define it in
//   config.cpp.
//
// Include order: config.h → uart.h → globals.h → (module headers as needed)
#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <Preferences.h>
#include <ESPAsyncWebServer.h>
#include "config.h"
#include "uart/uart.h"

// ─── UART port configuration ───────────────────────────────────────────────
extern UARTParam      COM[NUM_HW_COM];   // per-port settings and run-time state
extern HardwareSerial* HW_SERIAL[3];    // points to &Serial0, &Serial1, &Serial2

// ─── I/O buffers ──────────────────────────────────────────────────────────
// buf1: TCP → COM direction (bytes received from a TCP client, forwarded to UART)
extern uint8_t  buf1[NUM_HW_COM][bufferSize];
extern uint16_t i1[NUM_HW_COM];  // write cursor into buf1

// buf2: COM → TCP direction (bytes received from UART, forwarded to TCP clients)
// Allocated as bufferSize + 2 so a maximum-size msEnvelope frame (header 2 B +
// payload 249 B + CRC32 4 B = 255 B) always fits without overflowing bufferSize.
extern uint8_t  buf2[NUM_HW_COM][bufferSize + 2];
extern uint16_t i2[NUM_HW_COM];  // write cursor into buf2

// ─── TCP layer ────────────────────────────────────────────────────────────
extern WiFiServer* hwserver[NUM_HW_COM];                    // one listening server per UART port
extern WiFiClient  hwclient[NUM_HW_COM][MAX_NMEA_CLIENTS];  // active client connections per port

// ─── Web server ───────────────────────────────────────────────────────────
extern AsyncWebServer server;     // single AsyncWebServer on port 80
extern Preferences    preferences; // NVS storage handle (namespace "LK8000Bridge")

// ─── Debug output ─────────────────────────────────────────────────────────
// Using Print* instead of a concrete type lets us redirect debug output at
// runtime (e.g. point to a null sink during production). Currently points to
// TcpLogger (defined in config.cpp), which mirrors every write to both the
// USB-CDC Serial port and all TCP clients connected on port 8880.
extern Print* Debug_COM;

// ─── Authentication ───────────────────────────────────────────────────────
extern bool   bLoginEnable;     // gates ElegantOTA behind login_username / login_password
extern String login_username;
extern String login_password;

// ─── WiFi / network ───────────────────────────────────────────────────────
extern String receiverName;     // device display name, used as the AP SSID
extern bool   openAP;           // true = broadcast AP even when STA is connected
extern bool   connectFailed;    // true after a STA connect attempt times out; triggers AP-only fallback
extern String WiFiClientState;  // human-readable connection status string sent to the web UI
extern String AP_pw;
extern String WiFi_ssid;
extern String WiFi_pw;
extern int    iWiFiSearchTime;  // STA connect timeout in seconds; 0 = STA disabled
extern int    iWiFiTxPower;     // WiFi TX power in units of 0.25 dBm (e.g. 20 = 5 dBm max)

// ─── Debug flags ──────────────────────────────────────────────────────────
extern bool bSioDebugInit;   // enable initialization log messages
extern bool bSioDebugError;  // enable error/warning log messages

// ─── LED timing ───────────────────────────────────────────────────────────
// These are set to millis() + N to request an LED-on period of N milliseconds.
// Declared volatile because they are written from the bridge task (core 1)
// and read from loop() (core 0) without a mutex.
extern volatile uint32_t ledOffTime;     // blue: active while COM → TCP frames arrive
extern volatile uint32_t ledRedOffTime;  // red: active while TCP → COM data arrives on port 1
