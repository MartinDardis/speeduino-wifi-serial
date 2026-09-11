# Protocol — msEnvelope

The bridge implements the **msEnvelope** binary framing protocol for data arriving from the UART side (COM → TCP). This is the same protocol used by Megasquirt ECUs, which is why TunerStudio recognises the bridge as a native Megasquirt connection.

Raw bytes sent by a TCP client (TCP → COM) are forwarded to the UART as-is, without additional framing.

## Frame structure (COM → TCP)

```
┌─────────┬──────────────┬─────────────────────────┬──────────────────┐
│ 0x00    │ LEN  (1 byte)│ Payload  (LEN bytes)    │ CRC32  (4 bytes) │
│ (sync)  │ > 0          │                         │ big-endian       │
└─────────┴──────────────┴─────────────────────────┴──────────────────┘
  Total = 2 + LEN + 4 bytes   (max LEN = 248)
```

- **Sync byte** `0x00` marks the start of every frame.
- **LEN** is the payload length in bytes (must be > 0).
- **CRC32** is computed over the payload only (not the 2-byte header), stored most-significant byte first.
- Partial frames time out after **400 ms** and are discarded.

## TCP connection details

Connect any TCP client to one of the three ports:

```
<device-ip>:8880  ←→  COM0 (UART0)
<device-ip>:8881  ←→  COM1 (UART1)
<device-ip>:8882  ←→  COM2 (UART2)
```

Up to **4 simultaneous clients** per port. All clients on the same port share the UART — incoming TCP data is merged and sent to the UART, and every valid frame is broadcast to all connected clients.

## LED indicators

The onboard WS2812B RGB LED (GPIO 48) signals bridge activity:

| Color | Meaning |
|-------|---------|
| Blue | Valid msEnvelope frame received from ECU (COM1 → TCP) |
| Red | Data received from TCP client and forwarded to COM1 (TCP → COM1) |
| Cyan | Both directions active simultaneously |
| Off | No traffic |

Each event keeps the LED on for 50 ms.
