/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include <string>

#define GW_VERSION "5.4.5"

#ifndef GW_BOARD
    #define GW_BOARD "xiao_esp32s3_can"
#endif

// Pines del XIAO ESP32-S3: D0 = GPIO1, D1 = GPIO2, D2 = GPIO3, D3 = GPIO4, D4 = GPIO5 (SDA), D5 = GPIO6 (SCL),
// D6 = GPIO43 (TX), D7 = GPIO44 (RX), D8 = GPIO7 (SCK), D9 = GPIO8 (MISO), D10 = GPIO9 (MOSI)
struct GwConfig {
    std::string name = "APRS-N2K";              // red (mDNS), punto de acceso, Bluetooth y WebUI
    std::string pin = "123456";                  // clave de la WebUI para cambiar la configuración (igual que el PIN de control del ecosistema APRS LoRa+; vacía = sin clave)

    // WiFi: 0 apagada, 1 cliente (con punto de acceso de rescate), 2 punto de acceso
    int         wifiMode = 2;
    std::string ssid, pass;
    std::string apPass = "aprsn2000";            // mínimo 8 caracteres

    // Tracker: "ble", "tcp" o "both"
    std::string src = "tcp";
    std::string bleName = "";                    // vacío = el primero que anuncie el servicio KISS; si no, parte del nombre
    std::string bleAddr = "";                    // dirección fija (aa:bb:cc:dd:ee:ff), gana a bleName
    std::string tcpHost = "";                    // vacío = buscarlo (mDNS "_kiss._tcp" y los equipos conectados al punto de acceso)
    int         tcpPort = 8001;

    // Blancos AIS
    int         mid = 224;                       // MID del MMSI sintético, como el tracker
    int         refreshS = 30;
    int         ttlMin = 30;
    int         staticMin = 6;
    double      maxKm = 0;
    bool        objects = true;
    bool        aton = true;                      // estaciones APRS sin perfil AIS: real -> ayuda a la navegación virtual (129041), sin alarma de abordaje en el plotter
    std::string ignore = "";                     // indicativos que no se envían (el del propio barco), separados por comas

    // GPS propio: emitir la posición del tracker al bus (129025/129026/129029) cuando no hay otra antena GPS
    bool        gpsOut = false;                   // apagado por defecto (si el barco ya tiene GPS en el bus, dejarlo así)
    bool        wxOut = false;                     // emitir al bus el tiempo de internet de la app (viento/presión/temperatura) si no hay sensores
    std::string ownCall = "";                    // indicativo-SSID del propio tracker (el barco), p. ej. EA7JRS-5

    // NMEA 2000 (MCP2515)
    int         canCs = 44;                      // D7
    int         canMHz = 16;                     // cristal del MCP2515: 16 u 8
    int         canInt = -1;                     // sin interrupción: se consulta en el bucle
    int         n2kAddr = 34;                    // dirección preferida (se negocia en el bus y se guarda la que queda)
    bool        sendEnv = true;                  // temperatura y humedad del HAT al bus
    int         envS = 5;

    // Anemómetro de cazoletas (ampolla reed a masa): D3 = GPIO4
    bool        anemoOn = false;
    int         anemoPin = 4;
    double      anemoKmhHz = 2.4;                // km/h por impulso por segundo (Misol / Fine Offset 2,4; Davis 6410 3,62)
    bool        anemoPullup = true;
    std::string windRef = "apparent";            // "apparent" en el barco (relativo a la proa) o "true" en tierra (fijo)

    // HAT XIAO-LOG (SHT40, BH1750, PCF8563): I2C en D4 / D5
    bool        logHat = true;
    int         sda = 5, scl = 6;
    bool        battery = false;                 // medir la batería en D1 (divisor 100 k / 100 k)
    int         batPin = 2;
};

extern GwConfig Cfg;

namespace Config {
    void        begin();                          // monta LittleFS y lee /gw_conf.json
    bool        save();
    void        toJson(JsonObject o, bool secrets);
    // Aplica un JSON (parcial o entero); vacío = bien, si no el error
    std::string apply(JsonObjectConst in, GwConfig& out);
    void        reset();
}
