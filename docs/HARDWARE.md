# Hardware

Copyright (C) 2026 Manuel - EA7JRS. GPL-3.0-or-later.

## Material

| Pieza | Para qué |
|---|---|
| **Seeed Studio XIAO ESP32-S3** | WiFi, BLE 5, 8 MB de flash, 8 MB de PSRAM |
| **XIAO CAN Bus Expansion Board** | MCP2515 (controlador CAN por SPI) + SN65HVD230 (transceptor de 3,3 V), borna CAN-H / CAN-L / GND |
| **XIAO-LOG** (opcional) | SHT40 (temperatura y humedad), BH1750 (luz), PCF8563 (reloj con su pila), medida de la batería |
| Convertidor de 12 V a 5 V (mínimo 500 mA) | alimentar la pasarela desde el propio bus NMEA 2000 |
| Cable de derivación NMEA 2000 (Micro-C de 5 pines) con un extremo libre, o un conector Micro-C para montar | llevar el bus a la borna del kit CAN |

## Pines del XIAO ESP32-S3

| Pin | GPIO | Kit CAN | XIAO-LOG |
|---|---|---|---|
| D0 | 1 | — | aviso del reloj (PCF8563 INT) |
| D1 | 2 | — | batería (divisor 100 k / 100 k) |
| D2, D3 | 3, 4 | — | — (libres) |
| D4 / D5 | 5 / 6 | — | I2C SDA / SCL |
| D6 | 43 | — | — |
| D7 | 44 | **CS del MCP2515** | — |
| D8 / D9 / D10 | 7 / 8 / 9 | **SPI** SCK / MISO / MOSI | D10 = alimentación de los sensores ⚠️ |

## ⚠️ Arreglo del XIAO-LOG para el kit CAN

El XIAO-LOG alimenta el SHT40, el BH1750 y el divisor de la batería por **D10**, para dormir sin consumo. En el kit CAN, D10 es el **MOSI** del SPI del MCP2515. Si no se toca nada, cada mensaje que se manda al bus enciende y apaga los sensores, y los datos del SPI salen deformados.

**Arreglo:** el pin D10 del XIAO-LOG **no** se une al XIAO, y esa pista del HAT se lleva a **3V3**:

1. Antes de soldar el HAT, deja sin soldar su almena **D10/MOSI**. Si ya está soldada, desuéldala o corta la pista justo al lado de la almena.
2. Une con un hilo fino esa almena del HAT con la almena **3V3** del HAT.
3. Comprueba con el polímetro que el D10 del XIAO **no** tiene continuidad con el 3V3.

Así los sensores quedan siempre alimentados y la batería siempre medible (el divisor gasta unos 17 µA). Con alimentación fija desde el barco no importa. En la WebUI, *Ambiente* dice si encontró el SHT40, el BH1750 y el reloj.

## Bus NMEA 2000

El conector Micro-C de NMEA 2000 (5 pines):

| Pin | Señal | Color habitual | A |
|---|---|---|---|
| 1 | Malla | sin funda | sin conectar (o a GND solo en un punto) |
| 2 | NET-S (+12 V) | rojo | entrada del convertidor de 12 V a 5 V |
| 3 | NET-C (0 V) | negro | GND del convertidor y **GND** de la borna CAN |
| 4 | NET-H (CAN H) | blanco | **CAN-H** |
| 5 | NET-L (CAN L) | azul | **CAN-L** |

- **No** cierres el puente P1 del kit CAN (resistencia de 120 Ω). El bus del barco ya tiene sus dos terminadores en los extremos; un tercero estropea la señal. Solo para probar en la mesa con un único equipo y sin bus.
- Salida de 5 V del convertidor al pin **5V** del XIAO (o por el USB-C). No conectes el USB del PC a la vez que el convertidor si este no tiene diodo.
- Consumo: unos 100-150 mA a 5 V, es decir, unos 70 mA a 12 V tras el convertidor: cuenta **2 LEN** (1 LEN = 50 mA) al calcular el bus.
- Velocidad: 250 kbit/s, fija.
- **Cristal del MCP2515**: mira lo que pone el cristal plateado del kit CAN (16.000 u 8.000). Si no coincide con `n2k.mhz` (16 por defecto), el bus no funciona.

## Montaje

1. XIAO ESP32-S3 sobre el kit CAN (en las tiras de pines del kit).
2. XIAO-LOG encima del XIAO, con el arreglo de D10.
3. Antena WiFi/BLE del XIAO en su conector U.FL: sin ella el alcance es muy corto.
4. Caja estanca o en un sitio seco: el kit no está protegido contra la humedad del barco.
