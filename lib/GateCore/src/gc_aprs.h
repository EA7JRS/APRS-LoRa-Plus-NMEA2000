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
// Posiciones APRS (especificación APRS 1.0.1): sin comprimir, comprimidas, Mic-E, objetos e ítems; rumbo y velocidad
// ("CCC/SSS", comprimidas y Mic-E) y altitud ("/A=", comprimida y Mic-E). Sin dependencias de Arduino.
#include <math.h>
#include <stdlib.h>
#include <string>

namespace GC {

    struct AprsPos {
        std::string name;               // indicativo del emisor, o nombre del objeto / ítem
        double  lat = 0, lon = 0;
        double  cogDeg = -1;            // < 0 = sin dato
        double  sogKnots = -1;
        double  altM = NAN;
        char    symTable = '/', symCode = ' ';
        bool    object = false;         // objeto o ítem
        bool    killed = false;         // objeto / ítem borrado ('_')
        bool    micE = false;
        bool    compressed = false;
        std::string comment;
    };

    namespace AprsDetail {
        inline bool digitOrSpace(char c) { return (c >= '0' && c <= '9') || c == ' '; }

        // "DDMM.mmN" o "DDDMM.mmE" (espacios de ambigüedad = 0); false si no tiene la forma
        inline bool coord(const char* s, int degDigits, double& v) {
            const int n = degDigits + 5;
            for (int i = 0; i < n; i++) {
                if (i == degDigits + 2) { if (s[i] != '.') return false; continue; }
                if (!digitOrSpace(s[i])) return false;
            }
            char b[12];
            for (int i = 0; i < n; i++) b[i] = s[i] == ' ' ? '0' : s[i];
            b[n] = 0;
            double deg = 0;
            for (int i = 0; i < degDigits; i++) deg = deg * 10 + (b[i] - '0');
            const double min = atof(b + degDigits);
            v = deg + min / 60.0;
            const char h = s[n];
            if (degDigits == 2) { if (h == 'S') v = -v; else if (h != 'N') return false; if (v > 90) return false; }
            else { if (h == 'W') v = -v; else if (h != 'E') return false; if (v > 180) return false; }
            return true;
        }

        inline bool isSymTable(char c) { return c == '/' || c == '\\' || (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'j'); }   // comprimida

        inline long base91(const char* s, int n) {
            long v = 0;
            for (int i = 0; i < n; i++) {
                const int c = (unsigned char)s[i] - 33;
                if (c < 0 || c > 90) return -1;
                v = v * 91 + c;
            }
            return v;
        }

        // Comentario: "/A=001234" (pies) -> metros
        inline void altitudeFromComment(AprsPos& p) {
            const size_t a = p.comment.find("/A=");
            if (a == std::string::npos || a + 9 > p.comment.size()) return;
            const std::string v = p.comment.substr(a + 3, 6);
            char* e;
            const long ft = strtol(v.c_str(), &e, 10);
            if (*e == 0) p.altM = ft * 0.3048;
        }

        // Posición sin comprimir (19 caracteres) o comprimida (13) en s; resto -> comentario
        inline bool position(const std::string& s, size_t at, AprsPos& p) {
            if (at + 13 > s.size()) return false;
            const char* q = s.c_str() + at;
            if (at + 19 <= s.size() && coord(q, 2, p.lat) && coord(q + 9, 3, p.lon)) {
                p.symTable = q[8];
                p.symCode = q[18];
                p.comment = s.substr(at + 19);
                // "CCC/SSS" (no en estaciones meteorológicas, donde es el viento)
                if (p.symCode != '_' && p.comment.size() >= 7 && p.comment[3] == '/') {
                    bool ok = true;
                    for (int i : {0, 1, 2, 4, 5, 6}) if (p.comment[i] < '0' || p.comment[i] > '9') ok = false;
                    if (ok) {
                        const int c = atoi(p.comment.substr(0, 3).c_str()), v = atoi(p.comment.substr(4, 3).c_str());
                        if (c >= 1 && c <= 360) p.cogDeg = c == 360 ? 0 : c;
                        p.sogKnots = v;
                        p.comment = p.comment.substr(7);
                    }
                }
                altitudeFromComment(p);
                return true;
            }
            // Comprimida: tabla, YYYY, XXXX, símbolo, c, s, T
            if (!isSymTable(q[0])) return false;
            const long y = base91(q + 1, 4), x = base91(q + 5, 4);
            if (y < 0 || x < 0) return false;
            p.compressed = true;
            p.symTable = q[0];
            if (p.symTable >= 'a' && p.symTable <= 'j') p.symTable = (char)('0' + (p.symTable - 'a'));
            p.symCode = q[9];
            p.lat = 90.0 - y / 380926.0;
            p.lon = -180.0 + x / 190463.0;
            if (p.lat < -90 || p.lat > 90 || p.lon < -180 || p.lon > 180) return false;
            const char c = q[10], sp = q[11], t = q[12];
            if (c != ' ') {
                const int ti = (unsigned char)t - 33;
                if (ti >= 0 && (ti & 0x18) == 0x10) {
                    const long cs = ((unsigned char)c - 33) * 91L + ((unsigned char)sp - 33);
                    p.altM = pow(1.002, (double)cs) * 0.3048;
                } else if (c >= '!' && c <= 'z') {
                    p.cogDeg = fmod(((unsigned char)c - 33) * 4.0, 360.0);
                    p.sogKnots = pow(1.08, (double)((unsigned char)sp - 33)) - 1.0;
                }
            }
            p.comment = s.substr(at + 13);
            if (std::isnan(p.altM)) altitudeFromComment(p);
            return true;
        }

