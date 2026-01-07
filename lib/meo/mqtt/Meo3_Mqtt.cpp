#include "Meo3_Mqtt.h"
#include <WiFi.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

static WiFiClient    _meoWifiClient;
static PubSubClient  _meoPubSub(_meoWifiClient);

MeoMqttClient::MeoMqttClient()
    : _port(1883),
      _features(nullptr),
      _logger(nullptr) {}

void MeoMqttClient::setLogger(MeoLogFunction logger) {
    _logger = logger;
}

void MeoMqttClient::configure(const char* host,
                              uint16_t port,
                              const String& deviceId,
                              const String& transmitKey,
                              MeoFeatureRegistry* featureRegistry) {
    _host = host;
    _port = port;
    _deviceId = deviceId;
    _transmitKey = transmitKey;
    _features = featureRegistry;

    _meoPubSub.setServer(_host.c_str(), _port);
    _meoPubSub.setCallback(
        [this](char* topic, uint8_t* payload, unsigned int length) {
            this->_onMqttMessage(topic, payload, length);
        }
    );
}

bool MeoMqttClient::connect() {
    if (WiFi.status() != WL_CONNECTED) {
        if (_logger) _logger("ERROR", "WiFi not connected, cannot connect MQTT");
        return false;
    }

    if (_meoPubSub.connected()) {
        return true;
    }

    String clientId = "meo-" + _deviceId;
    if (_logger) {
        String msg = "Connecting MQTT as " + clientId + " to " + _host + ":" + String(_port);
        _logger("INFO", msg.c_str());
    }

    // Use deviceId/transmitKey as MQTT credentials
    bool ok = _meoPubSub.connect(clientId.c_str(),
                                 _deviceId.c_str(),      // username
                                 _transmitKey.c_str());  // password

    if (!ok) {
        if (_logger) _logger("ERROR", "MQTT connection failed");
        return false;
    }

    _subscribeFeatureTopics();
    if (_logger) _logger("INFO", "MQTT connected and subscribed");
    return true;
}

void MeoMqttClient::loop() {
    if (_meoPubSub.connected()) {
        _meoPubSub.loop();
    }
}

bool MeoMqttClient::isConnected() const {
    return _meoPubSub.connected();
}

bool MeoMqttClient::publishEvent(const char* eventName, const MeoEventPayload& payload) {
    if (!_meoPubSub.connected()) {
        if (_logger) _logger("WARN", "MQTT not connected, cannot publish event");
        return false;
    }

    String topic = "meo/" + _deviceId + "/event" + "/" + String(eventName);

    StaticJsonDocument<512> doc;
    for (const auto& kv : payload) {
        doc[kv.first] = kv.second;
    }

    char buffer[512];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len == 0) {
        if (_logger) _logger("ERROR", "Failed to serialize event JSON");
        return false;
    }

    if (_logger) {
        String msg = "Publishing event to " + topic + ": " + String(buffer);
        _logger("DEBUG", msg.c_str());
    }

    return _meoPubSub.publish(topic.c_str(), buffer, len);
}

bool MeoMqttClient::sendFeatureResponse(const MeoFeatureCall& call, bool success, const char* message) {
    if (!_meoPubSub.connected()) {
        if (_logger) _logger("WARN", "MQTT not connected, cannot send feature response");
        return false;
    }

    // You can define a dedicated response topic; here we'll re-use "event" with a special type
    String topic = "meo/" + _deviceId + "/event/feature_response";

    StaticJsonDocument<512> doc;
    doc["feature_name"] = call.featureName;
    doc["request_id"]  = call.requestId;
    doc["device_id"]   = call.deviceId;
    doc["success"]     = success;
    if (message) {
        doc["message"] = message;
    }

    char buffer[512];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len == 0) {
        if (_logger) _logger("ERROR", "Failed to serialize feature response JSON");
        return false;
    }

    return _meoPubSub.publish(topic.c_str(), buffer, len);
}

void MeoMqttClient::_subscribeFeatureTopics() {
    if (!_meoPubSub.connected()) return;

    // Subscribe to all feature invocations for this device
    String topic = "meo/" + _deviceId + "/feature/+/invoke";
    _meoPubSub.subscribe(topic.c_str());

    if (_logger) {
        String msg = "Subscribed to feature topics: " + topic;
        _logger("DEBUG", msg.c_str());
    }
}

