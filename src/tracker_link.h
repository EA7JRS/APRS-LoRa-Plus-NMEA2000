/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#pragma once
#include <Arduino.h>
#include <string>

// Enlace con el tracker APRS LoRa+: KISS por BLE (servicio 00000001-ba2a-46c9-ae49-01b0961f68bb, notificaciones en …0003),
// TNC2 en texto por BLE (servicio UART de Nordic, el del tracker con el BLE en TNC2) o KISS por TCP (puerto 8001).
// Solo recibe: la pasarela no transmite por radio.
// Va en su propia tarea (conectar y buscar bloquean); los bytes llegan al bucle por un búfer.
namespace TrackerLink {
    struct Status {
        bool        ble = false, tcp = false;       // conectado
        std::string blePeer, tcpPeer;               // nombre / dirección del tracker
        std::string state = "";                     // texto para la WebUI
        uint32_t    bytes = 0;
        uint32_t    lastData = 0;                    // millis()
        bool        bleText = false;                 // el enlace BLE va en TNC2 (texto), no en KISS
        std::string seen;                            // equipos BLE vistos en la última búsqueda (nombre, servicio, dBm)
    };

    void   begin();
    // Copia en buf lo que haya llegado (devuelve los bytes)
    size_t read(uint8_t* buf, size_t size);
    // Lo mismo para el texto TNC2 (tracker con el BLE en TNC2)
    size_t readText(uint8_t* buf, size_t size);
    Status status();
    void   restart();                                // tras cambiar la configuración
}
