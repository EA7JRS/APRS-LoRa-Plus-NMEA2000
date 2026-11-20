/* Copyright (C) 2026 Manuel - EA7JRS
 *
 * This file is part of APRS LoRa+ NMEA 2000. GPL-3.0-or-later (ver LICENSE).
 */

#include "tracker_link.h"
#include "config.h"
#include <NimBLEDevice.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <esp_wifi.h>
#include <esp_netif.h>
#include <freertos/stream_buffer.h>
#include <freertos/semphr.h>

namespace TrackerLink {

    static const char* KISS_SVC = "00000001-ba2a-46c9-ae49-01b0961f68bb";
    static const char* KISS_TX  = "00000003-ba2a-46c9-ae49-01b0961f68bb";   // el tracker notifica aquí
    // UART de Nordic (tracker con el BLE en TNC2). El tracker notifica en …0002 (al revés que el Nordic de serie).
    static const char* NUS_SVC  = "6E400001-B5A3-F393-E0A9-E50E24DCCA9E";
    static const char* NUS_A    = "6E400002-B5A3-F393-E0A9-E50E24DCCA9E";
    static const char* NUS_B    = "6E400003-B5A3-F393-E0A9-E50E24DCCA9E";

    static StreamBufferHandle_t sb = nullptr, sbText = nullptr;
    static SemaphoreHandle_t mtx = nullptr;
    static Status st;
    static volatile bool restartAsked = false;

    static void push(const uint8_t* d, size_t n, bool text = false) {
        StreamBufferHandle_t b = text ? sbText : sb;
        if (!b || !n) return;
        xStreamBufferSend(b, d, n, 0);
        xSemaphoreTake(mtx, portMAX_DELAY);
        st.bytes += n;
        st.lastData = millis();
        xSemaphoreGive(mtx);
    }
    static void setState(const std::string& s) { xSemaphoreTake(mtx, portMAX_DELAY); st.state = s; xSemaphoreGive(mtx); }

    // ------------------------------------------------------------------------------------------------ BLE
    static NimBLEClient* client = nullptr;
    static volatile bool found = false;                 // tracker con KISS (o la dirección fija)
    static NimBLEAddress foundAddr, textAddr;
    static std::string foundName, textName;
    static volatile bool textFound = false;             // tracker con el BLE en TNC2 (UART de Nordic)
    static std::string seenList;                        // lo visto en esta búsqueda, para la WebUI
    static int seenCount = 0;
    static uint32_t bleRetryAt = 0;

    static bool trackerName(const std::string& n) {     // LoRaTracker, APRSLoRa+…, MAPRSLoRa+…
        return n.find("LoRa") != std::string::npos || n.find("LORA") != std::string::npos;
    }

    class ScanCb : public NimBLEScanCallbacks {
        void onResult(const NimBLEAdvertisedDevice* d) override {
            if (found) return;
            const std::string name = d->getName();
            const bool kiss = d->isAdvertisingService(NimBLEUUID(KISS_SVC));
            const bool nus = d->isAdvertisingService(NimBLEUUID(NUS_SVC));
            const bool fixed = !Cfg.bleAddr.empty() && strcasecmp(d->getAddress().toString().c_str(), Cfg.bleAddr.c_str()) == 0;
            if ((kiss || trackerName(name) || fixed) && seenCount < 6 && seenList.find(d->getAddress().toString()) == std::string::npos) {
                char b[96];
                snprintf(b, sizeof b, "%s%s %s (%s, %d dBm)", seenCount ? " · " : "", name.empty() ? "?" : name.c_str(),
                         d->getAddress().toString().c_str(), kiss ? "KISS" : nus ? "TNC2" : "sin servicio APRS", d->getRSSI());
                seenList += b;
                seenCount++;
            }
            if (!Cfg.bleAddr.empty()) {
                if (!fixed) return;
            } else {
                const bool nameOk = Cfg.bleName.empty() ? true : name.find(Cfg.bleName) != std::string::npos;
                if (!nameOk) return;
                if (!kiss) {
                    // TNC2: el UART de Nordic lo usan muchos equipos; solo un tracker (por el nombre) o el nombre pedido
                    if (nus && !textFound && (trackerName(name) || !Cfg.bleName.empty())) {
                        textAddr = d->getAddress(); textName = name; textFound = true;
                    }
                    return;
                }
            }
            foundAddr = d->getAddress();
            foundName = name;
            found = true;
            NimBLEDevice::getScan()->stop();
        }
    } scanCb;

