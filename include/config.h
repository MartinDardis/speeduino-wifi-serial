// config.h — Compile-time constants
//
// Purpose: single source of truth for hardware pin assignments, sizing limits,
//   and UART bit-mask values. No types, no extern declarations — those live in
//   uart.h and globals.h respectively.
//
// Who includes this: uart.h (for struct field types), and transitively every
//   module that includes globals.h.
#pragma once

// ─── Firmware ──────────────────────────────────────────────────────────────
#define VERSION "1.07"

// ─── LED ───────────────────────────────────────────────────────────────────
#define LED_PIN        48  // WS2812B NeoPixel, onboard on ESP32-S3-DevKitC-1
#define LED_BRIGHTNESS 30  // 0-255; kept low to stay within USB current budget

// ─── Port counts & buffer sizes ────────────────────────────────────────────
#define NUM_HW_COM       3    // number of hardware UART ports (0, 1, 2)
#define BT_IDX           3    // reserved index — placeholder for a future BT port
#define MAX_NMEA_CLIENTS 4    // simultaneous TCP clients accepted per UART port
#define MAX_NO_FILTER    2    // NMEA sentence filter slots per port (pass & block lists)
#define bufferSize       250  // RX/TX frame buffer in bytes; must fit one msEnvelope frame

// ─── HTTP ──────────────────────────────────────────────────────────────────
#define HTTP_STATUS_OK 200

// ─── UART pin assignments (ESP32-S3-DevKitC-1) ─────────────────────────────
// NOTE: SERIAL1 and SERIAL2 share the same RX pin (GPIO 15). The intent is
// that only one of them is used for input at a time; using both simultaneously
// will cause bus contention.
#define SERIAL0_RXPIN 21
#define SERIAL0_TXPIN  1
#define SERIAL1_RXPIN 15
#define SERIAL1_TXPIN 17
#define SERIAL2_RXPIN 15
#define SERIAL2_TXPIN  4

// ─── UART config bit masks ─────────────────────────────────────────────────
// These values match the ESP32 Arduino UART_CONFIG_T / uart_word_length_t /
// uart_stop_bits_t / uart_parity_t encoding used by HardwareSerial::begin().
// They are OR-ed together with a base mask of 0x8000000 (no hardware flow control)
// to build the final config word passed to the ESP32 UART driver.

// Data bits
#define UART_5_BIT 0x00
#define UART_6_BIT 0x04
#define UART_7_BIT 0x08
#define UART_8_BIT 0x0c

// Stop bits
#define UART_1_STOPBIT 0x10
#define UART_2_STOPBIT 0x30

// Parity
#define UART_NO_PARITY   0x00
#define UART_EVEN_PARITY 0x02
#define UART_ODD_PARITY  0x03
