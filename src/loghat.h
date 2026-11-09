/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#pragma once
#include <Arduino.h>

// HAT XIAO-LOG: SHT40 (0x44, temperatura y humedad), BH1750 (0x23, luz), PCF8563 (0x51, reloj) y la batería por D1.
// Con el kit CAN los sensores tienen que ir alimentados a 3V3 (el HAT los alimenta por D10, que es el MOSI del CAN):
// ver docs/HARDWARE.md.
namespace LogHat {
    struct Data {
        bool  sht = false, light = false, rtc = false;
        float tempC = NAN, humPct = NAN, lux = NAN, batV = NAN;
        uint32_t at = 0;                     // millis() de la última lectura buena
    };

    void        begin();
    void        loop();                      // lee cada 2 s sin bloquear
    const Data& data();
    // Reloj: hora UTC guardada (false si no hay reloj o no tiene hora válida)
    bool        rtcGet(struct tm& t);
    bool        rtcSet(const struct tm& t);
}
