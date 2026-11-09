/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#pragma once
#include <Arduino.h>
#include "gc_gate.h"

// Bus NMEA 2000 (250 kbit/s) por el MCP2515 del kit CAN, con la librería NMEA2000 de Timo Lappalainen.
// Manda los blancos AIS (clase B: 129039 + 129809 + 129810; clase A: 129038 + 129794) y la temperatura y la humedad del
// HAT (130311, 130312, 130313). Lee del bus la posición del barco (129025 / 129029) y la hora (126992).
namespace N2k {
    struct Status {
        bool     open = false;
        uint8_t  address = 0;
        uint32_t sent = 0, errors = 0, received = 0;
        bool     hasOwn = false;
        double   ownLat = 0, ownLon = 0;
        uint32_t ownAt = 0;
        bool     hasTime = false;
        time_t   time = 0;                     // UTC del bus (126992 o 129029)
        uint32_t timeAt = 0;
        int      devices = 0;                  // otros equipos oídos en el bus
    };

    void          begin();
    void          loop(GC::Gate& gate);
    const Status& status();
}
