/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#include "n2k.h"
#include "config.h"
#include "loghat.h"
#include "anemo.h"
// Las cabeceras de la librería no pasan -Werror (volatile ++ en C++20).
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wvolatile"
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#include <NMEA2000_mcp.h>
#include <N2kMessages.h>
#pragma GCC diagnostic pop
#include <Preferences.h>
#include <cstring>
#include <ctime>
#include <cmath>

namespace N2k {

    static tNMEA2000_mcp* bus = nullptr;
    static Status st;
    static Preferences prefs;
    static uint32_t lastEnv = 0, lastEvent = 0, lastWind = 0, lastGps = 0, lastWx = 0;
    static uint8_t sid = 0;
    static uint8_t seen[32];                     // fuentes oídas (bit por dirección)

    static const unsigned long TX[] = {129038L, 129039L, 129794L, 129809L, 129810L, 130306L, 130311L, 130312L, 130313L, 0};
    static const unsigned long RX[] = {126992L, 129025L, 129029L, 0};

    static void setTime(uint16_t days, double secs) {
        if (days == N2kUInt16NA || N2kIsNA(secs) || days < 19000) return;   // antes de 2022: sin hora
        st.time = (time_t)days * 86400 + (time_t)secs;
        st.hasTime = true;
        st.timeAt = millis();
    }

    static void handler(const tN2kMsg& m) {
        st.received++;
        if (m.Source < 252) seen[m.Source >> 3] |= (uint8_t)(1 << (m.Source & 7));
        switch (m.PGN) {
            case 129025L: {
                double lat, lon;
                if (ParseN2kPGN129025(m, lat, lon) && !N2kIsNA(lat) && !N2kIsNA(lon)) {
                    st.ownLat = lat; st.ownLon = lon; st.hasOwn = true; st.ownAt = millis();
                }
                break;
            }
            case 129029L: {
                unsigned char s, nsat, nref; uint16_t days, refId; double secs, lat, lon, alt, hdop, pdop, geo, age;
                tN2kGNSStype type, refType; tN2kGNSSmethod method;
                if (ParseN2kPGN129029(m, s, days, secs, lat, lon, alt, type, method, nsat, hdop, pdop, geo, nref, refType, refId, age)) {
                    if (!N2kIsNA(lat) && !N2kIsNA(lon)) { st.ownLat = lat; st.ownLon = lon; st.hasOwn = true; st.ownAt = millis(); }
                    setTime(days, secs);
                }
                break;
            }
            case 126992L: {
                unsigned char s; uint16_t days; double secs; tN2kTimeSource src;
                if (ParseN2kPGN126992(m, s, days, secs, src)) setTime(days, secs);
                break;
            }
        }
    }

    void begin() {
        prefs.begin("n2k", false);
        const uint8_t addr = (uint8_t)prefs.getUChar("addr", (uint8_t)Cfg.n2kAddr);
        bus = new tNMEA2000_mcp((unsigned char)Cfg.canCs, Cfg.canMHz == 8 ? MCP_8MHz : MCP_16MHz,
                                Cfg.canInt >= 0 ? (unsigned char)Cfg.canInt : 0xff);
        bus->SetN2kCANMsgBufSize(8);
        bus->SetN2kCANReceiveFrameBufSize(100);
        char serial[16];
        const uint64_t efuse = ESP.getEfuseMac();               // MAC de fábrica (sin arrancar la WiFi)
        uint8_t mac[6];
        for (int i = 0; i < 6; i++) mac[i] = (uint8_t)(efuse >> (8 * i));
        snprintf(serial, sizeof serial, "%02X%02X%02X", mac[3], mac[4], mac[5]);
        bus->SetProductInformation(serial, 5305, "APRS LoRa+ NMEA 2000", GW_VERSION " (EA7JRS)", "XIAO ESP32-S3 + CAN");
        // Equipo de navegación (clase 60), función AIS (195); fabricante 2046 = sin registrar
        const unsigned long unique = ((unsigned long)mac[3] << 16 | (unsigned long)mac[4] << 8 | mac[5]) & 0x1FFFFF;
        bus->SetDeviceInformation(unique, 195, 60, 2046);
        bus->ExtendTransmitMessages(TX);
        bus->ExtendReceiveMessages(RX);
        bus->SetMode(tNMEA2000::N2km_ListenAndNode, addr);
        bus->EnableForward(false);
        bus->SetMsgHandler(handler);
        st.open = bus->Open();
        st.address = addr;
        Serial.printf("n2k: %s, CS %d, %d MHz, dirección %u\n", st.open ? "abierto" : "sin MCP2515", Cfg.canCs, Cfg.canMHz, addr);
    }

    static bool send(const tN2kMsg& m) {
        const bool ok = bus->SendMsg(m);
        if (ok) st.sent++; else st.errors++;
        return ok;
    }

