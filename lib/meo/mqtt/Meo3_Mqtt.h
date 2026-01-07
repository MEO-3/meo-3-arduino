#pragma once

#include "core/Meo3_Type.h"

class MeoMqttClient {
public:
    MeoMqttClient();

    void setLogger(MeoLogFunction logger);

    void configure(const char* host,
                   uint16_t port,
                   const String& deviceId,
                   const String& transmitKey,
                   MeoFeatureRegistry* featureRegistry);

    bool connect();
    void loop();
    bool isConnected() const;

    bool publishEvent(const char* eventName, const MeoEventPayload& payload);

    bool sendFeatureResponse(const MeoFeatureCall& call, bool success, const char* message);

private:
    String           _host;
    uint16_t         _port;
    String           _deviceId;
    String           _transmitKey;
    MeoFeatureRegistry* _features;
    MeoLogFunction   _logger;

    // underlying MQTT client object (to be defined in .cpp)
    // e.g., WiFiClient _wifiClient; PubSubClient _mqtt;

    void _onMqttMessage(char* topic, uint8_t* payload, unsigned int length);
    void _subscribeFeatureTopics();

    void _dispatchFeatureCall(const MeoFeatureCall& call);
};