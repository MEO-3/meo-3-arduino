#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "../storage/Meo3_Storage.h"
#include "../ble/Meo3_Ble.h"

// Provisioning service UUID (stable across all devices)
#define MEO_BLE_PROV_SERV_UUID      "9f27f7f0-0000-1000-8000-00805f9b34fb" // Service UUID

// Characteristic UUIDs for provisioning
#define CH_UUID_WIFI_SSID           "9f27f7f1-0000-1000-8000-00805f9b34fb" // RW - WiFi SSID
#define CH_UUID_WIFI_PASS           "9f27f7f2-0000-1000-8000-00805f9b34fb" // WO - WiFi Password

// Device info characteristics
#define CH_UUID_DEV_MODEL           "9f27f7f3-0000-1000-8000-00805f9b34fb" // RO - Device Model
#define CH_UUID_DEV_MANUF           "9f27f7f4-0000-1000-8000-00805f9b34fb" // RO - Device Manufacturer
#define CH_UUID_DEV_ID              "9f27f7f6-0000-1000-8000-00805f9b34fb" // RW - Device ID
#define CH_UUID_TX_KEY              "9f27f7f7-0000-1000-8000-00805f9b34fb" // WO - Transmit Key (write-only)

// Provisioning progress / status (small JSON string)
#define CH_UUID_PROV_PROG           "9f27f7f5-0000-1000-8000-00805f9b34fb" // RO + NOTIFY

// All characteristics from f8 to fe are reserved for future use

/**
 * MeoBleProvision
 * - Uses MeoBle to host a Provisioning service with stable UUIDs
 * - Keeps RAM/flash low (no dynamic JSON, small static buffers)
 * - Persists SSID/PASS/Device ID/TxKey via MeoStorage
 * - Model/Manufacturer are read-only and sourced from device config you pass in
 */
class MeoBleProvision {
public:
    MeoBleProvision() = default;

    // Initialize with BLE and storage; model/manuf taken from device config (recommended)
    // Returns false if service creation fails
    bool begin(MeoBle* ble, MeoStorage* storage,
               const char* devModel, const char* devManufacturer);

    // Fallback initializer (will try to load model/manuf from storage if present)
    bool begin(MeoBle* ble, MeoStorage* storage);

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

    // Device static info provided by config (preferred)
    const char*        _devModel = nullptr;
    const char*        _devManuf = nullptr;

    NimBLEService*         _svc      = nullptr;
    NimBLECharacteristic*  _chSsid   = nullptr;  // RW
    NimBLECharacteristic*  _chPass   = nullptr;  // WO
    NimBLECharacteristic*  _chModel  = nullptr;  // RO
    NimBLECharacteristic*  _chManuf  = nullptr;  // RO
    NimBLECharacteristic*  _chDevId  = nullptr;  // RW
    NimBLECharacteristic*  _chTxKey  = nullptr;  // WO
    NimBLECharacteristic*  _chProg   = nullptr;  // R+Notify

    const char*         _wifiStatus = "unknown";
    const char*         _mqttStatus = "unknown";
    char                _statusBuf[128];

    bool                _autoReboot = true;
    uint32_t            _rebootDelayMs = 300;
    bool                _ssidWritten = false;
    bool                _passWritten = false;
    bool                _rebootScheduled = false;
    uint32_t            _rebootAtMs = 0;

    // Internal lifecycle
    bool _createServiceAndCharacteristics();
    void _bindWriteHandlers();
    void _loadInitialValues();
    void _updateStatus();
    void _scheduleRebootIfReady();
    void _log(const char* level, const char* msg) const;

    // Write callbacks
    static void _onWriteStatic(NimBLECharacteristic* ch, void* ctx);
    void _onWrite(NimBLECharacteristic* ch);
};