    // Tipo AIS para estaciones sin etiqueta "AIS:": velero o embarcación de recreo según el símbolo APRS
    static uint8_t shipTypeFor(const NMEA0183::AisStation& s) {
        if (s.hasInfo && s.info.shipType) return s.info.shipType;
        return 0;
    }

    static void sendStation(const GC::Gate& gate, const GC::GateEvent& e) {
        const NMEA0183::AisStation& s = gate.table().at(e.index);
        tN2kMsg m;
        const double cog = s.cogDeg >= 0 ? DegToRad(s.cogDeg) : N2kDoubleNA;
        const double sog = s.sogKnots >= 0 ? KnotsToms(s.sogKnots) : N2kDoubleNA;
        const std::string name = GC::Gate::aisName(s);
        std::string call = s.callsign;
        if (s.hasInfo) { char b[12]; AIS_Vessel::baseCallsign(s.callsign, b, sizeof b); call = b; }
        if (s.hasInfo && s.info.call[0]) call = s.info.call;              // distintivo maritimo del MMSI
        if (call.size() > 7) call.resize(7);
        uint16_t bow = 0, stern = 0; uint8_t port = 0, stbd = 0;
        const bool dims = s.hasInfo && s.info.length > 0;
        if (dims) AIS_Vessel::splitDimensions(s.info.length, s.info.beam, bow, stern, port, stbd);
        const double len = dims ? s.info.length : N2kDoubleNA, beam = dims && s.info.beam ? s.info.beam : N2kDoubleNA;
        const double refStbd = dims ? stbd : N2kDoubleNA, refBow = dims ? bow : N2kDoubleNA;
        // Estación APRS genérica (sin perfil "AIS:" náutico): ayuda a la navegación virtual (129041, mensaje 21).
        // El plotter la muestra como baliza, no como buque, así que no dispara la alarma de abordaje (no es un barco real).
        if (Cfg.aton && !s.hasInfo) {
            tN2kAISAtoNReportData a;
            a.MessageID = 21;
            a.Repeat = N2kaisr_Initial;
            a.UserID = s.mmsi;
            a.Longitude = s.lon;
            a.Latitude = s.lat;
            a.Accuracy = false;
            a.RAIM = false;
            a.Seconds = 60;
            a.Length = N2kDoubleNA;
            a.Beam = N2kDoubleNA;
            a.PositionReferenceStarboard = N2kDoubleNA;
            a.PositionReferenceTrueNorth = N2kDoubleNA;
            a.AtoNType = N2kAISAtoN_not_specified;
            a.OffPositionIndicator = false;
            a.VirtualAtoNFlag = true;
            a.AssignedModeFlag = false;
            a.GNSSType = N2kGNSSt_GPS;
            strncpy(a.AtoNName, name.c_str(), sizeof(a.AtoNName) - 1);
            a.AtoNName[sizeof(a.AtoNName) - 1] = 0;
            SetN2kAISAtoNReport(m, a);
            send(m);
            return;
        }
        if (GC::Gate::classA(s)) {
            if (e.kind == GC::GateEvent::POSITION) {
                SetN2kPGN129038(m, 1, N2kaisr_Initial, s.mmsi, s.lat, s.lon, false, false, 60, cog, sog,
                                N2kaischannel_A_VDL_reception, N2kDoubleNA, N2kDoubleNA, N2kaisns_Under_Way_Motoring);
                send(m);
            } else {
                SetN2kPGN129794(m, 5, N2kaisr_Initial, s.mmsi, 0, call.c_str(), name.c_str(), shipTypeFor(s), len, beam, refStbd, refBow,
                                N2kUInt16NA, N2kDoubleNA, s.info.draughtDm ? s.info.draughtDm / 10.0 : N2kDoubleNA, "",
                                N2kaisv_ITU_R_M_1371_1, N2kGNSSt_GPS, N2kaisdte_Ready);
                send(m);
            }
            return;
        }
        if (e.kind == GC::GateEvent::POSITION) {
            SetN2kPGN129039(m, 18, N2kaisr_Initial, s.mmsi, s.lat, s.lon, false, false, 60, cog, sog, N2kaischannel_A_VDL_reception,
                            N2kDoubleNA, N2kaisunit_ClassB_CS, false, false, false, false, N2kaismode_Autonomous, false);
            send(m);
        } else {
            SetN2kPGN129809(m, 24, N2kaisr_Initial, s.mmsi, name.c_str());
            send(m);
            SetN2kPGN129810(m, 24, N2kaisr_Initial, s.mmsi, shipTypeFor(s), "APRSLP", call.c_str(), len, beam, refStbd, refBow, 0);
            send(m);
        }
    }

