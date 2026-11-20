// Copyright (C) 2026 Manuel - EA7JRS - GPL-3.0-or-later
// Pruebas del núcleo de APRS LoRa+ NMEA 2000 en el PC: make run
#include <stdio.h>
#include <math.h>
#include "gc_gate.h"
#include "gc_anemo.h"

using namespace GC;
static int checks = 0, fails = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FALLO %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
static bool near(double a, double b, double e = 1e-4) { return fabs(a - b) <= e; }

// AX.25 UI + KISS a partir de las partes (como lo manda el tracker)
static void ax25Addr(std::vector<uint8_t>& o, const std::string& call, bool last, bool h = false) {
    std::string c = call; int ssid = 0;
    const size_t d = c.find('-');
    if (d != std::string::npos) { ssid = atoi(c.c_str() + d + 1); c = c.substr(0, d); }
    for (int i = 0; i < 6; i++) o.push_back((uint8_t)((i < (int)c.size() ? c[i] : ' ') << 1));
    o.push_back((uint8_t)(0x60 | (ssid << 1) | (last ? 1 : 0) | (h ? 0x80 : 0)));
}
static std::vector<uint8_t> ax25(const std::string& src, const std::string& dst, const std::vector<std::string>& path, const std::string& info) {
    std::vector<uint8_t> o;
    ax25Addr(o, dst, false);
    ax25Addr(o, src, path.empty());
    for (size_t i = 0; i < path.size(); i++) ax25Addr(o, path[i], i + 1 == path.size());
    o.push_back(0x03); o.push_back(0xF0);
    o.insert(o.end(), info.begin(), info.end());
    return o;
}
static std::vector<uint8_t> kiss(const std::vector<uint8_t>& f) {
    std::vector<uint8_t> k{0xC0, 0x00};
    for (uint8_t b : f) {
        if (b == 0xC0) { k.push_back(0xDB); k.push_back(0xDC); }
        else if (b == 0xDB) { k.push_back(0xDB); k.push_back(0xDD); }
        else k.push_back(b);
    }
    k.push_back(0xC0);
    return k;
}

static void testKissAx25() {
    const auto f = ax25("EA7JRS-9", "APLRT1", {"WIDE1-1"}, std::string("!3725.00N/00559.00W>\xC0\xDB"));
    const auto k = kiss(f);
    KissReader r; std::vector<uint8_t> out; int n = 0;
    // en trozos de 5 bytes, como un MTU pequeño
    for (size_t i = 0; i < k.size(); i++) if (r.push(k[i], out)) n++;
    CHECK(n == 1 && out == f);
    Ax25 a;
    CHECK(ax25Decode(out.data(), out.size(), a) && a.src == "EA7JRS-9" && a.dst == "APLRT1" && a.path.size() == 1 && a.path[0] == "WIDE1-1");
    CHECK(a.info == std::string("!3725.00N/00559.00W>\xC0\xDB"));
    CHECK(tnc2(a).rfind("EA7JRS-9>APLRT1,WIDE1-1:!3725", 0) == 0);
    // orden que no es de datos (puerto 0, tipo 1) -> se ignora
    std::vector<uint8_t> k2{0xC0, 0x01, 0x10, 0xC0};
    n = 0; for (uint8_t b : k2) if (r.push(b, out)) n++;
    CHECK(n == 0);
    Ax25 t;
    CHECK(tnc2Parse("EB3XX>APRS,TCPIP,EA7JRS*:!3725.00N/00559.00W-", t) && t.src == "EB3XX" && t.dst == "APRS" && t.path.size() == 2 && t.info[0] == '!');
}

