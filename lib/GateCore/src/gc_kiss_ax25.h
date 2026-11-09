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
// KISS (FEND 0xC0, FESC 0xDB 0xDC / 0xDD) y tramas AX.25 UI: lo que manda el tracker por BLE o por TCP.
// Sin dependencias de Arduino.
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

namespace GC {

    // Recoge tramas KISS de un flujo de bytes (trozos de BLE o de TCP de cualquier tamaño)
    class KissReader {
    public:
        static const uint8_t FEND = 0xC0, FESC = 0xDB, TFEND = 0xDC, TFESC = 0xDD;
        static const size_t  MAX = 400;

        // true con una trama de datos (puerto 0, orden 0) completa en frame (sin el byte de tipo)
        bool push(uint8_t c, std::vector<uint8_t>& frame) {
            if (c == FEND) {
                const bool ok = !buf_.empty() && !over_ && (buf_[0] & 0x0F) == 0x00;
                if (ok) frame.assign(buf_.begin() + 1, buf_.end());
                buf_.clear(); esc_ = false; over_ = false;
                return ok && !frame.empty();
            }
            if (esc_) {
                esc_ = false;
                if (c == TFEND) c = FEND;
                else if (c == TFESC) c = FESC;
            } else if (c == FESC) {
                esc_ = true;
                return false;
            }
            if (buf_.size() >= MAX) { over_ = true; return false; }
            buf_.push_back(c);
            return false;
        }
        void clear() { buf_.clear(); esc_ = false; over_ = false; }
    private:
        std::vector<uint8_t> buf_;
        bool esc_ = false, over_ = false;
    };

    struct Ax25 {
        std::string src, dst;
        std::vector<std::string> path;
        std::string info;
    };

    // "EA7JRS-9" desde los 7 bytes de una dirección AX.25
    inline std::string ax25Call(const uint8_t* a) {
        std::string s;
        for (int i = 0; i < 6; i++) {
            const char c = (char)(a[i] >> 1);
            if (c != ' ') s += c;
        }
        const int ssid = (a[6] >> 1) & 0x0F;
        if (ssid) s += "-" + std::to_string(ssid);
        return s;
    }

    // Trama UI (control 0x03, PID 0xF0); false si no lo es
    inline bool ax25Decode(const uint8_t* d, size_t n, Ax25& out) {
        if (n < 16) return false;
        size_t i = 0;
        int k = 0;
        out.path.clear();
        while (true) {
            if (i + 7 > n) return false;
            const std::string c = ax25Call(d + i);
            if (k == 0) out.dst = c;
            else if (k == 1) out.src = c;
            else out.path.push_back(c + ((d[i + 6] & 0x80) ? "*" : ""));
            const bool last = d[i + 6] & 0x01;
            i += 7;
            k++;
            if (last) break;
            if (k > 10) return false;
        }
        if (k < 2 || i + 2 > n) return false;
        if (d[i] != 0x03 || d[i + 1] != 0xF0) return false;
        out.info.assign((const char*)d + i + 2, n - i - 2);
        while (!out.info.empty() && (out.info.back() == '\r' || out.info.back() == '\n')) out.info.pop_back();
        return !out.src.empty();
    }

    // Texto TNC2 "SRC>DST,PATH:info" (para el registro y la WebUI)
    inline std::string tnc2(const Ax25& a) {
        std::string s = a.src + ">" + a.dst;
        for (const auto& p : a.path) s += "," + p;
        return s + ":" + a.info;
    }

    // Lee un paquete en texto TNC2 (tráfico de terceros "}..."); false si no tiene la forma
    inline bool tnc2Parse(const std::string& t, Ax25& out) {
        const size_t gt = t.find('>'), colon = t.find(':');
        if (gt == std::string::npos || colon == std::string::npos || gt == 0 || colon < gt) return false;
        out.src = t.substr(0, gt);
        std::string hdr = t.substr(gt + 1, colon - gt - 1);
        out.path.clear();
        size_t p = hdr.find(',');
        out.dst = hdr.substr(0, p);
        while (p != std::string::npos) {
            const size_t q = hdr.find(',', p + 1);
            out.path.push_back(hdr.substr(p + 1, q == std::string::npos ? std::string::npos : q - p - 1));
            p = q;
        }
        out.info = t.substr(colon + 1);
        return !out.dst.empty();
    }
}
