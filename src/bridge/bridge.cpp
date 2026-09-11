// bridge.cpp — Serial ↔ TCP data bridge
//
// Purpose: implement the two-direction data path between hardware UARTs and
//   TCP clients. Called continuously from the bridge FreeRTOS task on core 1.
//
// msEnvelope frame format (COM → TCP direction only):
//   ┌────────┬───────────┬─────────────────┬────────────────┐
//   │ 0x00   │ LEN (1 B) │ Payload (LEN B) │ CRC32 (4 B BE) │
//   └────────┴───────────┴─────────────────┴────────────────┘
//   Total = 2 + LEN + 4 bytes. LEN must be > 0.
//   CRC32 is computed over the payload only (not the 2-byte header).
//   The CRC is stored big-endian (most-significant byte first).
//
// The TCP → COM direction (PortHandler) forwards raw bytes without framing.
// The frame protocol is only required by the ECU/device on the UART side.
//
// State machine in COMHandler:
//   Phase 1 — header: wait until 2 bytes are in buf2. Check byte[0]==0x00
//             and byte[1]>0; compute expected total frame length.
//   Phase 2 — payload + CRC: keep reading until all expected bytes arrive.
//   A 400 ms timeout discards any partial frame so a dropped byte can't
//   permanently stall the channel. The timeout is generous enough to cover
//   the slowest supported baud rate (4800 Bd) for a max-size frame (~400 ms).
#include "../globals.h"
#include "../webserver/webserver.h"
#include <FastCRC.h>

static FastCRC32 CRC32;

// Per-channel framing state (persists between calls to COMHandler).
static uint16_t frameTarget[NUM_HW_COM];  // total bytes expected for current frame; 0 = waiting for header
static uint32_t frameStartMs[NUM_HW_COM]; // millis() when the first byte of the current frame arrived

static void logHex(const char* direction, const uint8_t* buf, uint16_t len) {
  broadcastTrafficEvent(direction, buf, len);
  if (Debug_COM == NULL || len == 0) return;
  Debug_COM->print(direction);
  Debug_COM->print(" [");
  Debug_COM->print(len);
  Debug_COM->print(" bytes]: ");
  for (uint16_t i = 0; i < len; i++) {
    if (buf[i] < 0x10) Debug_COM->print("0");
    Debug_COM->print(buf[i], HEX);
    Debug_COM->print(" ");
  }
  Debug_COM->println();
}


int PortHandler(int num) {
  if (hwserver[num] == NULL) return 0;

  // Accept new incoming connections into the first free client slot.
  if (hwserver[num]->hasClient()) {
    bool found = false;
    int i = 0;
    while (!found && i < MAX_NMEA_CLIENTS) {
      if (!hwclient[num][i] || !hwclient[num][i].connected()) {
        if (hwclient[num][i]) hwclient[num][i].stop();
        hwclient[num][i] = hwserver[num]->available();
        if (Debug_COM != NULL) Debug_COM->print("New client for COM");
        if (Debug_COM != NULL) Debug_COM->print(num);
        if (Debug_COM != NULL) Debug_COM->print(" Client #");
        if (Debug_COM != NULL) Debug_COM->println(i);
        found = true;
      }
      i++;
    }
    // All slots are occupied — refuse the connection cleanly.
    if (!found) {
      WiFiClient tmp = hwserver[num]->available();
      tmp.stop();
    }
  }

  // Drain all bytes from every connected client and write them to the UART.
  // All clients on the same port feed the same UART; their data is interleaved.
  for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++) {
    if (hwclient[num][cln]) {
      while (hwclient[num][cln].available()) {
        buf1[num][i1[num]] = (uint8_t)hwclient[num][cln].read();
        if (i1[num] < bufferSize - 1) i1[num]++;
      }
      if (i1[num] > 0) {
        // COM1 is the ECU port (Speeduino). Light the red LED to distinguish
        // host→ECU traffic from ECU→host traffic (blue LED, set in COMHandler).
        if (num == 1) ledRedOffTime = millis() + 50;
        char dir[24];
        snprintf(dir, sizeof(dir), "TCP:%d → COM%d", COM[num].Port, num);
        logHex(dir, buf1[num], i1[num]);
        COM_write(num, buf1[num], i1[num]);
      }
      i1[num] = 0;
    }
  }
  return 0;
}

