# Building & Flashing

## Requirements

- [PlatformIO](https://platformio.org/) CLI or IDE extension
- USB cable to the ESP32-S3's native USB port (for first flash)

## Compile and flash firmware

```bash
pio run -t upload
```

## Upload web UI to SPIFFS

The web interface lives in `data/` and must be flashed separately to the SPIFFS partition:

```bash
pio run -t uploadfs
```

## Serial monitor

```bash
pio device monitor
```

---

## OTA firmware update

Once the device is on your network, you can update firmware without a USB cable:

1. Navigate to `http://<device-ip>/update` in your browser.
2. Drag and drop `.pio/build/esp32-s3/firmware.bin` onto the page.
3. To update the web UI files, switch to **Filesystem** mode and upload, or re-run `pio run -t uploadfs` over USB.

> The firmware must be compiled with `ELEGANTOTA_USE_ASYNC_WEBSERVER=1` (already set in `platformio.ini`).
