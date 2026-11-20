/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000.
 *
 * APRS LoRa+ NMEA 2000 is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * APRS LoRa+ NMEA 2000 is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with APRS LoRa+ NMEA 2000. If not, see <https://www.gnu.org/licenses/>.
 */

#pragma once
// Motor de la pasarela: tramas AX.25 del tracker -> posiciones APRS -> tabla de blancos AIS (el mismo MMSI sintético
// que el tracker en su salida NMEA 0183) -> qué mensajes NMEA 2000 tocan y cuándo. Sin dependencias de Arduino.
#include <ctype.h>
#include <deque>
#include <math.h>
#include <string>
#include <vector>
#include "gc_kiss_ax25.h"
#include "gc_aprs.h"
#include "nmea0183.h"
#include "ais_vessel.h"

namespace GC {

    inline double distanceKm(double lat1, double lon1, double lat2, double lon2) {
        const double r = M_PI / 180.0;
        const double dLat = (lat2 - lat1) * r, dLon = (lon2 - lon1) * r;
        const double a = sin(dLat / 2) * sin(dLat / 2) + cos(lat1 * r) * cos(lat2 * r) * sin(dLon / 2) * sin(dLon / 2);
        return 6371.0 * 2 * atan2(sqrt(a), sqrt(1 - a));
    }

    struct GateOptions {
        uint16_t    mid = 224;                  // MID del MMSI sintético (224 = España), como el tracker
        uint32_t    refreshMs = 30000;          // repetición de la posición de cada blanco
        uint32_t    ttlMs = 1800000;            // 30 min sin oírlo: deja de enviarse (el plotter lo da por perdido)
        uint32_t    staticMs = 360000;          // nombre, indicativo y tipo cada 6 min
        double      maxKm = 0;                  // > 0: solo estaciones a menos de esta distancia del barco (si se sabe)
        bool        objects = true;             // objetos e ítems APRS también como blancos
        std::string ignore;                     // indicativos separados por comas que no se envían (el propio barco)
        std::string ownCall;                    // indicativo-SSID del propio tracker (el barco): su posición se guarda como GPS propio, no como blanco
    };

    struct GateEvent {
        enum Kind { NONE, POSITION, STATIC, LOST } kind = NONE;
        int index = -1;
        uint32_t mmsi = 0;
    };

    class Gate {
    public:
        struct Stats { uint32_t frames = 0, positions = 0, ignored = 0, far = 0; };

        Gate() : table_(224) {}

        void configure(const GateOptions& o) { opt_ = o; table_ = NMEA0183::AisTable(o.mid); dirty_.assign(NMEA0183::AisTable::MAX_STATIONS, false); }
        const GateOptions& options() const { return opt_; }

        // Posición del barco (del plotter por NMEA 2000) para el filtro de distancia y la WebUI
        void setOwn(double lat, double lon) { ownLat_ = lat; ownLon_ = lon; hasOwn_ = true; }
        bool hasOwn() const { return hasOwn_; }
        double ownLat() const { return ownLat_; }
        double ownLon() const { return ownLon_; }

        // GPS propio desde la baliza APRS del tracker (opt_.ownCall), para emitirlo al bus si no hay otra antena GPS.
        // cog en grados y sog en nudos (-1 = sin dato); at = millis de la última baliza propia.
        bool ownAprs(double& lat, double& lon, float& cog, float& sog, uint32_t& at) const {
            if (!hasOwnAprs_) return false;
            lat = ownAprsLat_; lon = ownAprsLon_; cog = ownAprsCog_; sog = ownAprsSog_; at = ownAprsAt_;
            return true;
        }

        // Meteo de internet que la app manda en la baliza del propio barco (viento, temperatura, presión)
        bool ownWx(AprsWx& wx, uint32_t& at) const {
            if (!hasOwnWx_) return false;
            wx = ownWx_; at = ownWxAt_;
            return true;
        }