        // Cifra del destino Mic-E (0-9; los espacios de ambigüedad = 0); -1 si no vale
        inline int micEDigit(char c) {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'A' && c <= 'J') return c - 'A';
            if (c >= 'P' && c <= 'Y') return c - 'P';
            if (c == 'K' || c == 'L' || c == 'Z') return 0;
            return -1;
        }

        inline bool micE(const std::string& dst, const std::string& info, AprsPos& p) {
            std::string d = dst.substr(0, dst.find('-'));
            if (d.size() < 6 || info.size() < 9) return false;
            int dig[6];
            for (int i = 0; i < 6; i++) { dig[i] = micEDigit(d[i]); if (dig[i] < 0) return false; }
            double lat = dig[0] * 10 + dig[1] + (dig[2] * 10 + dig[3] + (dig[4] * 10 + dig[5]) / 100.0) / 60.0;
            const bool north = d[3] >= 'P' && d[3] <= 'Z';
            const bool lonOff = d[4] >= 'P' && d[4] <= 'Z';
            const bool west = d[5] >= 'P' && d[5] <= 'Z';
            if (!north) lat = -lat;
            int deg = (unsigned char)info[1] - 28;
            if (lonOff) deg += 100;
            if (deg >= 180 && deg <= 189) deg -= 80;
            else if (deg >= 190 && deg <= 199) deg -= 190;
            int min = (unsigned char)info[2] - 28;
            if (min >= 60) min -= 60;
            const int hun = (unsigned char)info[3] - 28;
            if (deg < 0 || deg > 179 || min < 0 || min > 59 || hun < 0 || hun > 99) return false;
            double lon = deg + (min + hun / 100.0) / 60.0;
            if (west) lon = -lon;
            if (fabs(lat) > 90) return false;
            const int sp = (unsigned char)info[4] - 28, dc = (unsigned char)info[5] - 28, se = (unsigned char)info[6] - 28;
            int speed = sp * 10 + dc / 10;
            if (speed >= 800) speed -= 800;
            int course = (dc % 10) * 100 + se;
            if (course >= 400) course -= 400;
            p.lat = lat; p.lon = lon;
            p.micE = true;
            p.symCode = info[7];
            p.symTable = info[8];
            if (speed >= 0) p.sogKnots = speed;
            if (course >= 1 && course <= 360) p.cogDeg = course == 360 ? 0 : course;
            p.comment = info.substr(9);
            // altitud "xxx}" (metros + 10000, base 91) al principio del comentario (tras un posible tipo de equipo)
            for (size_t i = 0; i + 4 <= p.comment.size() && i <= 1; i++) {
                if (p.comment[i + 3] == '}') {
                    const long v = base91(p.comment.c_str() + i, 3);
                    if (v >= 0) { p.altM = v - 10000.0; p.comment.erase(0, i + 4); }
                    break;
                }
            }
            return true;
        }
    }

    // Posición de un paquete APRS (origen, destino y campo de información); false si no lleva posición
    inline bool aprsPosition(const std::string& src, const std::string& dst, const std::string& info, AprsPos& p) {
        p = AprsPos();
        if (info.empty()) return false;
        const char t = info[0];
        p.name = src;
        switch (t) {
            case '!': case '=':
                return AprsDetail::position(info, 1, p);
            case '/': case '@':
                return info.size() > 8 && AprsDetail::position(info, 8, p);
            case ';': {                                         // objeto: nombre de 9, '*' o '_', hora de 7
                if (info.size() < 18 + 13) return false;
                p.object = true;
                p.killed = info[10] == '_';
                if (info[10] != '*' && info[10] != '_') return false;
                std::string n = info.substr(1, 9);
                while (!n.empty() && n.back() == ' ') n.pop_back();
                if (!AprsDetail::position(info, 18, p)) return false;
                p.object = true; p.name = n;
                return !n.empty();
            }
            case ')': {                                         // ítem: nombre de 3 a 9 y '!' o '_'
                size_t e = 1;
                while (e < info.size() && e <= 10 && info[e] != '!' && info[e] != '_') e++;
                if (e >= info.size() || e < 4 || e > 10) return false;
                const bool killed = info[e] == '_';
                std::string n = info.substr(1, e - 1);
                if (!AprsDetail::position(info, e + 1, p)) return false;
                p.object = true; p.killed = killed; p.name = n;
                return true;
            }
            case '`': case '\'': case 0x1C: case 0x1D:
                return AprsDetail::micE(dst, info, p);
        }
        // Equipos antiguos: el '!' puede ir hasta el carácter 40
        const size_t b = info.find('!');
        if (b != std::string::npos && b > 0 && b < 40) return AprsDetail::position(info, b + 1, p);
        return false;
    }
}