    static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) { push(data, len); }
    static void onNotifyText(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) { push(data, len, true); }

    // Se suscribe al KISS; si no lo tiene, al UART de Nordic (texto TNC2)
    static bool subscribe(bool& text) {
        NimBLERemoteService* svc = client->getService(KISS_SVC);
        NimBLERemoteCharacteristic* ch = svc ? svc->getCharacteristic(KISS_TX) : nullptr;
        if (ch && ch->canNotify() && ch->subscribe(true, onNotify)) { text = false; return true; }
        svc = client->getService(NUS_SVC);
        if (!svc) return false;
        for (const char* u : {NUS_A, NUS_B}) {
            ch = svc->getCharacteristic(u);
            if (ch && ch->canNotify() && ch->subscribe(true, onNotifyText)) { text = true; return true; }
        }
        return false;
    }

    static void bleStep() {
        const bool want = Cfg.src == "ble" || Cfg.src == "both";
        if (!want) {
            if (client && client->isConnected()) client->disconnect();
            st.ble = false;
            return;
        }
        if (client && client->isConnected()) { st.ble = true; return; }
        if (st.ble) { st.ble = false; setState("BLE: enlace perdido"); bleRetryAt = millis() + 2000; }
        if ((int32_t)(millis() - bleRetryAt) < 0) return;
        found = false;
        textFound = false;
        seenList.clear();
        seenCount = 0;
        setState("BLE: buscando el tracker");
        NimBLEScan* scan = NimBLEDevice::getScan();
        scan->setScanCallbacks(&scanCb, false);
        scan->setActiveScan(true);                      // el nombre del tracker va en la respuesta al escaneo
        scan->start(6000, false, true);
        const uint32_t t0 = millis();
        while (!found && scan->isScanning() && millis() - t0 < 6500) vTaskDelay(pdMS_TO_TICKS(50));
        scan->stop();
        xSemaphoreTake(mtx, portMAX_DELAY);
        st.seen = seenList;
        xSemaphoreGive(mtx);
        if (!found && textFound) { foundAddr = textAddr; foundName = textName; found = true; }
        if (!found) {
            setState(seenCount ? "BLE: veo equipos, pero ninguno con KISS ni TNC2 (WebUI del tracker > Bluetooth: BLE y KISS)"
                               : "BLE: no aparece ningún tracker (¿Bluetooth apagado o clásico? ¿ya conectado a la app o al plotter? "
                                 "Solo admite un enlace BLE)");
            bleRetryAt = millis() + 5000;
            return;
        }
        if (!client) {
            client = NimBLEDevice::createClient();
            client->setConnectTimeout(6000);
        }
        setState("BLE: conectando con " + foundName);
        if (!client->connect(foundAddr)) { setState("BLE: no conecta con " + foundName); bleRetryAt = millis() + 5000; return; }
        bool text = false;
        if (!subscribe(text)) {
            setState("BLE: " + foundName + " no tiene KISS ni TNC2 por BLE (WebUI del tracker > Bluetooth)");
            client->disconnect();
            bleRetryAt = millis() + 10000;
            return;
        }
        xSemaphoreTake(mtx, portMAX_DELAY);
        st.ble = true;
        st.bleText = text;
        st.blePeer = (foundName.empty() ? foundAddr.toString() : foundName + " " + foundAddr.toString());
        st.state = "BLE: conectado a " + st.blePeer + (text ? " (TNC2)" : " (KISS)");
        xSemaphoreGive(mtx);
    }

    // ------------------------------------------------------------------------------------------------ TCP
    static WiFiClient tcp;
    static uint32_t tcpRetryAt = 0;

    // Candidatos: la dirección configurada, o los anunciados por mDNS (_kiss._tcp) y los equipos del punto de acceso
    static bool tcpConnect() {
        if (!Cfg.tcpHost.empty()) {
            setState("TCP: conectando con " + Cfg.tcpHost);
            if (tcp.connect(Cfg.tcpHost.c_str(), (uint16_t)Cfg.tcpPort, 3000)) { st.tcpPeer = Cfg.tcpHost + ":" + std::to_string(Cfg.tcpPort); return true; }
            return false;
        }
        setState("TCP: buscando el tracker (mDNS)");
        const int n = MDNS.queryService("kiss", "tcp");      // el mDNS lo arranca net.cpp
        for (int i = 0; i < n; i++) {
            const IPAddress ip = MDNS.address(i);
            const uint16_t port = MDNS.port(i);
            if (tcp.connect(ip, port, 2000)) { st.tcpPeer = std::string(MDNS.hostname(i).c_str()) + " " + ip.toString().c_str() + ":" + std::to_string(port); return true; }
        }
        // Equipos conectados al punto de acceso de la pasarela (el tracker como cliente de su WiFi)
        if (WiFi.getMode() & WIFI_MODE_AP) {
            esp_netif_t* ap = esp_netif_get_handle_from_ifkey("WIFI_AP_DEF");
            wifi_sta_list_t list;
            if (ap && esp_wifi_ap_get_sta_list(&list) == ESP_OK && list.num > 0) {
                esp_netif_pair_mac_ip_t pairs[ESP_WIFI_MAX_CONN_NUM];
                for (int i = 0; i < list.num && i < ESP_WIFI_MAX_CONN_NUM; i++) memcpy(pairs[i].mac, list.sta[i].mac, 6);
                if (esp_netif_dhcps_get_clients_by_mac(ap, list.num, pairs) == ESP_OK) {
                    for (int i = 0; i < list.num && i < ESP_WIFI_MAX_CONN_NUM; i++) {
                        if (!pairs[i].ip.addr) continue;
                        const IPAddress ip(pairs[i].ip.addr);
                        setState(std::string("TCP: probando ") + ip.toString().c_str());
                        if (tcp.connect(ip, (uint16_t)Cfg.tcpPort, 800)) { st.tcpPeer = std::string(ip.toString().c_str()) + ":" + std::to_string(Cfg.tcpPort); return true; }
                    }
                }
            }
        }
        return false;
    }

    static void tcpStep() {
        const bool want = (Cfg.src == "tcp" || Cfg.src == "both") && Cfg.wifiMode != 0;
        if (!want) { if (tcp.connected()) tcp.stop(); st.tcp = false; return; }
        if (tcp.connected()) {
            uint8_t b[256];
            int n;
            while ((n = tcp.available()) > 0) {
                n = tcp.read(b, sizeof b);
                if (n <= 0) break;
                push(b, (size_t)n);
            }
            return;
        }
        if (st.tcp) { st.tcp = false; setState("TCP: enlace perdido"); tcp.stop(); tcpRetryAt = millis() + 2000; }
        if ((int32_t)(millis() - tcpRetryAt) < 0) return;
        const bool net = (WiFi.getMode() & WIFI_MODE_AP) || WiFi.status() == WL_CONNECTED;
        if (!net) { tcpRetryAt = millis() + 2000; return; }
        if (tcpConnect()) {
            tcp.setNoDelay(true);
            xSemaphoreTake(mtx, portMAX_DELAY);
            st.tcp = true;
            st.state = "TCP: conectado a " + st.tcpPeer;
            xSemaphoreGive(mtx);
        } else {
            setState(Cfg.tcpHost.empty() ? "TCP: no encuentro el tracker (KISS WiFi activado en su WebUI?)" : "TCP: no conecta con " + Cfg.tcpHost);
            tcpRetryAt = millis() + 5000;
        }
    }

    static void task(void*) {
        for (;;) {
            if (restartAsked) {
                restartAsked = false;
                if (tcp.connected()) tcp.stop();
                if (client && client->isConnected()) client->disconnect();
                st.tcp = st.ble = false;
                bleRetryAt = tcpRetryAt = 0;
            }
            tcpStep();
            bleStep();
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }

    void begin() {
        mtx = xSemaphoreCreateMutex();
        sb = xStreamBufferCreate(8192, 1);
        sbText = xStreamBufferCreate(4096, 1);
        NimBLEDevice::init(Cfg.name);
        NimBLEDevice::setMTU(247);
        xTaskCreatePinnedToCore(task, "tracker", 8192, nullptr, 1, nullptr, 0);
    }

    size_t read(uint8_t* buf, size_t size) { return sb ? xStreamBufferReceive(sb, buf, size, 0) : 0; }
    size_t readText(uint8_t* buf, size_t size) { return sbText ? xStreamBufferReceive(sbText, buf, size, 0) : 0; }

    Status status() {
        Status s;
        if (!mtx) return s;
        xSemaphoreTake(mtx, portMAX_DELAY);
        s = st;
        xSemaphoreGive(mtx);
        return s;
    }

    void restart() { restartAsked = true; }
}
