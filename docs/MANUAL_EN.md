# APRS LoRa+ NMEA 2000 — User manual

Version 5.4.5 · Copyright (C) 2026 Manuel - EA7JRS · GPL-3.0-or-later

## 1. What it is

A gateway between the **APRS LoRa+ tracker** and the boat's **NMEA 2000** bus. The APRS stations the tracker hears (with position, including objects and items) appear on the chartplotter (Garmin GPSMAP and others) as **AIS targets**, with their name, course and speed. Temperature and humidity from the XIAO-LOG HAT go out as environmental data.

> ⚠️ **These are not real AIS targets.** They are amateur radio stations, many of them on land. Do not use them for collision avoidance. See section 8 (alarms).

- Each station gets a **synthetic MMSI** `8 MID nnnnn` derived from its callsign: the same one the tracker uses in its NMEA 0183 output, so the target is the same everywhere. With the tracker's nautical AIS profile (the `AIS:` tag in the comment) the **real MMSI**, name, length, beam and the vessel's class A or B are used.
- Class B: PGN 129039 (position), 129809 and 129810 (name, callsign, type). Class A: 129038 and 129794.
- Environment: PGN 130311, 130312 and 130313 (outside temperature and humidity).
- From the bus it reads the boat's position (129025 / 129029) for the distance filter and the time (126992), which it stores in the HAT's clock.
- Listen only: it does not transmit by radio or send commands to the tracker.

## 2. Hardware

XIAO ESP32-S3, XIAO CAN Bus Expansion Board, XIAO-LOG (optional), a 12 V to 5 V converter and an NMEA 2000 drop cable. Assembly, the **D10 pin fix** on the XIAO-LOG and the bus wiring are in [HARDWARE.md](HARDWARE.md) (Spanish).

## 3. Installing the firmware

- **From a PC**: `pio run -e xiao_esp32s3_can -t upload` (PlatformIO).
- **Full image**: `factory.bin` from the build (CI artifact), at address `0x0` with `esptool.py --chip esp32s3 write_flash 0x0 factory.bin`.
- **Afterwards**: WebUI → *Actualizar firmware* (`/update`, with `firmware.bin`).

If it will not enter download mode: hold **B** (BOOT) on the XIAO, press **R** (RESET) and release B.

## 4. First start

The gateway creates the WiFi network **APRS-N2K** (password `aprsn2000`). Join it and open `http://192.168.4.1`.

- **WebUI password**: in the JSON configuration, `pin`. When set, saving changes or updating the firmware asks for user `admin` and that password.
- **WiFi client**: *WiFi = Cliente de otra red*, with the boat's network and password. If it cannot join within 30 s, it brings up its own network again (rescue).

## 5. Connecting the tracker

The tracker accepts **only one Bluetooth connection**: if the phone app is connected over BLE, the gateway must use **WiFi**.

### Over WiFi (recommended)

1. In the tracker's WebUI: enable **KISS WiFi** (port 8001).
2. Put the tracker on the same network: as a client of the gateway's **APRS-N2K** network, or both on the boat's network.
3. On the gateway, *Recibir del tracker = WiFi*. With an empty address it searches by itself: first by mDNS (`_kiss._tcp`, announced by the tracker) and then among the devices connected to its own network. If the tracker has a fixed IP, enter it.

### Over Bluetooth

1. On the tracker, **BLE KISS** enabled (WebUI > Bluetooth).
2. Disconnect the phone app from the tracker (or connect it over WiFi).
3. On the gateway, *Recibir del tracker = Bluetooth*. Empty = the first tracker advertising the KISS service; if there are several, enter part of its name.

*Los dos* (both) uses both links at once (repeated frames do no harm).

## 6. On the chartplotter

- With an anemometer: wind speed (PGN 130306, no direction) in the plotter's wind data.

