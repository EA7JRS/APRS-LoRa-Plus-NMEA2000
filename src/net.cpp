/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#include "net.h"
#include "config.h"
#include "n2k.h"
#include "loghat.h"
#include "tracker_link.h"
#include "web_page.h"
#include <WiFi.h>
#include <WebServer.h>
#include <HTTPUpdateServer.h>
#include <ESPmDNS.h>
#include <time.h>

namespace Net {

    static WebServer web(80);
    static HTTPUpdateServer updater;
    static GC::Gate* gate = nullptr;
    static bool ap = false, started = false;
    static uint32_t staSince = 0, rebootAt = 0;
    static const char* USER = "admin";

    std::string ip() { return (ap ? WiFi.softAPIP() : WiFi.localIP()).toString().c_str(); }
    bool apMode() { return ap; }

    // Cambios solo con la clave (si hay): HTTP Basic, usuario "admin"
    static bool allowed() {
        if (Cfg.pin.empty() || web.authenticate(USER, Cfg.pin.c_str())) return true;
        web.requestAuthentication(BASIC_AUTH, Cfg.name.c_str(), "PIN");
        return false;
    }

    static void sendJson(JsonDocument& d, int code = 200) {
        String s;
        serializeJson(d, s);
        web.sendHeader("Cache-Control", "no-store");
        web.send(code, "application/json", s);
    }

    static double r6(double v) { return round(v * 1e6) / 1e6; }

    static void status() {
        JsonDocument d;
        const uint32_t now = millis();
        d["ver"] = GW_VERSION; d["board"] = GW_BOARD; d["name"] = Cfg.name; d["up"] = now / 1000;
        const TrackerLink::Status l = TrackerLink::status();
        JsonObject lk = d["link"].to<JsonObject>();
        lk["src"] = Cfg.src; lk["ble"] = l.ble; lk["tcp"] = l.tcp; lk["state"] = l.state; lk["bytes"] = l.bytes;
        lk["blePeer"] = l.blePeer; lk["tcpPeer"] = l.tcpPeer;
        if (l.lastData) lk["ageS"] = (now - l.lastData) / 1000;
        const auto& gs = gate->stats();
        JsonObject g = d["gate"].to<JsonObject>();
        g["frames"] = gs.frames; g["positions"] = gs.positions; g["ignored"] = gs.ignored; g["far"] = gs.far; g["active"] = gate->active(now);
        JsonArray sa = d["stations"].to<JsonArray>();
        for (int i = 0; i < NMEA0183::AisTable::MAX_STATIONS; i++) {
            const auto& s = gate->table().at(i);
            if (!s.used) continue;
            JsonObject o = sa.add<JsonObject>();
            o["call"] = s.callsign; o["name"] = GC::Gate::aisName(s); o["mmsi"] = s.mmsi;
            o["lat"] = r6(s.lat); o["lon"] = r6(s.lon);
            if (s.cogDeg >= 0) o["cog"] = round(s.cogDeg);
            if (s.sogKnots >= 0) o["sog"] = round(s.sogKnots * 10) / 10;
            o["ageS"] = (now - s.heardAt) / 1000;
            o["cls"] = GC::Gate::classA(s) ? "A" : "B";
            o["live"] = now - s.heardAt <= (uint32_t)Cfg.ttlMin * 60000;
            if (gate->hasOwn()) o["km"] = round(GC::distanceKm(gate->ownLat(), gate->ownLon(), s.lat, s.lon) * 10) / 10;
        }
        const N2k::Status& n = N2k::status();
        JsonObject nk = d["n2k"].to<JsonObject>();
        nk["open"] = n.open; nk["addr"] = n.address; nk["sent"] = n.sent; nk["errors"] = n.errors; nk["rx"] = n.received; nk["devices"] = n.devices;
        if (n.hasOwn) { nk["lat"] = r6(n.ownLat); nk["lon"] = r6(n.ownLon); nk["posAgeS"] = (now - n.ownAt) / 1000; }
        if (n.hasTime) nk["utc"] = (uint32_t)(n.time + (now - n.timeAt) / 1000);
        const LogHat::Data& h = LogHat::data();
        JsonObject lg = d["log"].to<JsonObject>();
        lg["on"] = Cfg.logHat; lg["sht"] = h.sht; lg["light"] = h.light; lg["rtc"] = h.rtc;
        if (!isnan(h.tempC)) lg["t"] = round(h.tempC * 10) / 10;
        if (!isnan(h.humPct)) lg["h"] = round(h.humPct);
        if (!isnan(h.lux)) lg["lux"] = round(h.lux);
        if (!isnan(h.batV)) lg["bat"] = round(h.batV * 100) / 100;
        time_t t = time(nullptr);
        if (t > 1700000000) d["time"] = (uint32_t)t;
        JsonObject w = d["wifi"].to<JsonObject>();
        w["ap"] = ap; w["ip"] = ip(); w["ssid"] = ap ? Cfg.name : Cfg.ssid;
        if (!ap && WiFi.status() == WL_CONNECTED) w["rssi"] = WiFi.RSSI();
        if (WiFi.getMode() & WIFI_MODE_AP) w["clients"] = WiFi.softAPgetStationNum();
        JsonArray rc = d["recent"].to<JsonArray>();
        for (const auto& p : gate->recent()) rc.add(p);
        sendJson(d);
    }

