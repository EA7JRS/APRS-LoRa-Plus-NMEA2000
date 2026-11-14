# APRS LoRa+ NMEA 2000 — contexto del proyecto

Propietario: Manuel (EA7JRS). Idioma de trabajo: español, respuestas breves y directas.
Archivos nuevos con la cabecera `Copyright (C) 2026 Manuel - EA7JRS` y licencia GPL-3.0-or-later.
Repositorio: `EA7JRS/APRS-LoRa-Plus-NMEA2000` (privado; pedido por Manuel el 2026-11-09). Proyectos hermanos: tracker `EA7JRS/APRS-LoRa-Plus`, rotor `EA7JRS/APRS-LoRa-Plus-Rotor`, app `EA7JRS/APRS-LoRa-Plus-Android`.

## Qué es
Punto 1 del «kit CAN» de Manuel (XIAO ESP32-S3 + XIAO CAN Bus Expansion Board + HAT XIAO-LOG): pasarela APRS LoRa+ → NMEA 2000 para ver las estaciones APRS en el plotter Garmin GPSMAP. Los otros usos del kit (OBD-II, rotor por CAN, bus entre cajas) y el kit relé quedan para después.

## Decisiones
- Recibe del tracker por **KISS TCP** (puerto 8001, `kissNet` del tracker; búsqueda por mDNS `_kiss._tcp` y por los clientes DHCP del punto de acceso propio) o por **KISS BLE** (central NimBLE; servicio `00000001-ba2a-46c9-ae49-01b0961f68bb`, notificaciones en `…0003`). El tracker solo acepta **una** conexión BLE (no vuelve a anunciarse mientras está conectado): con la app conectada por BLE, la pasarela va por WiFi. Solo escucha.
- Blancos AIS con la tabla `NMEA0183::AisTable` y `mmsiFromCallsign` **copiados del tracker** (`nmea0183.*`, `ais_vessel.*`, en `lib/GateCore/src`; aquí 48 estaciones y `find`/`forget` añadidos): el mismo MMSI ficticio `8 MID nnnnn` que la salida NMEA 0183 del tracker; con la etiqueta `AIS:` del perfil náutico, MMSI real, nombre, dimensiones y clase. Clase B: 129039 + 129809 + 129810; clase A: 129038 + 129794. Estáticos antes que la primera posición; posición nueva = envío inmediato; repetición `refreshS` 30; `ttlMin` 30; `staticMin` 6. Objeto borrado = se olvida (el plotter lo da por perdido solo).
- Ambiente del XIAO-LOG: 130311 + 130312 + 130313 cada `envS` (5 s). Del bus: 129025 / 129029 (posición del barco, filtro `maxKm`) y 126992 / 129029 (hora → reloj PCF8563 cada hora).
- **Anemómetro de cazoletas** (2026-11-09; modelo sin conocer): reed en D3 (GPIO4) a GND, `anemo.*` (`on` false, `kmhPerHz` 2,4, `ref` apparent/true); `gc_anemo.h` (copia de `rc_anemo.h` del rotor) + `src/anemo.*`; PGN 130306 cada segundo (sin dirección); `/api/status` `anemo` con Hz para calibrar.
- Librerías: NMEA2000 + NMEA2000_mcp (ttlappalainen, MIT) y CAN_BUS_Shield (fork de ttlappalainen, MIT), fijadas por commit en `platformio.ini`; NimBLE 2.5.2; ArduinoJson 7.4.3; espressif32 8.0.0, placa `seeed_xiao_esp32s3`, `default_8MB.csv`.
- Equipo NMEA 2000: clase 60 (navegación), función 195 (AIS), fabricante 2046 (sin registrar), código de producto 5305, número único de la MAC. Dirección preferida 34, la negociada se guarda en NVS (`n2k/addr`).
- **XIAO-LOG con el kit CAN**: el HAT alimenta sus sensores por D10, que es el MOSI del MCP2515 → arreglo de hardware (D10 del HAT sin unir al XIAO y llevado a 3V3), en `docs/HARDWARE.md`.
- MCP2515: CS D7 (GPIO44), SPI D8/D9/D10 (GPIO7/8/9, los del variante), cristal 16 MHz por defecto (`n2k.mhz`; **comprobar en la placa**), sin interrupción (consulta en el bucle).

## Estructura
- `lib/GateCore/src/`: `gc_kiss_ax25.h`, `gc_aprs.h` (posiciones APRS: sin comprimir, comprimidas, Mic-E, objetos, ítems, terceros), `gc_gate.h` (motor: tramas → tabla → eventos), `nmea0183.*`, `ais_vessel.*`. Pruebas: `test_host` (`make run`).
- `src/`: `config` (LittleFS `/gw_conf.json`), `tracker_link` (tarea propia, búfer de 8 KB al bucle), `n2k`, `loghat`, `net` (WiFi AP «APRS-N2K» / `aprsn2000`, cliente con rescate a los 30 s, WebUI, `/api/status`, `/api/config`, `/update`; clave `pin` = HTTP Basic `admin`), `web_page.h`, `main.cpp` (USB `APRS-INFO?` → `device=n2k`).
- CI: `.github/workflows/build.yml` (pruebas + compilación + `factory.bin` con esptool 4.x).

## Estado (2026-11-09)
- Núcleo probado en el PC (50 comprobaciones; la etiqueta AIS legible de la 5.4.4 y la compacta anterior). Firmware: **compila en la CI** (primera subida 2026-11-09; las cabeceras de NMEA2000_mcp van sin `-Wvolatile` en `n2k.cpp`); **sin probar** con el kit, un tracker ni un plotter.
- Falta: comprobar con un Garmin GPSMAP que acepta los MMSI 8xxxxxxxx y cómo los muestra; mensajes APRS al plotter (¿129802?); lanzador en el instalador web del tracker; sección en la app (regla del ecosistema: todo controlable desde la app).

## Cómo trabajar
- No publicar releases ni tocar otros repositorios sin que lo pida Manuel.
- Antes de subir: `make -C test_host run`; la CI compila el firmware.
