/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * APRS LoRa+ NMEA 2000: pasarela del tracker APRS LoRa+ al bus NMEA 2000 del barco (Garmin GPSMAP y otros plotters).
 * Las estaciones APRS que oye el tracker salen en el plotter como blancos AIS; la temperatura y la humedad del HAT
 * XIAO-LOG, como datos de ambiente. Placa: Seeed XIAO ESP32-S3 + XIAO CAN Bus Expansion Board (MCP2515) + XIAO-LOG.
 *
 * This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public
 * License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later
 * version. See LICENSE.
 */

#include <Arduino.h>
#include <sys/time.h>
#include "config.h"
#include "gc_gate.h"
#include "loghat.h"
#include "anemo.h"
#include "n2k.h"
#include "net.h"
#include "tracker_link.h"

static GC::Gate gate;
static String usbLine;
static uint32_t lastRtcSync = 0;

static void applyGateOptions() {
    GC::GateOptions o;
    o.mid = (uint16_t)Cfg.mid;
    o.refreshMs = (uint32_t)Cfg.refreshS * 1000;
    o.ttlMs = (uint32_t)Cfg.ttlMin * 60000;
    o.staticMs = (uint32_t)Cfg.staticMin * 60000;
    o.maxKm = Cfg.maxKm;
    o.objects = Cfg.objects;
    o.ignore = Cfg.ignore;
    gate.configure(o);
}

// Hora: la del bus (plotter) manda; si no hay, la del reloj del HAT. La del bus se guarda en el reloj cada hora.
static void syncClock() {
    const N2k::Status& n = N2k::status();
    const uint32_t now = millis();
    if (n.hasTime && (lastRtcSync == 0 || now - lastRtcSync > 3600000UL)) {
        timeval tv{(time_t)(n.time + (now - n.timeAt) / 1000), 0};
        settimeofday(&tv, nullptr);
        struct tm t;
        gmtime_r(&tv.tv_sec, &t);
        LogHat::rtcSet(t);
        lastRtcSync = now;
    }
}

// Instalador web: la placa contesta a "APRS-INFO?" por el USB
static void usbLoop() {
    while (Serial.available()) {
        const char c = (char)Serial.read();
        if (c == '\n' || c == '\r') {
            if (usbLine == "APRS-INFO?")
                Serial.printf("APRS-LoRa-Plus: device=n2k board=%s version=%s chip=esp32s3 ota=1\n", GW_BOARD, GW_VERSION);
            usbLine = "";
        } else if (usbLine.length() < 32) usbLine += c;
    }
}

void setup() {
    Serial.begin(115200);
    delay(300);
    Serial.printf("\nAPRS LoRa+ NMEA 2000 %s - (C) 2026 Manuel - EA7JRS\n", GW_VERSION);
    Config::begin();
    applyGateOptions();
    LogHat::begin();
    Anemo::begin();
    struct tm t;
    if (LogHat::rtcGet(t)) {                                    // hora del reloj hasta que llegue la del bus
        timeval tv{mktime(&t), 0};
        settimeofday(&tv, nullptr);
    }
    N2k::begin();
    Net::begin(gate);
    TrackerLink::begin();
}

void loop() {
    uint8_t buf[512];
    size_t n;
    while ((n = TrackerLink::read(buf, sizeof buf)) > 0) gate.onKissBytes(buf, n, millis());
    N2k::loop(gate);
    LogHat::loop();
    Anemo::loop();
    Net::loop();
    syncClock();
    usbLoop();
    delay(1);
}
