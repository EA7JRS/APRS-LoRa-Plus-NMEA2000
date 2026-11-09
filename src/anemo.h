/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#pragma once
#include <Arduino.h>
#include "gc_anemo.h"

// Anemómetro de cazoletas con ampolla reed entre un pin (D3 por defecto) y masa: la interrupción cuenta los cierres
// (rebotes de 5 ms fuera) y GC::Anemometer los convierte en viento, viento medio y racha.
namespace Anemo {
    void                   begin();
    void                   loop();
    bool                   active();          // configurado y con al menos 3 s de datos
    const GC::Anemometer&  meter();
    uint32_t               pulses();
}