int COMHandler(int num) {
  // Discard any partial frame that has been waiting too long.
  // This handles the case where the device sends a bad header or a frame is
  // truncated mid-transmission — without the timeout, a single lost byte
  // would permanently desync the framing state for this channel.
  if (i2[num] > 0 && millis() - frameStartMs[num] > 400) {
    if (Debug_COM != NULL) Debug_COM->println("msEnvelope timeout, discarding");
    i2[num] = 0;
    frameTarget[num] = 0;
  }

  if (!COM_available(num)) {
    yield();
    return 0;
  }

  while (COM_available(num)) {
    if (i2[num] == 0) frameStartMs[num] = millis(); // stamp first byte of new frame
    buf2[num][i2[num]++] = (uint8_t)COM_read(num);

    // Safety guard: discard oversized data that cannot be a valid frame.
    if (i2[num] >= bufferSize - 1) {
      i2[num] = 0;
      frameTarget[num] = 0;
      return 0;
    }

    // Phase 1: once we have 2 bytes, validate the header and compute expected length.
    if (frameTarget[num] == 0 && i2[num] == 2) {
      if (buf2[num][0] == 0x00 && buf2[num][1] > 0) {
        frameTarget[num] = 2u + (uint16_t)buf2[num][1] + 4u; // header + payload + CRC32
      } else {
        if (Debug_COM != NULL)
          Debug_COM->printf("msEnvelope bad header: %02X %02X\n", buf2[num][0], buf2[num][1]);
        i2[num] = 0;
        return 0;
      }
    }

    // Phase 2: stop reading as soon as the full frame is buffered.
    if (frameTarget[num] > 0 && i2[num] >= frameTarget[num]) break;
  }

  // Frame not yet complete — preserve buffer state and wait for more bytes.
  if (frameTarget[num] == 0 || i2[num] < frameTarget[num]) return 0;

  // ── CRC32 validation ────────────────────────────────────────────────────
  frameTarget[num] = 0;
  uint16_t payloadLen = buf2[num][1];
  uint16_t totalFrame = 2u + payloadLen + 4u;

  uint32_t computed  = CRC32.crc32(buf2[num] + 2, payloadLen);  // CRC over payload only
  uint32_t packetCRC = ((uint32_t)buf2[num][2 + payloadLen    ] << 24) |
                       ((uint32_t)buf2[num][2 + payloadLen + 1] << 16) |
                       ((uint32_t)buf2[num][2 + payloadLen + 2] <<  8) |
                        (uint32_t)buf2[num][2 + payloadLen + 3];

  if (computed != packetCRC) {
    if (Debug_COM != NULL) {
      Debug_COM->print("CRC32 err: rcv=");
      Debug_COM->print(packetCRC, HEX);
      Debug_COM->print(" calc=");
      Debug_COM->println(computed, HEX);
    }
    i2[num] = 0;
    return 0;
  }

  // ── Dispatch validated frame ─────────────────────────────────────────────
  if (num == 1) ledOffTime = millis() + 50; // blue LED: ECU → host data
  char dir[24];
  snprintf(dir, sizeof(dir), "COM%d → TCP:%d", num, COM[num].Port);
  logHex(dir, buf2[num], totalFrame);

  // Broadcast to all TCP clients on this port.
  for (byte cln = 0; cln < MAX_NMEA_CLIENTS; cln++)
    if (hwclient[num][cln])
      hwclient[num][cln].write((char*)buf2[num], totalFrame);

  // Forward to any cross-routed UARTs (COM[num].xCOM[x] set via web UI).
  for (int x = 0; x < NUM_HW_COM; x++)
    if (COM[num].xCOM[x])
      COM_write(x, buf2[num], totalFrame);

  i2[num] = 0;
  return 0;
}
