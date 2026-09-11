# Troubleshooting

**Device not reachable after saving settings**
Settings that change the AP SSID or password take effect on restart. Connect to the new network name after rebooting.

**No data forwarded, but TCP client connects**
Confirm the baud rate and framing (data bits / stop bits / parity) exactly match your serial device. An incorrect parity setting is a common cause.

**Traffic monitor shows CRC32 errors**
The device is receiving malformed frames. Check for cable noise, incorrect baud rate, or a device that does not implement msEnvelope framing. Raw serial devices must already produce msEnvelope-framed output.

**TCP → COM data missing from traffic monitor**
Verify the WebSocket connection is open (green badge in the traffic UI header).

**OTA update fails immediately**
Confirm the firmware was compiled with `ELEGANTOTA_USE_ASYNC_WEBSERVER=1` (already set in `platformio.ini`).

**TunerStudio shows "Not connected"**
- Confirm the device IP and port (`192.168.4.1:8881` in AP mode).
- Check that COM1 baud rate in the web UI matches the ECU's serial baud rate (Speeduino default: 115200).
- Open the Traffic Monitor and verify frames appear when TunerStudio tries to connect.