        // Bytes KISS tal como llegan (BLE o TCP)
        void onKissBytes(const uint8_t* d, size_t n, uint32_t now) {
            std::vector<uint8_t> f;
            for (size_t i = 0; i < n; i++) if (kiss_.push(d[i], f)) onFrame(f.data(), f.size(), now);
        }
        void resetKiss() { kiss_.clear(); tncLine_.clear(); }

        // Texto TNC2 por líneas ("SRC>DST,PATH:info\n"): tracker con el BLE en TNC2 (servicio UART de Nordic)
        void onTnc2Bytes(const uint8_t* d, size_t n, uint32_t now) {
            for (size_t i = 0; i < n; i++) {
                const char c = (char)d[i];
                if (c == '\n' || c == '\r') {
                    if (!tncLine_.empty()) {
                        Ax25 a;
                        if (tnc2Parse(tncLine_, a)) { stats_.frames++; onPacket(a, now, 0); }
                        tncLine_.clear();
                    }
                } else if (tncLine_.size() < 512) tncLine_ += c;
                else tncLine_.clear();                                  // línea imposible: se descarta
            }
        }

        void onFrame(const uint8_t* d, size_t n, uint32_t now) {
            Ax25 a;
            if (!ax25Decode(d, n, a)) return;
            stats_.frames++;
            onPacket(a, now, 0);
        }

        void onPacket(const Ax25& a, uint32_t now, int depth) {
            remember(tnc2(a));
            if (!a.info.empty() && a.info[0] == '}' && depth == 0) {   // tráfico de terceros: el paquete de dentro
                Ax25 in;
                if (tnc2Parse(a.info.substr(1), in)) onPacket(in, now, 1);
                return;
            }
            AprsPos p;
            if (!aprsPosition(a.src, a.dst, a.info, p)) return;
            if (p.object && !opt_.objects) return;
            if (p.lat == 0 && p.lon == 0) return;
            if (!opt_.ownCall.empty() && sameCall(p.name, opt_.ownCall)) {   // baliza del propio tracker: es el barco, no un blanco
                ownAprsLat_ = p.lat; ownAprsLon_ = p.lon;
                ownAprsCog_ = p.cogDeg; ownAprsSog_ = p.sogKnots;
                ownAprsAt_ = now; hasOwnAprs_ = true;
                if (p.wx.valid) { ownWx_ = p.wx; ownWxAt_ = now; hasOwnWx_ = true; }   // meteo de internet de la app
                setOwn(p.lat, p.lon);                                        // también para el filtro de distancia si no hay GPS del bus
                return;
            }
            if (ignored(p.name)) { stats_.ignored++; return; }
            const int known = table_.find(p.name.c_str());
            if (p.killed) {
                if (known >= 0) { lost_.push_back(table_.at(known).mmsi); table_.forget(known); dirty_[known] = false; }
                return;
            }
            if (opt_.maxKm > 0 && hasOwn_ && distanceKm(ownLat_, ownLon_, p.lat, p.lon) > opt_.maxKm) { stats_.far++; return; }
            bool isNew;
            const int i = table_.track(p.name.c_str(), p.lat, p.lon, p.sogKnots, p.cogDeg, now, isNew);
            if (i < 0) return;
            stats_.positions++;
            AIS_Vessel::Info info;
            if (AIS_Vessel::parseTag(a.info.c_str(), info)) table_.setInfo(i, info);
            if (i < (int)alt_.size()) alt_[i] = p.altM;
            dirty_[i] = true;
        }

