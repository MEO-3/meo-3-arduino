#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include "../mqtt/Meo3_Mqtt.h"

/**
 * MeoFeature: device-level MQTT feature/event layer built on MeoMqtt.
 * - Subscribes to feature invokes: meo/{device_id}/feature/{feature}/invoke
 * - Publishes events:       meo/{device_id}/event/{event}
 * - Publishes status:       meo/{device_id}/status (online/offline LWT recommended in base)
 * - Sends feature responses: meo/{device_id}/event/feature_response
 *
 * Keeps RAM low: single reusable buffer and small JSON docs.
 */
class MeoFeature {
public:
    // Callback invoked on feature calls from the platform (no request_id)
    typedef void (*FeatureCallback)(
        const char* featureName,
        const char* deviceId,
        const char* const* keys,   // param keys
        const char* const* values, // param values
        uint8_t count,
        void* ctx
    );

    MeoFeature();

    // Wire base transport and device identity
    void attach(MeoMqttClient* transport, const char* deviceId);

    // Subscribe to invokes and set callback
    bool beginFeatureSubscribe(FeatureCallback cb, void* ctx);

    // Publish an event with params (arrays of keys/values)
    bool publishEvent(const char* eventName,
                      const char* const* keys,
                      const char* const* values,
                      uint8_t count);

    // Send a feature response (no request_id)
    bool sendFeatureResponse(const char* featureName,
                             bool success,
                             const char* message);

    // Publish status online/offline
    bool publishStatus(const char* status); // "online"/"offline"

    // Adapter: feed base MQTT messages into this layer
    static void onRawMessage(const char* topic, const uint8_t* payload, unsigned int length, void* ctx);

private:
    MeoMqttClient* _mqtt = nullptr;
    const char*    _deviceId = nullptr;

    // Reusable JSON buffer
    static constexpr size_t BUF_SIZE = 512;
    char _buf[BUF_SIZE];

    // Feature callback and context
    FeatureCallback _cb = nullptr;
    void*           _cbCtx = nullptr;

    void _dispatchFeatureInvoke(const char* topic, const uint8_t* payload, unsigned int length);
};