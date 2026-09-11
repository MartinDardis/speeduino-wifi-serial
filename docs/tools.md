# Desktop tools

## proxy.py — transparent proxy & traffic logger

`tools/proxy.py` sits between TunerStudio and the bridge (or a direct serial device), logging every packet in both directions to stdout and to a `.txt` file. It creates a **virtual serial port (PTY)** that TunerStudio connects to — no changes needed in TunerStudio's configuration other than the port name.

Useful for:
- Debugging communication issues between TunerStudio and the ECU
- Capturing a raw traffic log to share when reporting bugs
- Testing the bridge without a physical ECU (connect via TCP, inspect what TunerStudio sends)

### TCP mode (bridge over WiFi)

```bash
python tools/proxy.py 192.168.4.1
python tools/proxy.py 192.168.4.1 8881 --log session.txt
```

The script prints the virtual port name (e.g. `/dev/ttys004`). Point TunerStudio at that port.

### Serial USB mode (direct ECU connection)

```bash
python tools/proxy.py /dev/cu.usbserial-1410 --baud 115200
python tools/proxy.py /dev/cu.usbmodem101 --baud 57600 --log session.txt
```

Intercepts traffic on a physical USB-to-serial adapter. Requires `pyserial`:

```bash
pip install pyserial
```

### Output format

Each line shows a timestamp, direction, byte count, and hex dump:

```
[12:34:56.789] TS → Bridge   [  4b]  00 51 CE 6E
[12:34:56.812] Bridge → TS   [ 78b]  00 4D 6F 52 ...
```

The log is appended to `traffic.txt` by default (change with `--log`).

---

## monitor.py — interactive TCP console

`tools/monitor.py` connects to one of the bridge's TCP ports and lets you send and receive text interactively. Handy for quick manual testing of a serial port.

```bash
python tools/monitor.py 192.168.4.1 8880
```

Defaults to `192.168.4.1:8880` if no arguments are given. Press `Ctrl+C` to disconnect.