static void testAprs() {
    AprsPos p;
    CHECK(aprsPosition("N0CALL", "APRS", "!4903.50N/07201.75W-Test /A=001234", p));
    CHECK(near(p.lat, 49.058333) && near(p.lon, -72.029167) && p.symCode == '-' && p.cogDeg < 0 && near(p.altM, 1234 * 0.3048));
    CHECK(aprsPosition("N0CALL", "APRS", "=4903.50N/07201.75W>088/036", p) && p.cogDeg == 88 && p.sogKnots == 36);
    CHECK(aprsPosition("N0CALL", "APRS", "@092345z4903.50S\\07201.75E>", p) && near(p.lat, -49.058333) && near(p.lon, 72.029167) && p.symTable == '\\');
    CHECK(aprsPosition("N0CALL", "APRS", "!49  .  N/072  .  W-", p) && near(p.lat, 49.0) && near(p.lon, -72.0));      // ambigüedad
    CHECK(!aprsPosition("N0CALL", "APRS", "!4903.50X/07201.75W-", p));
    CHECK(aprsPosition("WX", "APRS", "!4903.50N/07201.75W_220/004g005t077", p) && p.cogDeg < 0);                   // viento, no rumbo
    CHECK(p.wx.valid && p.wx.windDir == 220 && near(p.wx.windMs, 4 * 0.44704) && near(p.wx.gustMs, 5 * 0.44704) && near(p.wx.tempC, (77 - 32) * 5.0 / 9.0));
    CHECK(aprsPosition("WX", "APRS", "!4903.50N/07201.75W_180/010g015t068r000p000P000h55b10132", p) && near(p.wx.pressPa, 101320) && p.wx.windDir == 180);
    // Comprimidas (ejemplos de la especificación)
    CHECK(aprsPosition("N0CALL", "APRS", "=/5L!!<*e7>7P[", p) && p.compressed && near(p.lat, 49.5, 1e-4) && near(p.lon, -72.75, 1e-4));
    CHECK(p.cogDeg == 88 && near(p.sogKnots, 36.2, 0.05) && p.symCode == '>');
    CHECK(aprsPosition("N0CALL", "APRS", "!/5L!!<*e7OS]S", p) && near(p.altM, 10004 * 0.3048, 1.0) && p.cogDeg < 0);
    // Mic-E: 33 25.64 N, 72 45.30 W, 36 nudos, 88 grados
    std::string mi = "`";
    mi += (char)(72 + 28); mi += (char)(45 + 28); mi += (char)(30 + 28);
    mi += (char)(3 + 28); mi += (char)(60 + 28); mi += (char)(88 + 28);
    mi += ">/";
    CHECK(aprsPosition("EA7JRS-7", "S32U6T", mi + "Hola", p) && p.micE && near(p.lat, 33 + 25.64 / 60) && near(p.lon, -(72 + 45.30 / 60)));
    CHECK(p.sogKnots == 36 && p.cogDeg == 88 && p.symCode == '>' && p.symTable == '/' && p.comment == "Hola");
    // Mic-E con 5 grados de longitud (desplazamiento de 100) y altitud "xxx}"
    std::string m2 = "'"; m2 += (char)(5 + 118); m2 += (char)(30 + 28); m2 += (char)(0 + 28);
    m2 += (char)(0 + 28); m2 += (char)(0 + 28); m2 += (char)(0 + 28); m2 += "k/";
    const long alt = 10000 + 120;             // 120 m
    m2 += (char)(33 + alt / 8281); m2 += (char)(33 + (alt / 91) % 91); m2 += (char)(33 + alt % 91); m2 += "}x";
    CHECK(aprsPosition("EA7JRS-9", "S32UPT", m2, p) && near(p.lon, -(5 + 30 / 60.0)) && near(p.altM, 120) && p.comment == "x" && p.sogKnots == 0);
    // Objetos e ítems
    CHECK(aprsPosition("EA7JRS", "APRS", ";LEADER   *092345z4903.50N/07201.75W>088/036", p) && p.object && !p.killed && p.name == "LEADER" && p.cogDeg == 88);
    CHECK(aprsPosition("EA7JRS", "APRS", ";LEADER   _092345z4903.50N/07201.75W>", p) && p.killed && p.name == "LEADER");
    CHECK(aprsPosition("EA7JRS", "APRS", ")AID #2!4903.50N/07201.75WA", p) && p.object && p.name == "AID #2" && p.symCode == 'A');
    CHECK(!aprsPosition("EA7JRS", "APRS", ":EA7JRS   :hola{1", p));
    CHECK(aprsPosition("OLD", "APRS", "TEST!4903.50N/07201.75W-", p) && near(p.lat, 49.058333));
}

