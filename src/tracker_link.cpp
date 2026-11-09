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

    static StreamBufferHandle_t sb = nullptr;
    static SemaphoreHandle_t mtx = nullptr;
    static Status st;
    static volatile bool restartAsked = false;

    static void push(const uint8_t* d, size_t n) {
        if (!sb || !n) return;
        xStreamBufferSend(sb, d, n, 0);
        xSemaphoreTake(mtx, portMAX_DELAY);
        st.bytes += n;
        st.lastData = millis();
        xSemaphoreGive(mtx);
    }
    static void setState(const std::string& s) { xSemaphoreTake(mtx, portMAX_DELAY); st.state = s; xSemaphoreGive(mtx); }

    // ------------------------------------------------------------------------------------------------ BLE
    static NimBLEClient* client = nullptr;
    static volatile bool found = false;
    static NimBLEAddress foundAddr;
    static std::string foundName;
    static uint32_t bleRetryAt = 0;

    class ScanCb : public NimBLEScanCallbacks {
        void onResult(const NimBLEAdvertisedDevice* d) override {
            if (found) return;
            if (!Cfg.bleAddr.empty()) {
                if (strcasecmp(d->getAddress().toString().c_str(), Cfg.bleAddr.c_str()) != 0) return;
            } else {
                if (!d->isAdvertisingService(NimBLEUUID(KISS_SVC))) return;
                if (!Cfg.bleName.empty() && d->getName().find(Cfg.bleName) == std::string::npos) return;
            }
            foundAddr = d->getAddress();
            foundName = d->getName();
            found = true;
            NimBLEDevice::getScan()->stop();
        }
    } scanCb;

    static void onNotify(NimBLERemoteCharacteristic*, uint8_t* data, size_t len, bool) { push(data, len); }

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
        setState("BLE: buscando el tracker");
        NimBLEScan* scan = NimBLEDevice::getScan();
        scan->setScanCallbacks(&scanCb, false);
        scan->setActiveScan(true);
        scan->start(6000, false, true);
        const uint32_t t0 = millis();
        while (!found && scan->isScanning() && millis() - t0 < 6500) vTaskDelay(pdMS_TO_TICKS(50));
        scan->stop();
        if (!found) { setState("BLE: no aparece el tracker (¿conectado a la app?)"); bleRetryAt = millis() + 5000; return; }
        if (!client) {
            client = NimBLEDevice::createClient();
            client->setConnectTimeout(6000);
        }
        setState("BLE: conectando con " + foundName);
        if (!client->connect(foundAddr)) { setState("BLE: no conecta"); bleRetryAt = millis() + 5000; return; }
        NimBLERemoteService* svc = client->getService(KISS_SVC);
        NimBLERemoteCharacteristic* ch = svc ? svc->getCharacteristic(KISS_TX) : nullptr;
        if (!ch || !ch->canNotify() || !ch->subscribe(true, onNotify)) {
            setState("BLE: el tracker no tiene KISS por BLE (WebUI > Bluetooth)");
            client->disconnect();
            bleRetryAt = millis() + 10000;
            return;
        }
        xSemaphoreTake(mtx, portMAX_DELAY);
        st.ble = true;
        st.blePeer = (foundName.empty() ? foundAddr.toString() : foundName + " " + foundAddr.toString());
        st.state = "BLE: conectado a " + st.blePeer;
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
        NimBLEDevice::init(Cfg.name);
        NimBLEDevice::setMTU(247);
        xTaskCreatePinnedToCore(task, "tracker", 8192, nullptr, 1, nullptr, 0);
    }

    size_t read(uint8_t* buf, size_t size) { return sb ? xStreamBufferReceive(sb, buf, size, 0) : 0; }

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
