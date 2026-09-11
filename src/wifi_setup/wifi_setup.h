// wifi_setup.h — WiFi initialization interface
//
// Purpose: expose setup_wifi(), the single entry point for bringing up the
//   WiFi stack in whichever mode the current configuration requires.
//
// Three supported modes (selected by global flags at call time):
//   1. AP-only     — always reachable at 192.168.4.1; no upstream connectivity.
//   2. AP + STA    — broadcasts its own AP while also connecting to a router.
//   3. STA-only    — connects to a router; AP not broadcast.
//
// The function is recursive: if a STA connection attempt times out, it sets
// connectFailed = true and calls itself to fall back to AP-only mode.
#pragma once

// Initialize WiFi. Pass NULL for the void* argument (reserved for future use
// as a FreeRTOS task entry point if WiFi setup is ever moved to its own task).
void setup_wifi(void*);
