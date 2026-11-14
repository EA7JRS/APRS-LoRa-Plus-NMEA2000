# APRS LoRa+ NMEA 2000

Pasarela del tracker **APRS LoRa+** al bus **NMEA 2000** del barco: las estaciones APRS que oye el tracker salen en el plotter (Garmin GPSMAP y otros) como **blancos AIS**, y la temperatura y la humedad del HAT XIAO-LOG como datos de ambiente.

Copyright (C) 2026 Manuel - EA7JRS · GPL-3.0-or-later · versión **5.4.5** (la misma que el tracker, el rotor, la app y el gateway).

```
 Tracker APRS LoRa+ ──KISS por WiFi (TCP 8001) o por BLE──► XIAO ESP32-S3 + CAN (MCP2515) ──NMEA 2000──► plotter
                                                                │
                                                          HAT XIAO-LOG (temperatura, humedad, luz, reloj)
```

- **Blancos AIS**: posiciones APRS sin comprimir, comprimidas y Mic-E, objetos e ítems, tráfico de terceros; rumbo, velocidad y altitud. MMSI ficticio `8 MID nnnnn` (el mismo que el tracker en NMEA 0183) o el real con la etiqueta `AIS:` del perfil náutico. Clase B (129039, 129809, 129810) o A (129038, 129794).
- **Ambiente**: 130311, 130312, 130313; anemómetro de cazoletas (opcional, D3): 130306.
- **Del bus**: posición del barco (filtro de distancia) y hora (al reloj del HAT).
- **WebUI**: estado del enlace y del bus, blancos, ambiente, últimos paquetes, configuración y actualización del firmware.
- **Solo escucha**: no transmite por radio.

## Red, WebUI y claves

La pasarela trae WiFi y WebUI propios (puerto 80: `/`, `/api/status`, `/api/config`, `/update` para OTA).

| | Por defecto | Notas |
|---|---|---|
| Red WiFi propia (AP) | **`APRS-N2K`** / clave **`aprsn2000`** | `wifi.mode` 2 = punto de acceso; abre `http://192.168.4.1`. La clave del AP necesita ≥ 8 caracteres. |
| WiFi cliente | — | `wifi.mode` 1: se une a la red del barco (`wifi.ssid`/`wifi.pass`); si no entra en 30 s levanta el AP de rescate. |
| Clave de la WebUI | **`123456`** | El mismo PIN de control del ecosistema APRS LoRa+ (tracker, plotter, app). HTTP Basic, usuario `admin`; solo se pide para **cambiar** la configuración o actualizar. Déjala vacía para no pedir clave; **cámbiala** si la pasarela es accesible desde la red del barco. |

Nombre común `APRS-N2K` para la red/AP, mDNS (`APRS-N2K.local`) y Bluetooth. Detalle en el [manual](docs/MANUAL_ES.md#4-primer-arranque).

## Documentación

- **Manual de usuario**: [español](docs/MANUAL_ES.md) · [English](docs/MANUAL_EN.md)
- **Guía rápida**: [español](docs/QUICKSTART_ES.md) · [English](docs/QUICKSTART_EN.md)
- [docs/HARDWARE.md](docs/HARDWARE.md): material, pines, **arreglo del pin D10 del XIAO-LOG**, conexión al bus NMEA 2000

## Compilar

```
pio run -e xiao_esp32s3_can -t upload     # firmware (PlatformIO)
make -C test_host run                     # pruebas del núcleo en el PC
```

## Estructura

```
lib/GateCore/src/   núcleo en C++ sin Arduino: KISS, AX.25, APRS (gc_aprs.h), tabla AIS y MMSI (nmea0183, ais_vessel: los del tracker), motor (gc_gate.h)
src/                firmware: config, tracker_link (BLE / TCP), n2k (NMEA 2000), loghat (XIAO-LOG), net (WiFi, WebUI)
test_host/          pruebas en el PC
```

Librerías: NMEA2000 y NMEA2000_mcp (Timo Lappalainen, MIT), CAN_BUS_Shield (Seeed Studio, MIT), NimBLE-Arduino, ArduinoJson.
