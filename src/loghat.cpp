/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#include "loghat.h"
#include "config.h"
#include <Wire.h>
#include <time.h>

namespace LogHat {

    static const uint8_t SHT40 = 0x44, BH1750 = 0x23, PCF8563 = 0x51;
    static Data d;
    static bool started = false;
    static uint32_t lastRead = 0, shtAsked = 0;
    static bool shtPending = false;

    static bool present(uint8_t a) { Wire.beginTransmission(a); return Wire.endTransmission() == 0; }

    static uint8_t crc8(const uint8_t* p, int n) {
        uint8_t c = 0xFF;
        for (int i = 0; i < n; i++) {
            c ^= p[i];
            for (int b = 0; b < 8; b++) c = (c & 0x80) ? (uint8_t)((c << 1) ^ 0x31) : (uint8_t)(c << 1);
        }
        return c;
    }

    static uint8_t bcd(uint8_t v) { return (uint8_t)((v / 10) << 4 | (v % 10)); }
    static uint8_t unbcd(uint8_t v) { return (uint8_t)((v >> 4) * 10 + (v & 0x0F)); }

    void begin() {
        if (!Cfg.logHat) return;
        Wire.begin(Cfg.sda, Cfg.scl);
        Wire.setClock(100000);
        d.sht = present(SHT40);
        d.light = present(BH1750);
        d.rtc = present(PCF8563);
        if (d.light) {                                          // encendido y medida continua de alta resolución (1 lx, 120 ms)
            Wire.beginTransmission(BH1750); Wire.write(0x01); Wire.endTransmission();
            Wire.beginTransmission(BH1750); Wire.write(0x10); Wire.endTransmission();
        }
        if (Cfg.battery) analogReadResolution(12);
        started = true;
        Serial.printf("log: SHT40 %s, BH1750 %s, PCF8563 %s\n", d.sht ? "si" : "no", d.light ? "si" : "no", d.rtc ? "si" : "no");
    }

    void loop() {
        if (!started) return;
        const uint32_t now = millis();
        if (shtPending && now - shtAsked >= 10) {               // medida de alta precisión: 8,3 ms
            shtPending = false;
            uint8_t b[6];
            if (Wire.requestFrom(SHT40, (uint8_t)6) == 6) {
                for (int i = 0; i < 6; i++) b[i] = Wire.read();
                if (crc8(b, 2) == b[2] && crc8(b + 3, 2) == b[5]) {
                    const uint16_t t = (uint16_t)(b[0] << 8 | b[1]), h = (uint16_t)(b[3] << 8 | b[4]);
                    d.tempC = -45.0f + 175.0f * t / 65535.0f;
                    float rh = -6.0f + 125.0f * h / 65535.0f;
                    d.humPct = rh < 0 ? 0 : (rh > 100 ? 100 : rh);
                    d.at = now;
                }
            }
        }
        if (now - lastRead < 2000) return;
        lastRead = now;
        if (d.sht) {
            Wire.beginTransmission(SHT40); Wire.write(0xFD);
            if (Wire.endTransmission() == 0) { shtPending = true; shtAsked = now; }
        }
        if (d.light && Wire.requestFrom(BH1750, (uint8_t)2) == 2) {
            const uint16_t v = (uint16_t)(Wire.read() << 8);
            d.lux = (v | Wire.read()) / 1.2f;
        }
        if (Cfg.battery && Cfg.batPin >= 0) d.batV = analogReadMilliVolts(Cfg.batPin) * 2.0f / 1000.0f;
    }

    const Data& data() { return d; }

    bool rtcGet(struct tm& t) {
        if (!d.rtc) return false;
        Wire.beginTransmission(PCF8563); Wire.write(0x02);
        if (Wire.endTransmission(false) != 0 || Wire.requestFrom(PCF8563, (uint8_t)7) != 7) return false;
        uint8_t r[7];
        for (int i = 0; i < 7; i++) r[i] = Wire.read();
        if (r[0] & 0x80) return false;                          // VL: el reloj perdió la hora
        memset(&t, 0, sizeof t);
        t.tm_sec = unbcd(r[0] & 0x7F); t.tm_min = unbcd(r[1] & 0x7F); t.tm_hour = unbcd(r[2] & 0x3F);
        t.tm_mday = unbcd(r[3] & 0x3F); t.tm_mon = unbcd(r[5] & 0x1F) - 1; t.tm_year = unbcd(r[6]) + 100;
        return t.tm_year >= 124;
    }

    bool rtcSet(const struct tm& t) {
        if (!d.rtc) return false;
        Wire.beginTransmission(PCF8563);
        Wire.write(0x02);
        Wire.write(bcd((uint8_t)t.tm_sec)); Wire.write(bcd((uint8_t)t.tm_min)); Wire.write(bcd((uint8_t)t.tm_hour));
        Wire.write(bcd((uint8_t)t.tm_mday)); Wire.write((uint8_t)t.tm_wday); Wire.write(bcd((uint8_t)(t.tm_mon + 1)));
        Wire.write(bcd((uint8_t)(t.tm_year % 100)));
        return Wire.endTransmission() == 0;
    }
}