    static void getConfig() {
        JsonDocument d;
        Config::toJson(d.to<JsonObject>(), false);
        sendJson(d);
    }

    static void postConfig() {
        if (!allowed()) return;
        JsonDocument in, out;
        if (deserializeJson(in, web.arg("plain"))) { out["err"] = "json"; sendJson(out, 400); return; }
        GwConfig n = Cfg;
        const std::string err = Config::apply(in.as<JsonObjectConst>(), n);
        if (!err.empty()) { out["err"] = err; sendJson(out, 400); return; }
        Cfg = n;
        if (!Config::save()) { out["err"] = "save"; sendJson(out, 500); return; }
        out["ok"] = true; out["reboot"] = true;
        sendJson(out);
        rebootAt = millis() + 800;
    }

    static void startServices() {
        if (started) return;
        started = true;
        web.on("/", HTTP_GET, [] { web.sendHeader("Cache-Control", "no-store"); web.send_P(200, "text/html", WEB_PAGE); });
        web.on("/api/status", HTTP_GET, status);
        web.on("/api/config", HTTP_GET, getConfig);
        web.on("/api/config", HTTP_POST, postConfig);
        web.on("/api/reboot", HTTP_POST, [] { if (!allowed()) return; web.send(200, "application/json", "{\"ok\":true}"); rebootAt = millis() + 500; });
        web.on("/api/reset", HTTP_POST, [] { if (!allowed()) return; Config::reset(); web.send(200, "application/json", "{\"ok\":true}"); rebootAt = millis() + 500; });
        updater.setup(&web, "/update", Cfg.pin.empty() ? "" : USER, Cfg.pin.c_str());
        web.begin();
        if (MDNS.begin(Cfg.name.c_str())) MDNS.addService("http", "tcp", 80);
        Serial.printf("red: %s %s\n", ap ? "punto de acceso" : "cliente", ip().c_str());
    }

    static void startAp() {
        WiFi.mode(Cfg.wifiMode == 1 ? WIFI_AP_STA : WIFI_AP);
        WiFi.softAP(Cfg.name.c_str(), Cfg.apPass.c_str());
        ap = true;
        startServices();
    }

    void begin(GC::Gate& g) {
        gate = &g;
        WiFi.persistent(false);
        WiFi.setHostname(Cfg.name.c_str());
        if (Cfg.wifiMode == 0) { WiFi.mode(WIFI_OFF); return; }
        if (Cfg.wifiMode == 2 || Cfg.ssid.empty()) { startAp(); return; }
        WiFi.mode(WIFI_STA);
        WiFi.begin(Cfg.ssid.c_str(), Cfg.pass.c_str());
        staSince = millis();
    }

    void loop() {
        if (rebootAt && (int32_t)(millis() - rebootAt) >= 0) ESP.restart();
        if (Cfg.wifiMode == 1 && !ap) {
            if (WiFi.status() == WL_CONNECTED) startServices();
            else if (!started && millis() - staSince > 30000) startAp();          // sin red: punto de acceso de rescate
        }
        if (started) web.handleClient();
    }
}