- Turn on the plotter's **AIS** layer: targets appear with the station's name (or the vessel's name, with the AIS tag).
- The NMEA 2000 device list shows **APRS LoRa+ NMEA 2000** (AIS function), with its version and serial number.
- Outside temperature and humidity appear wherever the plotter shows environmental data.
- A target stops being repeated after **30 min** without hearing it (`ais.ttlMin`); the plotter removes it later, according to its own lost-AIS timeout.

## 7. WebUI

- **Tracker**: link (WiFi, BLE), bytes and frames received, positions, ignored and distant ones.
- **NMEA 2000**: bus open (if not, the MCP2515 does not answer: crystal, CS, wiring), bus address, messages sent and failures, devices heard, boat position and time.
- **Ambiente**: temperature, humidity, light, clock, battery; with an anemometer, 3-s wind, 2 and 10 min averages, gust, Beaufort and pulses per second (for calibration).
- **Blancos AIS**: name, MMSI, class, position, course and speed, distance to the boat and how long ago it was heard.
- **Últimos paquetes**: the last 20 in TNC2 text.
- **Configuración**: the most used settings in a form, and the whole file as JSON.

## 8. Collision alarms (CPA / TCPA)

Nearby APRS stations (in the harbour, on the coast) can trigger the plotter's **AIS collision alarms**. To avoid it:

- **Maximum distance** (`ais.maxKm`): only stations closer than that to the boat are sent, if the bus provides the boat's position.
- **Do not send** (`ais.ignore`): comma-separated callsigns, for example the tracker's own if the boat already has AIS.
- Adjust the AIS alarm distance and time on the plotter, or turn it off for "lost" targets.

## 9. Configuration

File `/gw_conf.json` (WebUI > Configuración > JSON):

| Key | Default | |
|---|---|---|
| `name` | `APRS-N2K` | own WiFi network, mDNS (`APRS-N2K.local`) and Bluetooth |
| `pin` | empty | WebUI password (user `admin`) |
| `wifi.mode` | 2 | 0 off, 1 client (with rescue), 2 access point |
| `wifi.ssid`, `wifi.pass` | — | boat network (mode 1) |
| `wifi.apPass` | `aprsn2000` | own network password (at least 8) |
| `tracker.src` | `tcp` | `tcp`, `ble` or `both` |
| `tracker.tcpHost`, `tracker.tcpPort` | empty, 8001 | tracker over WiFi (empty = search) |
| `tracker.bleName`, `tracker.bleAddr` | empty | tracker over BLE (part of the name or fixed address) |
| `ais.mid` | 224 | MID of the synthetic MMSI (224 = Spain), the same as the tracker |
| `ais.refreshS` | 30 | position repeat for each target |
| `ais.ttlMin` | 30 | minutes without hearing it before it stops being sent |
| `ais.staticMin` | 6 | name and type repeat |
| `ais.maxKm` | 0 | maximum distance (0 = all) |
| `ais.objects` | true | APRS objects and items too |
| `ais.ignore` | empty | callsigns that are not sent |
| `n2k.cs`, `n2k.mhz`, `n2k.int` | 44, 16, -1 | MCP2515 CS (D7), crystal (16 or 8 MHz), interrupt (-1 = none) |
| `n2k.addr` | 34 | preferred bus address (negotiated; the final one is stored) |
| `n2k.env`, `n2k.envS` | true, 5 | temperature and humidity to the bus, every how many seconds |
| `log.on`, `log.sda`, `log.scl` | true, 5, 6 | XIAO-LOG HAT over I2C (D4 / D5) |
| `log.battery`, `log.batPin` | false, 2 | measure the battery on D1 |
| `anemo.on`, `anemo.pin`, `anemo.pullup` | false, 4, true | cup anemometer on D3 (`docs/HARDWARE.md`) |
| `anemo.kmhPerHz` | 2.4 | km/h per pulse per second (calibration) |
| `anemo.ref` | apparent | apparent wind (on the boat) or `true` (on land, fixed) |

## 10. Troubleshooting

| Symptom | What to check |
|---|---|
| *Bus: sin MCP2515* | crystal (`n2k.mhz`), XIAO well seated on the CAN board, CS on D7 |
| Bus open but the plotter shows nothing | CAN-H and CAN-L swapped, no bus power, extra termination (P1), AIS layer off |
| *TCP: no encuentro el tracker* | KISS WiFi enabled on the tracker, both on the same network; try the tracker's IP |
| *BLE: no aparece el tracker* | phone app connected to the tracker over BLE (only one connection allowed), BLE KISS off |
| Empty environment data | the XIAO-LOG D10 fix; `log.on` |
| Collision alarms | section 8 |

## 11. Licence and credits

GPL-3.0-or-later. Libraries: NMEA2000 and NMEA2000_mcp (Timo Lappalainen, MIT), CAN_BUS_Shield (Seeed Studio, MIT), NimBLE-Arduino (Apache-2.0), ArduinoJson (MIT). The AIS table and the synthetic MMSI are the APRS LoRa+ tracker's.
