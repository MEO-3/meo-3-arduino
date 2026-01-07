#pragma once

#include <Arduino.h>
#include <NimBLEDevice.h>

/**
 * MeoBle: thin wrapper around NimBLE-Arduino to centralize BLE init,
 * server/service/characteristic creation, advertising, and per-characteristic
 * write callbacks via lightweight function pointers (no std::function).
 *
 * Designed to keep RAM/flash low and to be reused by future BLE features.
 */
class MeoBle {
public:
    // Function pointer type for write handlers
    typedef void (*OnWriteFn)(NimBLECharacteristic* ch, void* userCtx);

    MeoBle();

    // Initialize BLE stack with advertised device name
    // Returns false if BLE server creation fails
    bool begin(const char* deviceName);

    // Start/stop advertising
    void startAdvertising();
    void stopAdvertising();

    // Create a service by UUID
    NimBLEService* createService(const char* serviceUuid);

    // Create a characteristic on the given service with properties (NIMBLE_PROPERTY flags)
    NimBLECharacteristic* createCharacteristic(NimBLEService* svc,
                                               const char* charUuid,
                                               uint32_t properties);

    // Attach a lightweight write handler to a characteristic
    // Internally creates a tiny callback adapter that forwards to your function pointer.
    void setCharWriteHandler(NimBLECharacteristic* ch, OnWriteFn fn, void* userCtx);

    // Expose server in case advanced features need it later
    NimBLEServer* server() const;

private:
    NimBLEServer* _server = nullptr;

    // Internal adapter bridging NimBLECharacteristicCallbacks to function pointer
    class _Callbacks : public NimBLECharacteristicCallbacks {
    public:
        _Callbacks(OnWriteFn fn, void* ctx);
        void onWrite(NimBLECharacteristic* ch) override;
    private:
        OnWriteFn _fn;
        void*     _ctx;
    };
};