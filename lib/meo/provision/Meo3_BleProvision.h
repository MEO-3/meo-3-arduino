#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <string>
#include "../storage/Meo3_Storage.h"
#include "../ble/Meo3_Ble.h"
#include "../Meo3_Type.h" // MeoLogFunction

// Provisioning service UUID (stable across all devices)
#define MEO_BLE_PROV_SERV_UUID      "9f27f7f0-0000-1000-8000-00805f9b34fb" // Service UUID

// Characteristic UUIDs for provisioning
#define CH_UUID_WIFI_SSID           "9f27f7f1-0000-1000-8000-00805f9b34fb" // RW - WiFi SSID
#define CH_UUID_WIFI_PASS           "9f27f7f2-0000-1000-8000-00805f9b34fb" // WO - WiFi Password
#define CH_UUID_WIFI_LIST           "9f27f7f3-0000-1000-8000-00805f9b34fb" // RO - WiFi SSID List (read-only)

#define CH_UUID_USER_ID             "9f27f7f4-0000-1000-8000-00805f9b34fb" // RW - User ID

// Device info characteristics (use cloud compatible)
#define CH_UUID_PRODUCT_ID          "9f27f7f5-0000-1000-8000-00805f9b34fb" // RO - Product ID / Manufacturer
#define CH_UUID_BUILD_INFO          "9f27f7f6-0000-1000-8000-00805f9b34fb" // RO - Build Number / Firmware Version
#define CH_UUID_MAC_ADDR            "9f27f7f7-0000-1000-8000-00805f9b34fb" // RO - MAC Address

// Device info characteristics (use edge compatible)
#define CH_UUID_DEV_MODEL           "9f27f7f8-0000-1000-8000-00805f9b34fb" // RO - Device Model
#define CH_UUID_DEV_MANUF           "9f27f7f9-0000-1000-8000-00805f9b34fb" // RO - Device Manufacturer

// Additional provisioning characteristics used by the implementation
#define CH_UUID_TX_KEY              "9f27f7fa-0000-1000-8000-00805f9b34fb" // WO - Transmit Key (MQTT password)

class MeoBleProvision {
public:
    MeoBleProvision() = default;

    // Logging
    void setLogger(MeoLogFunction logger);
    void setDebugTags(const char* tagsCsv); // enables DEBUG for "PROV" when tag present

    // Initialize with BLE and storage; model/manuf taken from device config (recommended)
    bool begin(MeoBle* ble, MeoStorage* storage, const char* devModel, const char* devManufacturer);

    void setCloudCompatibleInfo(const char* productId, const char* buildInfo);

    // Start/stop advertising through base BLE
    void startAdvertising();
    void stopAdvertising();

    // Call regularly to refresh status and handle optional scheduled reboot
    void loop();

    // Update runtime status (short strings: "connected"/"disconnected"/"unknown")
    void setRuntimeStatus(const char* wifi, const char* mqtt);

    // Enable auto reboot when both SSID and PASS are written via BLE
    void setAutoRebootOnProvision(bool enable, uint32_t delayMs = 300);

private:
    MeoBle*            _ble      = nullptr;
    MeoStorage*        _storage  = nullptr;

    std::string        _devModel;
    std::string        _devManuf;
    std::string        _devProductIdStr;
    std::string        _buildInfoStr;
    std::string        _wifiSsidStr;

    NimBLEService*         _svc      = nullptr;
    NimBLECharacteristic*  _chSsid   = nullptr;
    NimBLECharacteristic*  _chPass   = nullptr;
    NimBLECharacteristic*  _chWifiList = nullptr;
    NimBLECharacteristic*  _chModel     = nullptr;
    NimBLECharacteristic*  _chManuf     = nullptr;
    NimBLECharacteristic*  _chProductId = nullptr;
    NimBLECharacteristic*  _chBuildInfo = nullptr;
    NimBLECharacteristic*  _chMacAddr   = nullptr;
    NimBLECharacteristic*  _chUserId    = nullptr;
    NimBLECharacteristic*  _chTxKey     = nullptr;

    const char*         _wifiStatus;
    const char*         _mqttStatus;
    char                _statusBuf[128];

    bool                _autoReboot = true;
    uint32_t            _rebootDelayMs = 300;
    bool                _ssidWritten = false;
    bool                _passWritten = false;
    bool                _rebootScheduled = false;
    uint32_t            _rebootAtMs = 0;

    // Logging
    MeoLogFunction _logger = nullptr;
    char           _debugTags[96] = {0};

    // Internal lifecycle
    bool _createServiceAndCharacteristics();
    void _bindWriteHandlers();
    void _loadInitialValues();
    void _updateStatus();
    void _scheduleRebootIfReady();
    bool _debugTagEnabled(const char* tag) const;

    // Write callbacks
    static void _onWriteStatic(NimBLECharacteristic* ch, void* ctx);
    void _onWrite(NimBLECharacteristic* ch);
};