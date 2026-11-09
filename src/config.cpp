/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#include "config.h"
#include <LittleFS.h>

GwConfig Cfg;

namespace Config {

    static const char* PATH = "/gw_conf.json";
    static const char* MASK = "********";

    template <typename T> static void get(JsonObjectConst o, const char* k, T& v) { if (o[k].is<T>()) v = o[k].as<T>(); }
    static void getS(JsonObjectConst o, const char* k, std::string& v) { if (o[k].is<const char*>()) v = o[k].as<const char*>(); }
    static void getSecret(JsonObjectConst o, const char* k, std::string& v) {
        if (o[k].is<const char*>() && strcmp(o[k].as<const char*>(), MASK) != 0) v = o[k].as<const char*>();
    }

    void toJson(JsonObject o, bool secrets) {
        const GwConfig& c = Cfg;
        o["name"] = c.name; o["pin"] = secrets ? c.pin : (c.pin.empty() ? "" : MASK);
        JsonObject w = o["wifi"].to<JsonObject>();
        w["mode"] = c.wifiMode; w["ssid"] = c.ssid; w["pass"] = secrets ? c.pass : MASK; w["apPass"] = secrets ? c.apPass : MASK;
        JsonObject t = o["tracker"].to<JsonObject>();
        t["src"] = c.src; t["bleName"] = c.bleName; t["bleAddr"] = c.bleAddr; t["tcpHost"] = c.tcpHost; t["tcpPort"] = c.tcpPort;
        JsonObject a = o["ais"].to<JsonObject>();
        a["mid"] = c.mid; a["refreshS"] = c.refreshS; a["ttlMin"] = c.ttlMin; a["staticMin"] = c.staticMin; a["maxKm"] = c.maxKm;
        a["objects"] = c.objects; a["ignore"] = c.ignore;
        JsonObject n = o["n2k"].to<JsonObject>();
        n["cs"] = c.canCs; n["mhz"] = c.canMHz; n["int"] = c.canInt; n["addr"] = c.n2kAddr; n["env"] = c.sendEnv; n["envS"] = c.envS;
        JsonObject l = o["log"].to<JsonObject>();
        l["on"] = c.logHat; l["sda"] = c.sda; l["scl"] = c.scl; l["battery"] = c.battery; l["batPin"] = c.batPin;
    }

    std::string apply(JsonObjectConst in, GwConfig& n) {
        getS(in, "name", n.name); getSecret(in, "pin", n.pin);
        JsonObjectConst w = in["wifi"];
        get(w, "mode", n.wifiMode); getS(w, "ssid", n.ssid); getSecret(w, "pass", n.pass); getSecret(w, "apPass", n.apPass);
        JsonObjectConst t = in["tracker"];
        getS(t, "src", n.src); getS(t, "bleName", n.bleName); getS(t, "bleAddr", n.bleAddr); getS(t, "tcpHost", n.tcpHost); get(t, "tcpPort", n.tcpPort);
        JsonObjectConst a = in["ais"];
        get(a, "mid", n.mid); get(a, "refreshS", n.refreshS); get(a, "ttlMin", n.ttlMin); get(a, "staticMin", n.staticMin);
        if (a["maxKm"].is<double>()) n.maxKm = a["maxKm"].as<double>();
        get(a, "objects", n.objects); getS(a, "ignore", n.ignore);
        JsonObjectConst k = in["n2k"];
        get(k, "cs", n.canCs); get(k, "mhz", n.canMHz); get(k, "int", n.canInt); get(k, "addr", n.n2kAddr); get(k, "env", n.sendEnv); get(k, "envS", n.envS);
        JsonObjectConst l = in["log"];
        get(l, "on", n.logHat); get(l, "sda", n.sda); get(l, "scl", n.scl); get(l, "battery", n.battery); get(l, "batPin", n.batPin);

        if (n.name.empty() || n.name.size() > 24) return "name 1-24";
        if (n.wifiMode < 0 || n.wifiMode > 2) return "wifi.mode 0-2";
        if (n.apPass.size() < 8) return "wifi.apPass >= 8";
        if (n.src != "ble" && n.src != "tcp" && n.src != "both") return "tracker.src ble | tcp | both";
        if (n.tcpPort < 1 || n.tcpPort > 65535) return "tracker.tcpPort";
        if (n.mid < 200 || n.mid > 799) return "ais.mid 200-799";
        if (n.refreshS < 2 || n.refreshS > 600) return "ais.refreshS 2-600";
        if (n.ttlMin < 1 || n.ttlMin > 240) return "ais.ttlMin 1-240";
        if (n.staticMin < 1 || n.staticMin > 60) return "ais.staticMin 1-60";
        if (n.maxKm < 0) return "ais.maxKm";
        if (n.canMHz != 8 && n.canMHz != 16) return "n2k.mhz 8 | 16";
        if (n.canCs < 0 || n.canCs > 48) return "n2k.cs";
        if (n.n2kAddr < 0 || n.n2kAddr > 251) return "n2k.addr 0-251";
        if (n.envS < 1 || n.envS > 60) return "n2k.envS 1-60";
        for (int used : {7, 8, 9, n.sda, n.scl}) if (n.canCs == used) return "n2k.cs (SPI / I2C pin)";
        return "";
    }

    bool save() {
        File f = LittleFS.open(PATH, "w");
        if (!f) return false;
        JsonDocument d;
        toJson(d.to<JsonObject>(), true);
        const bool ok = serializeJson(d, f) > 0;
        f.close();
        return ok;
    }

    void reset() { Cfg = GwConfig(); save(); }

    void begin() {
        if (!LittleFS.begin(true)) { Serial.println("LittleFS: no se puede montar"); return; }
        File f = LittleFS.open(PATH, "r");
        if (!f) { save(); return; }
        JsonDocument d;
        const DeserializationError e = deserializeJson(d, f);
        f.close();
        if (e) { Serial.printf("config: %s, valores por defecto\n", e.c_str()); return; }
        GwConfig n = Cfg;
        const std::string err = apply(d.as<JsonObjectConst>(), n);
        if (err.empty()) Cfg = n;
        else Serial.printf("config: %s, valores por defecto\n", err.c_str());
    }
}
