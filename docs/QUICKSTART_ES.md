# APRS LoRa+ NMEA 2000 — Guía rápida

1. **Monta**: XIAO ESP32-S3 sobre el kit CAN y, si lo usas, el XIAO-LOG encima **con su pin D10 sin unir al XIAO y llevado a 3V3** ([HARDWARE.md](HARDWARE.md)).
2. **Instala** el firmware: `pio run -e xiao_esp32s3_can -t upload`, o `factory.bin` en `0x0`.
3. **Bus NMEA 2000**: CAN-H al blanco (pin 4), CAN-L al azul (pin 5), GND al negro (pin 3); el rojo (12 V) a un convertidor a 5 V para el XIAO. **Sin** cerrar el puente P1 (terminación).
4. **WiFi**: conéctate a **APRS-N2K** (clave `aprsn2000`) y abre `http://192.168.4.1`.
5. **Tracker**: en su WebUI, **KISS WiFi** activado y su WiFi como cliente de **APRS-N2K** (o los dos en la red del barco). La pasarela lo encuentra sola. Por Bluetooth: *Recibir del tracker = Bluetooth*, con la app del móvil desconectada del tracker por BLE.
6. **Plotter**: capa AIS encendida. Las estaciones APRS salen como blancos AIS con su nombre.
7. **Anemómetro** (opcional): ampolla reed entre D3 y GND; *Anemómetro (D3) = Sí* en la configuración. Calibra con *km/h por impulso por segundo* (2,4 por defecto) mirando los Hz en *Ambiente*.
8. **Alarmas de colisión**: pon una **distancia máxima** (`ais.maxKm`) o los indicativos que no quieras ver en **No enviar**.

> No son blancos AIS reales: no sirven para evitar colisiones.

Manual completo: [MANUAL_ES.md](MANUAL_ES.md).
