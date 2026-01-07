#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include "storage/Meo3_Storage.h"
#include "ble/Meo3_Ble.h"
#include "provision/Meo3_BleProvision.h"
#include "mqtt/Meo3_Mqtt.h"
#include "feature/Meo3_Feature.h"

// Lightweight fixed-capacity registry
#ifndef MEO_MAX_FEATURE_EVENTS
#define MEO_MAX_FEATURE_EVENTS 8
#endif
#ifndef MEO_MAX_FEATURE_METHODS
#define MEO_MAX_FEATURE_METHODS 8
#endif

typedef void (*MeoFeatureCallback)(
    const char* featureName,
    const char* deviceId,
    const char* const* keys,
    const char* const* values,
    uint8_t count,
    void* ctx
);

class MeoDevice {
public:
    MeoDevice();

    // Device info for declare and BLE RO fields
    void setDeviceInfo(const char* label,
                       const char* model,
                       const char* manufacturer);

    // Optional: provide WiFi upfront; otherwise BLE provisioning can set it
    void beginWifi(const char* ssid, const char* pass);

    // MQTT broker (gateway)
    void setGateway(const char* host, uint16_t mqttPort = 1883);

    // Features
    bool addFeatureEvent(const char* name);
    bool addFeatureMethod(const char* name, MeoFeatureCallback cb, void* cbCtx = nullptr);

    // Lifecycle
    bool start();    // Load creds; BLE provisioning if needed; MQTT connect; declare
    void loop();     // BLE status, MQTT loop, lazy reconnect

    // Publish helpers
    bool publishEvent(const char* eventName,
                      const char* const* keys,
                      const char* const* values,
                      uint8_t count);

    bool sendFeatureResponse(const char* featureName,
                             bool success,
                             const char* message);

    // Status
    bool hasCredentials() const { return _deviceId.length() && _transmitKey.length(); }
    bool isMqttConnected() { return _mqtt.isConnected(); }

private:
    // Config
    const char* _label = nullptr;
    const char* _model = nullptr;
    const char* _manufacturer = nullptr;

    const char* _wifiSsid = nullptr;
    const char* _wifiPass = nullptr;
    const char* _gatewayHost = "meo-open-service.local";
    uint16_t    _mqttPort = 1883;

    // Identity (from BLE/app)
    String  _deviceId;
    String  _transmitKey;

    // Registries
    const char* _eventNames[MEO_MAX_FEATURE_EVENTS];
    uint8_t     _eventCount = 0;

    const char* _methodNames[MEO_MAX_FEATURE_METHODS];
    MeoFeatureCallback _methodHandlers[MEO_MAX_FEATURE_METHODS];
    void*       _methodCtx[MEO_MAX_FEATURE_METHODS];
    uint8_t     _methodCount = 0;

    // Modules
    MeoStorage      _storage;
    MeoBle          _ble;
    MeoBleProvision _prov;
    MeoMqttClient         _mqtt;
    MeoFeature      _feature;

    // State
    bool _wifiReady = false;

    // Internals
    void _updateBleStatus();
    void _onFeatureInvoke(const char* featureName,
                          const char* deviceId,
                          const char* const* keys,
                          const char* const* values,
                          uint8_t count,
                          void* ctx);

    bool _connectMqttAndDeclare();
    bool _publishDeclare();
};