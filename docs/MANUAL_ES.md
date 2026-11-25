# APRS LoRa+ NMEA 2000 — Manual de usuario

Versión 5.5.0 · Copyright (C) 2026 Manuel - EA7JRS · GPL-3.0-or-later

## 1. Qué es

Una pasarela entre el **tracker APRS LoRa+** y el bus **NMEA 2000** del barco. Las estaciones APRS que oye el tracker (con posición, objetos e ítems incluidos) aparecen en el plotter (Garmin GPSMAP y otros) como **blancos AIS**, con su nombre, su rumbo y su velocidad. La temperatura y la humedad del HAT XIAO-LOG salen como datos de ambiente.

> ⚠️ **No son blancos AIS reales.** Son estaciones de radioaficionado, muchas en tierra. No sirven para evitar colisiones. Mira el apartado 8 (alarmas).

- Cada estación recibe un **MMSI ficticio** `8 MID nnnnn` sacado de su indicativo: el mismo que da el tracker en su salida NMEA 0183, así que el blanco es el mismo en todos los equipos. Con el perfil náutico AIS del tracker (la etiqueta `AIS:` en el comentario) se usa el **MMSI real**, el nombre, la eslora y la manga, y la clase A o B de la embarcación.
- Clase B: PGN 129039 (posición), 129809 y 129810 (nombre, indicativo, tipo). Clase A: 129038 y 129794.
- Ambiente: PGN 130311, 130312 y 130313 (temperatura y humedad exteriores).
- Del bus lee la posición del barco (129025 / 129029) para el filtro de distancia y la hora (126992), que guarda en el reloj del HAT.
- Solo escucha: no transmite por radio ni manda órdenes al tracker.

## 2. Material

XIAO ESP32-S3, XIAO CAN Bus Expansion Board, XIAO-LOG (opcional), convertidor de 12 V a 5 V y un cable de derivación NMEA 2000. Montaje, el **arreglo del pin D10** del XIAO-LOG y la conexión al bus en [HARDWARE.md](HARDWARE.md).

## 3. Instalar el firmware

- **Desde el PC**: `pio run -e xiao_esp32s3_can -t upload` (PlatformIO).
- **Imagen completa**: `factory.bin` de la compilación (artefacto de la CI), en la dirección `0x0` con `esptool.py --chip esp32s3 write_flash 0x0 factory.bin`.
- **Después**: WebUI → *Actualizar firmware* (`/update`, con `firmware.bin`).

Si no entra en modo de carga: mantén pulsado **B** (BOOT) del XIAO, pulsa **R** (RESET) y suelta B.

## 4. Primer arranque

La pasarela crea la red WiFi **APRS-N2K** (clave `aprsn2000`, mínimo 8 caracteres). Conéctate y abre `http://192.168.4.1`.

- **Clave de la WebUI**: en la configuración JSON, `pin`. De fábrica es **`123456`** (el mismo PIN de control del resto del ecosistema APRS LoRa+: tracker, plotter, app). Para guardar cambios o actualizar el firmware piden usuario `admin` y esa clave. Déjala **vacía** para no pedir clave. **Cámbiala** si la pasarela es accesible desde la red del barco.
- **WiFi cliente**: *WiFi = Cliente de otra red*, con la red y la clave del barco. Si en 30 s no entra, vuelve a crear su red (rescate).
- **Hora**: la pasarela no necesita hora precisa para funcionar. La toma del GPS/plotter del bus NMEA 2000 (PGN 126992 o 129029) y la guarda en el reloj RTC del HAT XIAO-LOG. En modo cliente WiFi sincroniza además por **NTP `ntp.roa.es`** (reserva; el GPS del bus tiene prioridad 6 h sobre NTP).

## 5. Conectar el tracker

El tracker admite **una sola conexión Bluetooth**: si la app del móvil está conectada por BLE, la pasarela tiene que ir por **WiFi**.

### Por WiFi (recomendado)