        // Siguiente mensaje que hay que mandar (llamar hasta que devuelva NONE)
        bool poll(uint32_t now, GateEvent& e) {
            e = GateEvent();
            if (!lost_.empty()) { e.kind = GateEvent::LOST; e.mmsi = lost_.front(); lost_.pop_front(); return true; }
            for (int i = 0; i < NMEA0183::AisTable::MAX_STATIONS; i++) {
                const auto& s = table_.at(i);
                if (!s.used) continue;
                if (dirty_[i] || (now - s.heardAt <= opt_.ttlMs && now - s.emittedAt >= opt_.refreshMs)) {
                    if (now - s.heardAt > opt_.ttlMs) { dirty_[i] = false; continue; }
                    // los datos estáticos primero, para que el plotter ponga nombre al blanco desde la primera posición
                    if (table_.staticDue(i, now, opt_.staticMs)) {
                        table_.markStatic(i, now);
                        e.kind = GateEvent::STATIC; e.index = i; e.mmsi = s.mmsi;
                        return true;
                    }
                    dirty_[i] = false;
                    table_.markEmitted(i, now);
                    e.kind = GateEvent::POSITION; e.index = i; e.mmsi = s.mmsi;
                    return true;
                }
            }
            return false;
        }

        const NMEA0183::AisTable& table() const { return table_; }
        double altitudeM(int i) const { return i >= 0 && i < (int)alt_.size() ? alt_[i] : NAN; }
        const Stats& stats() const { return stats_; }
        const std::deque<std::string>& recent() const { return recent_; }
        int active(uint32_t now) const {
            int n = 0;
            for (int i = 0; i < NMEA0183::AisTable::MAX_STATIONS; i++) if (table_.at(i).used && now - table_.at(i).heardAt <= opt_.ttlMs) n++;
            return n;
        }

        // Nombre AIS (máx. 20) y clase de un blanco
        static std::string aisName(const NMEA0183::AisStation& s) {
            std::string n = s.hasInfo && s.info.name[0] ? s.info.name : s.callsign;
            char b[24];
            AIS_Vessel::sanitizeName(n.c_str(), b, sizeof b);
            return b;
        }
        static bool classA(const NMEA0183::AisStation& s) { return s.hasInfo && s.info.aisClass == AIS_Vessel::CLASS_A; }

    private:
        GateOptions opt_;
        NMEA0183::AisTable table_;
        KissReader kiss_;
        std::string tncLine_;
        std::vector<bool> dirty_ = std::vector<bool>(NMEA0183::AisTable::MAX_STATIONS, false);
        std::vector<double> alt_ = std::vector<double>(NMEA0183::AisTable::MAX_STATIONS, NAN);
        std::deque<uint32_t> lost_;
        std::deque<std::string> recent_;
        Stats stats_;
        bool hasOwn_ = false;
        double ownLat_ = 0, ownLon_ = 0;
        bool hasOwnAprs_ = false;
        double ownAprsLat_ = 0, ownAprsLon_ = 0;
        float ownAprsCog_ = -1, ownAprsSog_ = -1;
        uint32_t ownAprsAt_ = 0;
        bool hasOwnWx_ = false;
        AprsWx ownWx_;
        uint32_t ownWxAt_ = 0;

        static bool sameCall(const std::string& a, const std::string& b) {
            if (a.size() != b.size()) return false;
            for (size_t i = 0; i < a.size(); i++)
                if (toupper((unsigned char)a[i]) != toupper((unsigned char)b[i])) return false;
            return true;
        }

        void remember(const std::string& t) {
            recent_.push_back(t.size() > 200 ? t.substr(0, 200) : t);
            while (recent_.size() > 20) recent_.pop_front();
        }
        bool ignored(const std::string& name) const {
            size_t p = 0;
            const std::string& l = opt_.ignore;
            while (p < l.size()) {
                size_t q = l.find(',', p);
                if (q == std::string::npos) q = l.size();
                std::string c = l.substr(p, q - p);
                while (!c.empty() && c.front() == ' ') c.erase(0, 1);
                while (!c.empty() && c.back() == ' ') c.pop_back();
                if (!c.empty()) {
                    bool same = c.size() == name.size();
                    for (size_t i = 0; same && i < c.size(); i++) same = toupper((unsigned char)c[i]) == toupper((unsigned char)name[i]);
                    if (same) return true;
                }
                p = q + 1;
            }
            return false;
        }
    };
}