void MeoMqttClient::_onMqttMessage(char* topic, uint8_t* payload, unsigned int length) {
    if (_logger) {
        String msg = "MQTT message on ";
        msg += topic;
        _logger("DEBUG", msg.c_str());
    }

    // Expect topic: meo/{deviceId}/feature/{featureName}/invoke
    String t(topic);
    // Split by '/'
    // [0]=meo, [1]=deviceId, [2]=feature, [3]=featureName, [4]=invoke
    int firstSlash = t.indexOf('/');
    int secondSlash = t.indexOf('/', firstSlash + 1);
    int thirdSlash = t.indexOf('/', secondSlash + 1);
    int fourthSlash = t.indexOf('/', thirdSlash + 1);

    if (firstSlash < 0 || secondSlash < 0 || thirdSlash < 0 || fourthSlash < 0) {
        if (_logger) _logger("WARN", "Unexpected feature topic format");
        return;
    }

    String deviceId = t.substring(firstSlash + 1, secondSlash);
    String segment2 = t.substring(secondSlash + 1, thirdSlash);  // should be "feature"
    String featureName = t.substring(thirdSlash + 1, fourthSlash);
    String action = t.substring(fourthSlash + 1);                 // should be "invoke"

    if (segment2 != "feature" || action != "invoke") {
        if (_logger) _logger("WARN", "Topic is not feature invoke");
        return;
    }

    // Parse JSON payload
    // String json;
    // json.reserve(length + 1);
    // for (unsigned int i = 0; i < length; i++) {
    //     json += static_cast<char>(payload[i]);
    // }

    char json[512];
    for(unsigned int i = 0; i < length && i < sizeof(json) - 1; i++) {
        json[i] = static_cast<char>(payload[i]);
    }
    json[length < sizeof(json) ? length : sizeof(json) - 1] = '\0';
    // if (_logger) {
    //     String msg = "Feature invoke JSON: " + json;
    //     _logger("DEBUG", msg.c_str());
    // }
    if(_logger) {
        String msg = "Feature invoke JSON: ";
        msg += String(json);
        _logger("DEBUG", msg.c_str());
    }
    StaticJsonDocument<512> doc;
    try {
        DeserializationError err = deserializeJson(doc, json);
        if (err) {
            if (_logger) {
                String msg = "Failed to parse feature JSON: ";
                msg += err.c_str();
                _logger("ERROR", msg.c_str());
            }
            return;
        }
    } catch (...) {
        if (_logger) _logger("ERROR", "Exception during JSON deserialization");
        return;
    }

    // if (_logger) {
    //     String msg = "Parsed feature JSON successfully";
    //     _logger("DEBUG", msg.c_str());
    // }

    MeoFeatureCall call;
    call.deviceId = deviceId;
    call.featureName = featureName;
    call.requestId = doc["request_id"] | "";

    // if (_logger) {
    //     String msg = "Invoking feature: " + call.featureName + " (request_id: " + call.requestId + ")";
    //     _logger("INFO", msg.c_str());
    // }

    JsonObject params = doc["params"];
    if (!params.isNull()) {
        for (JsonPair kv : params) {
            
            if (_logger) {
                String msg = "  Param: ";
                msg += String(kv.key().c_str());
                msg += " = ";
                msg += String(kv.value().as<const char*>());
                _logger("DEBUG", msg.c_str());
            }

            call.params[String(kv.key().c_str())] = String(kv.value().as<const char*>());
        }
    }

    _dispatchFeatureCall(call);
}

void MeoMqttClient::_dispatchFeatureCall(const MeoFeatureCall& call) {
    if (!_features) return;

    auto it = _features->methodHandlers.find(call.featureName);
    if (it == _features->methodHandlers.end()) {
        if (_logger) {
            String msg = "No handler for feature: ";
            msg += call.featureName;
            _logger("WARN", msg.c_str());
        }
        return;
    }

    MeoFeatureCallback cb = it->second;
    if (cb) {
        cb(call);
    }
}