1. En la WebUI del tracker: activa **KISS WiFi** (puerto 8001).
2. Pon el tracker en la misma red: cliente de la red **APRS-N2K** de la pasarela, o los dos en la red del barco.
3. En la pasarela, *Recibir del tracker = WiFi*. Con la dirección vacía, busca sola: primero por mDNS (`_kiss._tcp`, lo anuncia el tracker) y luego entre los equipos conectados a su red. Si el tracker tiene IP fija, ponla.

### Por Bluetooth

1. En el tracker, **BLE KISS** activado (WebUI > Bluetooth).
2. Desconecta la app del móvil del tracker (o conéctala por WiFi).
3. En la pasarela, *Recibir del tracker = Bluetooth*. Vacío = el primer tracker que anuncie el servicio KISS; si hay varios, escribe parte de su nombre.

*Los dos* usa los dos enlaces a la vez (las tramas repetidas no hacen daño).

## 6. En el plotter

- Enciende la capa **AIS** del plotter: los blancos aparecen con el nombre de la estación (o el de la embarcación, con la etiqueta AIS).
- En la lista de equipos NMEA 2000 aparece **APRS LoRa+ NMEA 2000** (función AIS), con la versión y el número de serie.
- La temperatura y la humedad exteriores salen donde el plotter muestre los datos de ambiente.
- Con anemómetro: la velocidad del viento (PGN 130306, sin dirección) en los datos de viento del plotter.
- Un blanco deja de repetirse a los **30 min** sin oírlo (`ais.ttlMin`); el plotter lo borra después, según su propio tiempo de AIS perdido.

## 7. WebUI

- **Tracker**: enlace (WiFi, BLE), bytes y tramas recibidos, posiciones, ignoradas y lejanas.
- **NMEA 2000**: bus abierto (si no, el MCP2515 no contesta: cristal, CS, cableado), dirección en el bus, mensajes enviados y fallos, equipos oídos, posición y hora del barco.
- **Ambiente**: temperatura, humedad, luz, reloj, batería; con anemómetro, viento de 3 s, medio de 2 y 10 min, racha, Beaufort e impulsos por segundo (para calibrar).
- **Blancos AIS**: nombre, MMSI, clase, posición, rumbo y velocidad, distancia al barco y hace cuánto se oyó.
- **Últimos paquetes**: los 20 últimos en texto TNC2.
- **Configuración**: lo más usado en un formulario y el fichero entero en JSON.

## 8. Alarmas de colisión (CPA / TCPA)

Las estaciones APRS son de radioaficionado, muchas en tierra: no son barcos y **no deben disparar la alarma de abordaje** del plotter. Por eso:

- **Estaciones APRS en tierra** (`ais.aton`, por defecto `Ayuda a la navegación`): las estaciones sin perfil «AIS:» salen como **ayuda a la navegación virtual** (PGN 129041). El plotter las muestra como baliza, no como buque, así que **no entran en la alarma de colisión**. Los barcos con perfil «AIS:» (MMSI real) siguen como buque AIS, con alarma normal. Si prefieres verlas todas como buque (clase B), pon `Buque AIS (clase B)`.
- **Distancia máxima** (`ais.maxKm`): solo se envían las estaciones a menos de esa distancia del barco, si el bus da la posición del barco.
- **No enviar** (`ais.ignore`): indicativos separados por comas, por ejemplo el del propio tracker si el barco ya tiene AIS.
- Ajusta en el plotter la distancia y el tiempo de la alarma AIS, o apágala para los blancos "perdidos".

## 9. Configuración

Fichero `/gw_conf.json` (WebUI > Configuración > JSON):

