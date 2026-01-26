#pragma once

#include <Arduino.h>
#include <WiFi.h>
#include <string>

#include "Meo3_Type.h"   // MeoFeatureCall, MeoEventPayload, MeoFeatureCallback, MeoConnectionType, MeoLogFunction
#include "storage/Meo3_Storage.h"
#include "ble/Meo3_Ble.h"
#include "provision/Meo3_BleProvision.h"
#include "mqtt/Meo3_Mqtt.h"              // MeoMqttClient transport

#ifndef MEO_MAX_FEATURE_EVENTS
#define MEO_MAX_FEATURE_EVENTS 8
#endif
#ifndef MEO_MAX_FEATURE_METHODS
#define MEO_MAX_FEATURE_METHODS 8
#endif

class MeoDevice {
public:
    MeoDevice();

    // Logging
    void setLogger(MeoLogFunction logger);
    // CSV of tags to enable DEBUG logs for (e.g. "DEVICE,MQTT,PROV")
    void setDebugTags(const char* tagsCsv);

    // Device info for declare and BLE RO fields
    void setDeviceInfo(const char* model, const char* manufacturer);

    // Optional: provide WiFi upfront; otherwise BLE provisioning can set it
    void beginWifi(const char* ssid, const char* pass);

    // MQTT broker (gateway)
    void setGateway(const char* host, uint16_t mqttPort = 1883);

    void setCloudCompatibleInfo(const char* productId, const char* buildInfo);

    // Features (simple API)
    bool addFeatureEvent(const char* name);
    bool addFeatureMethod(const char* name, MeoFeatureCallback cb);

    // Lifecycle
    bool start();    // Load creds; BLE provisioning if needed; MQTT connect; declare
    void loop();     // BLE status, MQTT loop, lazy reconnect

    // Publish helpers
    bool publishEvent(const char* eventName,
                      const char* const* keys,
                      const char* const* values,
                      uint8_t count);
    bool publishEvent(const char* eventName, const MeoEventPayload& payload);

    // Send feature response
    bool sendFeatureResponse(const char* featureName,
                             bool success,
                             const char* message);
    bool sendFeatureResponse(const MeoFeatureCall& call,
                             bool success,
                             const char* message);

    // Status
    bool hasCredentials() const { return _deviceId.length() && _transmitKey.length(); }
    bool isMqttConnected() { return _mqtt.isConnected(); }

private:
    // Config
    const char* _model;
    const char* _manufacturer;

    const char* _wifiSsid;
    const char* _wifiPass;
    const char* _gatewayHost;
    uint16_t    _mqttPort = 1883;

    // Identity (from BLE/app)
    std::string  _deviceId;
    std::string  _userId;
    std::string  _transmitKey;

    // Registries (simple arrays)
    const char* _eventNames[MEO_MAX_FEATURE_EVENTS];
    uint8_t     _eventCount = 0;

    const char*        _methodNames[MEO_MAX_FEATURE_METHODS];
    MeoFeatureCallback _methodHandlers[MEO_MAX_FEATURE_METHODS];
    uint8_t            _methodCount = 0;

    // Modules
    MeoStorage      _storage;
    MeoBle          _ble;
    MeoBleProvision _prov;
    MeoMqttClient   _mqtt;

    // State
    bool _wifiReady = false;

    // Logging
    MeoLogFunction _logger = nullptr;
    char           _debugTags[96] = {0}; // CSV list of enabled DEBUG tags

    // Internals
    void _updateBleStatus();
    bool _connectMqttAndDeclare();
    bool _publishDeclare();

    // MQTT message adapter: parse invoke and dispatch MeoFeatureCall
    static void _mqttThunk(const char* topic, const uint8_t* payload, unsigned int length, void* ctx);
    void _dispatchInvoke(const char* topic, const uint8_t* payload, unsigned int length);

    // Logging helpers
    bool _debugTagEnabled(const char* tag) const;
    void _log(const char* level, const char* tag, const char* msg) const;
    void _logf(const char* level, const char* tag, const char* fmt, ...) const;
};