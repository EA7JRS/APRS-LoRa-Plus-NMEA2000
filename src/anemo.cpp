/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#include "anemo.h"
#include "config.h"

namespace Anemo {

    static GC::Anemometer a;
    static volatile uint32_t count = 0, lastUs = 0;
    static bool on = false;

    static void IRAM_ATTR isr() {
        const uint32_t t = (uint32_t)micros();
        if (t - lastUs >= 5000) { lastUs = t; count = count + 1; }
    }

    void begin() {
        if (!Cfg.anemoOn || Cfg.anemoPin < 0) return;
        a.kmhPerHz = Cfg.anemoKmhHz;
        pinMode(Cfg.anemoPin, Cfg.anemoPullup ? INPUT_PULLUP : INPUT);
        attachInterrupt(digitalPinToInterrupt(Cfg.anemoPin), isr, FALLING);
        on = true;
        Serial.printf("anemómetro: GPIO %d, %.2f km/h por Hz\n", Cfg.anemoPin, Cfg.anemoKmhHz);
    }

    void loop() { if (on) a.sample(count, millis()); }
    bool active() { return on && a.have(); }
    const GC::Anemometer& meter() { return a; }
    uint32_t pulses() { return count; }
}
