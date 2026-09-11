// uart.h — UART hardware management
//
// Purpose: defines UARTParam (the per-port configuration struct) and exposes
//   thread-safe wrappers around HardwareSerial.
//
// Responsibilities:
//   - Own the UARTParam type (one instance per physical UART).
//   - Initialize UART peripherals with the settings stored in UARTParam.
//   - Provide mutex-guarded read/write so the bridge task (core 1) and the
//     web server (core 0) cannot corrupt each other's transfers.
//
// Does NOT own: TCP sockets, WiFi, NVS persistence.
#pragma once
#include <Arduino.h>
#include "config.h"

// Configuration and run-time state for one hardware UART port.
// One instance exists per port, stored in globals.h COM[].
struct UARTParam {
  int  Port;     // TCP port number this UART is bridged to (default: 8880/8881/8882)
  long Baudrate;
  char Parity;   // 'N' = none, 'E' = even, 'O' = odd
  long Stop;     // 1 or 2 stop bits
  long Data;     // data bits: 5, 6, 7, or 8

  int RxPin;
  int TxPin;

  // Cross-routing matrix: xCOM[x] == true means frames received on this UART
  // are also forwarded to COM[x]'s UART. Allows one serial device's output
  // to fan out to multiple destinations without a host in the loop.
  boolean xCOM[NUM_HW_COM];

  // NMEA sentence filter lists (space-separated 3-letter talker IDs, e.g. "GGA RMC").
  // NoNMEAPass / NoNMEABlock track how many filter entries are actually set.
  // (Filtering itself is not yet implemented in bridge.cpp — these are stored
  //  and persisted but not applied to frame routing in the current version.)
  int    NoNMEAPass;
  int    NoNMEABlock;
  String NMEAPass[MAX_NO_FILTER];
  String NMEABlock[MAX_NO_FILTER];

  // FreeRTOS mutexes protecting HardwareSerial access.
  // The bridge task (core 1) and the web server task (core 0) both call
  // InitSerial() on settings changes, so every read/write must be guarded.
  SemaphoreHandle_t WriteSemaphore;
  SemaphoreHandle_t ReadSemaphore;
};

// Initialize (or re-initialize) the hardware UART for port No.
// Called on boot for all ports, and again whenever the user changes COM
// settings via the web UI. Safe to call while the bridge task is running.
void InitSerial(int No);

// Returns true if bytes are waiting in the UART RX FIFO. Not mutex-guarded
// because HardwareSerial::available() is read-only and atomic on ESP32.
bool COM_available(uint8_t num);

// Thread-safe single-byte read from UART num. Blocks until mutex is acquired.
int COM_read(uint8_t num);

// Thread-safe bulk write to UART num. Blocks until mutex is acquired.
size_t COM_write(uint8_t num, const uint8_t* buf, uint16_t len);
