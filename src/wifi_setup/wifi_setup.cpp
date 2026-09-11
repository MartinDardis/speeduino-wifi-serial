// wifi_setup.cpp — WiFi stack initialization
//
// Purpose: implement setup_wifi(), which brings up the WiFi interface in the
//   mode selected by the current configuration.
//
// Mode selection logic (evaluated in order):
//   connectFailed == true   → AP-only (STA connect timed out on a previous attempt)
//   openAP == true          → AP-only (user explicitly chose AP-only mode)
//   iWiFiSearchTime == 0    → AP + no STA attempt (station disabled)
//   otherwise               → AP + STA (try to join the configured network)
//
// STA connect timeout:
//   The function waits up to iWiFiSearchTime * 2 half-second polls. If the
//   connection is not established in that window, it sets connectFailed = true
//   and recurses to enter AP-only mode. The recursion depth is at most 2
//   (once for the failed attempt, once for the fallback).
#include "../globals.h"
#include "wifi_setup.h"
#include <esp_wifi.h>
#include <WiFi.h>

void setup_wifi(void*) {
  if (connectFailed) {
    // Fallback: STA connect failed on a previous call — AP-only mode.
    WiFi.mode(WIFI_AP);
    WiFi.softAP(receiverName.c_str(), AP_pw.c_str());
    if (bSioDebugInit) Serial.println("Deactivating Client WiFi.");
    if (bSioDebugInit) Serial.print("Created WiFi Access Point: '");
    if (bSioDebugInit) Serial.print(receiverName);
    if (bSioDebugInit) Serial.print("' Password: '");
    if (bSioDebugInit) Serial.print(AP_pw);
    if (bSioDebugInit) Serial.println("'");
    if (bSioDebugInit) Serial.print("IP address: ");
    if (bSioDebugInit) Serial.println(WiFi.softAPIP());
  } else if (openAP) {
    // AP-only mode (user preference, not a fallback).
    WiFi.mode(WIFI_AP);
    WiFi.softAP(receiverName.c_str(), AP_pw.c_str());
    if (bSioDebugInit) Serial.println("");
    if (bSioDebugInit) Serial.print("Created WiFi: '");
    if (bSioDebugInit) Serial.print(receiverName);
    if (bSioDebugInit) Serial.print("' Password: '");
    if (bSioDebugInit) Serial.print(AP_pw);
    if (bSioDebugInit) Serial.println("'");
    if (bSioDebugInit) Serial.print("IP address: ");
    if (bSioDebugInit) Serial.println(WiFi.softAPIP());
  } else {
    // STA mode will be configured below; set a temporary mode first.
    WiFi.mode(WIFI_STA);
  }

  if (connectFailed) return; // AP-only fallback is complete; nothing more to do.

  WiFi.disconnect(true);

  if (iWiFiSearchTime == 0) {
    // Station mode explicitly disabled — stay AP-only with no connection attempt.
    if (bSioDebugInit) Serial.print("WiFi Station mode disabled!");
  } else {
    // Attempt to join the configured network while keeping the AP active.
    WiFi.mode(WIFI_MODE_APSTA);
    if (bSioDebugInit) Serial.println("");
    if (bSioDebugInit) Serial.print("Connecting to WiFi: '");
    if (bSioDebugInit) Serial.print(WiFi_ssid);
    if (bSioDebugInit) Serial.print("' Password: '");
    if (bSioDebugInit) Serial.print(WiFi_pw);
    if (bSioDebugInit) Serial.println("'");

    WiFi.begin(WiFi_ssid.c_str(), WiFi_pw.c_str());
    if (!openAP) WiFi.mode(WIFI_STA); // drop AP if user didn't ask for it

    int iCnt = 0;
    while ((WiFi.status() != WL_CONNECTED) &&
           ((iCnt < (iWiFiSearchTime * 2)) || (iWiFiSearchTime < 0))) {
      delay(500);
      if (bSioDebugInit) Serial.print(".");
      iCnt++;
    }
    if (bSioDebugInit) Serial.println("");

    if ((iWiFiSearchTime > 0) && (iCnt >= (iWiFiSearchTime * 2))) {
      // Timed out — remember the failure and fall back to AP-only.
      if (bSioDebugError) Serial.print("Failed to connect to '");
      if (bSioDebugError) Serial.print(WiFi_ssid);
      if (bSioDebugError) Serial.println("'");
      WiFiClientState  = "Failed to connect to '";
      WiFiClientState += WiFi_ssid;
      WiFiClientState += "'";
      connectFailed = true;
      setup_wifi(NULL); // recurse once to enter AP-only fallback
    } else {
      if (bSioDebugInit) Serial.println("");
      if (bSioDebugInit) Serial.print("Connected to: ");
      if (bSioDebugInit) Serial.print(WiFi_ssid);
      if (bSioDebugInit) Serial.print(" IP address: ");
      if (bSioDebugInit) Serial.println(WiFi.localIP());
    }
  }

  esp_wifi_set_max_tx_power(iWiFiTxPower);
}