static void testGate() {
    Gate g;
    GateOptions o; o.ignore = "EA7JRS-5, ec7zzz";
    g.configure(o);
    uint32_t t = 1000;
    auto send = [&](const std::string& src, const std::string& info) { const auto k = kiss(ax25(src, "APLRT1", {}, info)); g.onKissBytes(k.data(), k.size(), t); };
    send("EB7ABC-9", "!3725.00N/00559.00W>090/010");
    GateEvent e;
    CHECK(g.poll(t, e) && e.kind == GateEvent::STATIC && e.index >= 0);
    const auto& s = g.table().at(e.index);
    CHECK(std::string(s.callsign) == "EB7ABC-9" && s.mmsi == NMEA0183::mmsiFromCallsign("EB7ABC-9", 224, 0) && s.mmsi / 100000 == 8224);
    CHECK(Gate::aisName(s) == "EB7ABC-9" && !Gate::classA(s));
    CHECK(g.poll(t, e) && e.kind == GateEvent::POSITION && near(g.table().at(e.index).cogDeg, 90) && near(g.table().at(e.index).sogKnots, 10));
    CHECK(!g.poll(t, e));
    t += 29000; CHECK(!g.poll(t, e));
    t += 2000;  CHECK(g.poll(t, e) && e.kind == GateEvent::POSITION);        // repetición a los 30 s
    CHECK(!g.poll(t, e));
    send("EB7ABC-9", "!3725.10N/00559.00W>090/010");                             // nueva posición: sale enseguida
    CHECK(g.poll(t, e) && e.kind == GateEvent::POSITION && near(g.table().at(e.index).lat, 37 + 25.10 / 60));
    // ignorados (sin distinguir mayúsculas), la etiqueta AIS del perfil náutico
    send("EC7ZZZ", "!3725.00N/00559.00W>");
    send("EA7JRS-5", "!3725.00N/00559.00W>");
    CHECK(g.stats().ignored == 2 && !g.poll(t, e));
    send("EA1BOAT-5", "!3726.00N/00600.00Ws AIS:C224123456,12x4x1.8,37,SEA BREEZE;");
    CHECK(g.poll(t, e) && e.kind == GateEvent::STATIC && g.table().at(e.index).mmsi == 224123456 && Gate::aisName(g.table().at(e.index)) == "SEA BREEZE");
    CHECK(g.poll(t, e) && e.kind == GateEvent::POSITION);
    send("EA2SAIL-5", "!3727.00N/00601.00WY AIS B-CS: AZUL, MMSI 224654321, call EBXY, 10x3 m, draft 1.5 m, sailing;");   // etiqueta legible con distintivo maritimo
    CHECK(g.poll(t, e) && e.kind == GateEvent::STATIC && g.table().at(e.index).mmsi == 224654321 && Gate::aisName(g.table().at(e.index)) == "AZUL");
    CHECK(std::string(g.table().at(e.index).info.call) == "EBXY");
    CHECK(g.poll(t, e) && e.kind == GateEvent::POSITION);
    // objeto borrado
    send("EA7JRS", ";BUOY1    *092345z3720.00N/00610.00WB");
    CHECK(g.poll(t, e) && e.kind == GateEvent::STATIC); CHECK(g.poll(t, e) && e.kind == GateEvent::POSITION);
    const uint32_t buoy = g.table().at(e.index).mmsi;
    send("EA7JRS", ";BUOY1    _092345z3720.00N/00610.00WB");
    CHECK(g.poll(t, e) && e.kind == GateEvent::LOST && e.mmsi == buoy && g.table().find("BUOY1") < 0);
    // tercero
    send("EA7JRS-10", "}EB3XX>APRS,TCPIP,EA7JRS-10*:!3725.00N/00559.00W-");
    CHECK(g.table().find("EB3XX") >= 0);
    // filtro de distancia
    GateOptions o2; o2.maxKm = 50; g.configure(o2); g.setOwn(37.4, -5.98);
    send("NEAR", "!3725.00N/00559.00W-"); send("FAR", "!4025.00N/00359.00W-");
    CHECK(g.table().find("NEAR") >= 0 && g.table().find("FAR") < 0 && g.stats().far == 1);
    CHECK(near(distanceKm(37.4, -5.98, 37.4, -5.98), 0) && near(distanceKm(0, 0, 0, 1), 111.19, 0.05));
    // caducidad: 30 min sin oír
    t += 1800001; while (g.poll(t, e)) {}
    CHECK(g.active(t) == 0);
    CHECK(g.recent().size() <= 20 && !g.recent().empty());
    // GPS propio del tracker: su baliza se guarda como posición propia, no como blanco
    Gate g3; GateOptions o3; o3.ownCall = "EA7JRS-5"; g3.configure(o3);
    auto send3 = [&](const std::string& src, const std::string& info) { const auto k = kiss(ax25(src, "APLRT1", {}, info)); g3.onKissBytes(k.data(), k.size(), 1000); };
    send3("EA7JRS-5", "!3725.00N/00559.00W>090/010");
    double lat, lon; float cog, sog; uint32_t at;
    CHECK(g3.ownAprs(lat, lon, cog, sog, at) && near(lat, 37 + 25.0 / 60) && near(lon, -(5 + 59.0 / 60)) && cog == 90 && sog == 10);
    CHECK(g3.hasOwn() && g3.table().find("EA7JRS-5") < 0 && !g3.poll(1000, e));   // no es un blanco
    send3("EB7XX-9", "!3725.00N/00559.00W>");                                     // otra estación sí es blanco
    CHECK(g3.table().find("EB7XX-9") >= 0);
    // meteo de internet de la app en la baliza propia -> ownWx
    send3("EA7JRS-5", "!3725.00N/00559.00W_200/012g020t059b10110");
    AprsWx wx; uint32_t wat;
    CHECK(g3.ownWx(wx, wat) && wx.windDir == 200 && near(wx.windMs, 12 * 0.44704) && near(wx.tempC, (59 - 32) * 5.0 / 9.0) && near(wx.pressPa, 101100));
    // TNC2 en texto (BLE en TNC2, servicio UART de Nordic), en trozos como llegan por BLE
    Gate g4; GateEvent e4;
    const std::string l1 = "EB7TNC-9>APLRT1,WIDE1-1:!3725.00N/00559.00W>180/005\r\nEB7T", l2 = "NC-7>APLRT1:!3726.00N/00559.00W>\n";
    g4.onTnc2Bytes((const uint8_t*)l1.data(), l1.size(), 1000);
    CHECK(g4.table().find("EB7TNC-9") >= 0 && g4.table().find("EB7TNC-7") < 0);
    g4.onTnc2Bytes((const uint8_t*)l2.data(), l2.size(), 1000);
    CHECK(g4.table().find("EB7TNC-7") >= 0 && g4.stats().frames == 2);
    const std::string bad = "sin forma\n";
    g4.onTnc2Bytes((const uint8_t*)bad.data(), bad.size(), 1000);
    CHECK(g4.stats().frames == 2);
    CHECK(g4.poll(1000, e4) && e4.kind == GateEvent::STATIC);
}

static void testAnemo() {
    Anemometer a;
    uint32_t total = 0, t = 0;
    a.sample(total, t);
    for (int i = 0; i < 60; i++) { t += 1000; total += 10; a.sample(total, t); }
    CHECK(a.have() && near(a.windMs(), 10 * 2.4 / 3.6) && near(a.avgMs(600), 10 * 2.4 / 3.6));
    for (int i = 0; i < 3; i++) { t += 1000; total += 30; a.sample(total, t); }
    for (int i = 0; i < 10; i++) { t += 1000; total += 10; a.sample(total, t); }
    CHECK(near(a.gustMs(), 30 * 2.4 / 3.6) && near(a.avgMs(73), (70 * 10 + 90) / 73.0 * 2.4 / 3.6));
    t += 10000; a.sample(total, t); CHECK(!a.have());
    CHECK(Anemometer::beaufort(5.0) == 3 && Anemometer::beaufort(40) == 12);
}

int main() {
    testKissAx25();
    testAprs();
    testGate();
    testAnemo();
    printf("%d comprobaciones, %d fallos\n", checks, fails);
    return fails ? 1 : 0;
}
