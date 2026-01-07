#include "Meo3_Feature.h"

MeoFeature::MeoFeature() {}

void MeoFeature::attach(MeoMqttClient* transport, const char* deviceId) {
    _mqtt = transport;
    _deviceId = deviceId;
    if (_mqtt) {
        _mqtt->setMessageHandler(&MeoFeature::onRawMessage, this);
    }
}

bool MeoFeature::beginFeatureSubscribe(FeatureCallback cb, void* ctx) {
    if (!_mqtt || !_mqtt->isConnected() || !_deviceId) return false;
    _cb = cb;
    _cbCtx = ctx;

    // Subscribe to all feature invokes for this device
    String topic = String("meo/") + _deviceId + "/feature/+/invoke";
    return _mqtt->subscribe(topic.c_str());
}

bool MeoFeature::publishEvent(const char* eventName,
                              const char* const* keys,
                              const char* const* values,
                              uint8_t count) {
    if (!_mqtt || !_mqtt->isConnected() || !_deviceId) return false;

    String topic = String("meo/") + _deviceId + "/event/" + eventName;

    StaticJsonDocument<BUF_SIZE> doc;
    for (uint8_t i = 0; i < count; ++i) {
        doc[keys[i]] = values[i];
    }

    size_t len = serializeJson(doc, _buf, sizeof(_buf));
    if (len == 0) return false;

    return _mqtt->publish(topic.c_str(), (const uint8_t*)_buf, len, false);
}

bool MeoFeature::sendFeatureResponse(const char* featureName,
                                     bool success,
                                     const char* message) {
    if (!_mqtt || !_mqtt->isConnected() || !_deviceId) return false;

    String topic = String("meo/") + _deviceId + "/event/feature_response";

    StaticJsonDocument<BUF_SIZE> doc;
    doc["feature_name"] = featureName;
    doc["device_id"] = _deviceId;
    doc["success"]    = success;
    if (message) doc["message"] = message;

    size_t len = serializeJson(doc, _buf, sizeof(_buf));
    if (len == 0) return false;

    return _mqtt->publish(topic.c_str(), (const uint8_t*)_buf, len, false);
}

bool MeoFeature::publishStatus(const char* status) {
    if (!_mqtt || !_mqtt->isConnected() || !_deviceId) return false;
    String topic = String("meo/") + _deviceId + "/status";
    return _mqtt->publish(topic.c_str(), status, true);
}

void MeoFeature::onRawMessage(const char* topic, const uint8_t* payload, unsigned int length, void* ctx) {
    MeoFeature* self = reinterpret_cast<MeoFeature*>(ctx);
    if (!self) return;
    self->_dispatchFeatureInvoke(topic, payload, length);
}

void MeoFeature::_dispatchFeatureInvoke(const char* topic, const uint8_t* payload, unsigned int length) {
    if (!_cb || !_deviceId) return;

    // Expect "meo/{device_id}/feature/{featureName}/invoke"
    // Extract featureName between "/feature/" and "/invoke"
    const char* featureMarker = strstr(topic, "/feature/");
    const char* invokeMarker  = strstr(topic, "/invoke");
    if (!featureMarker || !invokeMarker || invokeMarker <= featureMarker) return;

    featureMarker += 9; // strlen("/feature/")
    size_t nameLen = (size_t)(invokeMarker - featureMarker);
    if (nameLen == 0 || nameLen >= 64) return; // safety cap

    char featureName[64];
    memcpy(featureName, featureMarker, nameLen);
    featureName[nameLen] = '\0';

    // Parse minimal JSON
    StaticJsonDocument<BUF_SIZE> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) return;

    // Build param arrays (keys/values) with a small cap
    const uint8_t MAX_PARAMS = 10;
    const char* keys[MAX_PARAMS];
    const char* values[MAX_PARAMS];
    uint8_t count = 0;

    if (doc.containsKey("params") && doc["params"].is<JsonObject>()) {
        for (JsonPair kv : doc["params"].as<JsonObject>()) {
            if (count >= MAX_PARAMS) break;
            keys[count]   = kv.key().c_str();
            values[count] = kv.value().as<const char*>();
            count++;
        }
    }

    const char* requestId = nullptr;
    if (doc.containsKey("request_id")) {
        requestId = doc["request_id"].as<const char*>();
    }

    _cb(featureName, _deviceId, keys, values, count, _cbCtx);
}