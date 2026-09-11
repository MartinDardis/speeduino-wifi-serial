// bridge.h — Serial ↔ TCP bridging interface
//
// Purpose: expose the two handler functions that form the core data path.
//   Both are called from the bridge FreeRTOS task (core 1) in a tight loop.
//
// PortHandler: TCP → UART direction
//   Accepts new TCP client connections and forwards received bytes to the UART.
//
// COMHandler: UART → TCP direction
//   Reads bytes from the UART, assembles them into a complete msEnvelope frame,
//   validates the CRC32, then broadcasts the frame to all connected TCP clients
//   and any cross-routed UARTs.
#pragma once

// Process incoming TCP connections and data for port num (0, 1, or 2).
// Accepts up to MAX_NMEA_CLIENTS simultaneous connections; refuses extras.
// Returns 0 always (errors are logged, not propagated).
int PortHandler(int num);

// Process UART receive data for port num (0, 1, or 2).
// Implements the msEnvelope framing state machine. Returns 0 always.
int COMHandler(int num);
