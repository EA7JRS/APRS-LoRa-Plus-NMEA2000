# APRS LoRa+ NMEA 2000 — Quick start

1. **Assemble**: XIAO ESP32-S3 on the CAN board and, if used, the XIAO-LOG on top **with its D10 pin not joined to the XIAO and wired to 3V3** ([HARDWARE.md](HARDWARE.md), Spanish).
2. **Install** the firmware: `pio run -e xiao_esp32s3_can -t upload`, or `factory.bin` at `0x0`.
3. **NMEA 2000 bus**: CAN-H to white (pin 4), CAN-L to blue (pin 5), GND to black (pin 3); red (12 V) to a 5 V converter for the XIAO. **Do not** close jumper P1 (termination).
4. **WiFi**: join **APRS-N2K** (password `aprsn2000`) and open `http://192.168.4.1`. To save changes or update: user `admin`, password `123456` (the ecosystem PIN; change it or leave it empty in *Configuración*).
5. **Tracker**: in its WebUI, enable **KISS WiFi** and set its WiFi as a client of **APRS-N2K** (or both on the boat's network). The gateway finds it by itself. Over Bluetooth: *Recibir del tracker = Bluetooth*; on the tracker (or the MAPRS), Bluetooth set to **BLE** (not *Classic*) with KISS, and neither the app nor the plotter connected to it over BLE (it allows only one).
6. **Chartplotter**: AIS layer on. APRS stations appear as AIS targets with their name.
7. **Anemometer** (optional): reed switch between D3 and GND; *Anemómetro (D3) = Sí* in the configuration. Calibrate with *km/h por impulso por segundo* (2.4 by default) watching the Hz under *Ambiente*.
8. **Collision alarms**: set a **maximum distance** (`ais.maxKm`) or list the callsigns you do not want in **No enviar**.

> These are not real AIS targets: do not use them for collision avoidance.

Full manual: [MANUAL_EN.md](MANUAL_EN.md).
