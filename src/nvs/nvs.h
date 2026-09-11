// nvs.h — Non-volatile storage (NVS) persistence interface
//
// Purpose: persist and restore all user-configurable settings across reboots.
//   Settings are stored in the ESP32 NVS (Non-Volatile Storage) partition
//   using the Arduino Preferences library, under the namespace "LK8000Bridge".
//
// The namespace name "LK8000Bridge" is inherited from the original project and
// preserved to avoid losing existing device settings on upgrade.
//
// Responsibilities:
//   - saveConfig(): write all current global variables to NVS.
//   - loadConfig(): read NVS into global variables; apply defaults for any
//     key that has never been written (first boot or after a factory reset).
//
// Does NOT own: the Preferences object (defined in config.cpp as `preferences`).
#pragma once

// Persist all configurable settings to NVS. Called by the web server after
// any settings change via the /set endpoint.
void saveConfig();

// Load settings from NVS into global variables. Called once during setup()
// before any UART or WiFi initialization, so that everything starts with the
// last-saved configuration.
void loadConfig();
