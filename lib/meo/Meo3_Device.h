#pragma once

#include <Arduino.h>
#include "core/Meo3_Type.h"
#include "registration/Meo3_Registration.h"
#include "mqtt/Meo3_Mqtt.h"
#include "storage/Meo3_Storage.h"

class MeoDevice {
public:
    MeoDevice();

    // --- Basic configuration ---
    void beginWifi(const char* ssid, const char* password);
    void setGateway(const char* host, uint16_t registrationPort = 8901, uint16_t mqttPort = 1883);

    // Convenience: set gateway + start
    void begin(const char* host, uint16_t mqttPort = 1883);

    void setDeviceInfo(const char* label,
                       const char* model,
                       const char* manufacturer,
                       MeoConnectionType connectionType = MeoConnectionType::LAN);

    // --- Feature and event model configuration ---
    void addFeatureEvent(const char* eventName);
    void addFeatureMethod(const char* methodName, MeoFeatureCallback callback);

    // --- Lifecycle ---
    // Trigger registration (if no device_id/transmit_key) and then connect MQTT
    bool start();
    void loop();   // must be called often from Arduino loop()

    bool isRegistered() const;
    bool isMqttConnected() const;

    // --- Event publishing ---
    bool publishEvent(const char* eventName, const MeoEventPayload& payload);

    // --- Feature responses ---
    bool sendFeatureResponse(const MeoFeatureCall& call, bool success, const char* message = nullptr);

    // --- Debug / logging hooks (optional) ---
    void setLogger(MeoLogFunction logger);

private:
    // Internal state and helper objects
    String       _gatewayHost;
    uint16_t     _registrationPort;
    uint16_t     _mqttPort;

    String       _deviceId;
    String       _transmitKey;

    MeoDeviceInfo          _deviceInfo;
    MeoFeatureRegistry     _featureRegistry;
    MeoRegistrationClient  _registration;
    MeoMqttClient          _mqtt;
    MeoStorage             _storage;
    MeoLogFunction         _logger;

    bool _wifiReady;
    bool _registered;
    bool _mqttReady;

    void _log(const char* level, const char* msg);

    bool _loadCredentials(String& deviceIdOut, String& transmitKeyOut);
    bool _saveCredentials(const String& deviceId, const String& transmitKey);
};