    static void sendEnvironment() {
        const LogHat::Data& d = LogHat::data();
        if (!d.at || millis() - d.at > 10000) return;
        tN2kMsg m;
        sid = (uint8_t)((sid + 1) % 253);
        SetN2kPGN130311(m, sid, N2kts_OutsideTemperature, CToKelvin(d.tempC), N2khs_OutsideHumidity, d.humPct, N2kDoubleNA);
        send(m);
        SetN2kPGN130312(m, sid, 0, N2kts_OutsideTemperature, CToKelvin(d.tempC));   // temperatura (obsoleto pero compatible)
        send(m);
        SetN2kPGN130316(m, sid, 0, N2kts_OutsideTemperature, CToKelvin(d.tempC));   // temperatura de rango ampliado (el que prefiere el Garmin)
        send(m);
        SetN2kPGN130313(m, sid, 0, N2khs_OutsideHumidity, d.humPct);
        send(m);
    }

    void loop(GC::Gate& gate) {
        if (!bus) return;
        bus->ParseMessages();
        if (bus->ReadResetAddressChanged()) {
            st.address = bus->GetN2kSource();
            prefs.putUChar("addr", st.address);
        }
        const uint32_t now = millis();
        if (st.hasOwn && now - st.ownAt < 60000) gate.setOwn(st.ownLat, st.ownLon);
        // como mucho un blanco cada 20 ms: no saturar el bus
        if (now - lastEvent >= 20) {
            GC::GateEvent e;
            if (gate.poll(now, e)) {
                lastEvent = now;
                if (e.kind != GC::GateEvent::LOST && st.open) sendStation(gate, e);
            }
        }
        // Viento (130306) cada segundo: velocidad de los últimos 3 s; sin veleta, sin dirección
        if (st.open && Anemo::active() && now - lastWind >= 1000) {
            lastWind = now;
            tN2kMsg m;
            SetN2kPGN130306(m, 0xFF, Anemo::meter().windMs(), N2kDoubleNA, Cfg.windRef == "true" ? N2kWind_True_North : N2kWind_Apparent);
            send(m);
        }
        if (Cfg.sendEnv && st.open && now - lastEnv >= (uint32_t)Cfg.envS * 1000) {
            lastEnv = now;
            sendEnvironment();
        }
        // Tiempo de internet de la app (viento/presión/temperatura) al bus, 1 vez/s, solo si no hay sensor del HAT fresco
        if (Cfg.wxOut && st.open && now - lastWx >= 1000) {
            const LogHat::Data& d = LogHat::data();
            const bool localEnv = Cfg.sendEnv && Cfg.logHat && d.at && now - d.at < 10000;
            GC::AprsWx wx; uint32_t at;
            if (!localEnv && gate.ownWx(wx, at) && now - at < 1800000 && wx.valid) {   // la meteo vale 30 min
                lastWx = now;
                sid = (uint8_t)((sid + 1) % 253);
                tN2kMsg m;
                if (wx.windDir >= 0 && std::isfinite(wx.windMs)) {
                    SetN2kPGN130306(m, sid, wx.windMs, DegToRad((double)wx.windDir), N2kWind_True_North);
                    send(m);
                }
                if (std::isfinite(wx.pressPa)) { SetN2kPGN130314(m, sid, 0, N2kps_Atmospheric, wx.pressPa); send(m); }
                if (std::isfinite(wx.tempC)) { SetN2kPGN130316(m, sid, 0, N2kts_OutsideTemperature, CToKelvin(wx.tempC)); send(m); }
            }
        }
        // GPS propio del tracker al bus (129025/129026/129029), 1 vez/s, solo si no hay otra antena GPS en el bus
        if (Cfg.gpsOut && st.open && now - lastGps >= 1000) {
            double lat, lon; float cog, sog; uint32_t at;
            const bool busGps = st.hasOwn && now - st.ownAt < 10000;
            if (!busGps && gate.ownAprs(lat, lon, cog, sog, at) && now - at < 60000) {
                lastGps = now;
                sid = (uint8_t)((sid + 1) % 253);
                tN2kMsg m;
                SetN2kPGN129025(m, lat, lon);
                send(m);
                SetN2kCOGSOGRapid(m, sid, N2khr_true, cog >= 0 ? DegToRad(cog) : N2kDoubleNA, sog >= 0 ? KnotsToms(sog) : N2kDoubleNA);
                send(m);
                const time_t t = time(nullptr);
                const bool haveTime = t > 1600000000;
                SetN2kPGN129029(m, sid, haveTime ? (uint16_t)(t / 86400) : N2kUInt16NA, haveTime ? (double)(t % 86400) : N2kDoubleNA,
                                lat, lon, N2kDoubleNA, N2kGNSSt_GPS, N2kGNSSm_GNSSfix, 10, 1.0, 1.0, N2kDoubleNA);
                send(m);
            }
        }
        int n = 0;
        for (int i = 0; i < 32; i++) n += __builtin_popcount(seen[i]);
        st.devices = n;
    }

    const Status& status() { return st; }
}