| Clave | Por defecto | |
|---|---|---|
| `name` | `APRS-N2K` | red WiFi propia, mDNS (`APRS-N2K.local`) y Bluetooth |
| `pin` | `123456` | clave de la WebUI (usuario `admin`); vacía = sin clave |
| `wifi.mode` | 2 | 0 apagada, 1 cliente (con rescate), 2 punto de acceso |
| `wifi.ssid`, `wifi.pass` | — | red del barco (modo 1) |
| `wifi.apPass` | `aprsn2000` | clave de la red propia (mínimo 8) |
| `tracker.src` | `tcp` | `tcp`, `ble` o `both` |
| `tracker.tcpHost`, `tracker.tcpPort` | vacío, 8001 | tracker por WiFi (vacío = buscarlo) |
| `tracker.bleName`, `tracker.bleAddr` | vacío | tracker por BLE (parte del nombre o dirección fija) |
| `ais.mid` | 224 | MID del MMSI ficticio (224 = España), el mismo que el tracker |
| `ais.refreshS` | 30 | repetición de la posición de cada blanco |
| `ais.ttlMin` | 30 | minutos sin oírlo para dejar de enviarlo |
| `ais.staticMin` | 6 | repetición del nombre y del tipo |
| `ais.maxKm` | 0 | distancia máxima (0 = todas) |
| `ais.objects` | true | objetos e ítems APRS también |
| `ais.aton` | true | estaciones sin perfil «AIS:» como ayuda a la navegación virtual (129041), sin alarma de abordaje; false = buque clase B |
| `ais.ignore` | vacío | indicativos que no se envían |
| `gps.on` | false | emitir el GPS del tracker al bus (129025/129026/129029) si no hay otra antena GPS |
| `gps.ownCall` | vacío | indicativo-SSID del propio tracker (el barco), p. ej. `EA7JRS-5` |
| `gps.wx` | false | emitir al bus el tiempo de internet de la app (130306 viento, 130314 presión, 130316 temperatura) de la baliza meteo del barco, si no hay sensores a bordo |
| `n2k.cs`, `n2k.mhz`, `n2k.int` | 44, 16, -1 | CS del MCP2515 (D7), cristal (16 u 8 MHz), interrupción (-1 = sin ella) |
| `n2k.addr` | 34 | dirección preferida en el bus (se negocia y se guarda la que queda) |
| `n2k.env`, `n2k.envS` | true, 5 | temperatura y humedad al bus, cada cuántos segundos |
| `log.on`, `log.sda`, `log.scl` | true, 5, 6 | HAT XIAO-LOG por I2C (D4 / D5) |
| `log.battery`, `log.batPin` | false, 2 | medir la batería por D1 |
| `anemo.on`, `anemo.pin`, `anemo.pullup` | false, 4, true | anemómetro de cazoletas en D3 (`docs/HARDWARE.md`) |
| `anemo.kmhPerHz` | 2.4 | km/h por impulso por segundo (calibración) |
| `anemo.ref` | apparent | viento aparente (en el barco) o `true` (en tierra, fijo) |

## 10. Problemas

| Síntoma | Qué mirar |
|---|---|
| *Bus: sin MCP2515* | cristal (`n2k.mhz`), XIAO bien asentado en el kit CAN, CS en D7 |
| Bus abierto pero el plotter no ve nada | CAN-H y CAN-L cruzados, sin alimentación del bus, terminación de más (P1), capa AIS apagada |
| *TCP: no encuentro el tracker* | KISS WiFi activado en el tracker, los dos en la misma red; prueba con la IP del tracker |
| *BLE: no aparece el tracker* | la app del móvil conectada al tracker por BLE (solo admite una conexión), BLE KISS apagado |
| Ambiente vacío | el arreglo de D10 del XIAO-LOG; `log.on` |
| Viento a 0 con viento | `anemo.on`, cable en D3 y GND; los impulsos de la WebUI deben subir al girar |
| Alarmas de colisión | apartado 8 |

## 11. Licencia y créditos

GPL-3.0-or-later. Librerías: NMEA2000 y NMEA2000_mcp (Timo Lappalainen, MIT), CAN_BUS_Shield (Seeed Studio, MIT), NimBLE-Arduino (Apache-2.0), ArduinoJson (MIT). La tabla AIS y el MMSI ficticio son los del tracker APRS LoRa+.
