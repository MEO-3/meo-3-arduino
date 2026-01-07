#include "Meo3_Device.h"
#include <ArduinoJson.h>
#include <string.h>

MeoDevice::MeoDevice() {}

void MeoDevice::setDeviceInfo(const char* label,
                              const char* model,
                              const char* manufacturer) {
    _label = label;
    _model = model;
    _manufacturer = manufacturer;
}

void MeoDevice::beginWifi(const char* ssid, const char* pass) {
    _wifiSsid = ssid;
    _wifiPass = pass;

    WiFi.mode(WIFI_STA);
    WiFi.begin(_wifiSsid, _wifiPass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
        delay(100);
    }
    _wifiReady = (WiFi.status() == WL_CONNECTED);
}

void MeoDevice::setGateway(const char* host, uint16_t mqttPort) {
    _gatewayHost = host;
    _mqttPort = mqttPort;
}

bool MeoDevice::addFeatureEvent(const char* name) {
    if (!name || !*name || _eventCount >= MEO_MAX_FEATURE_EVENTS) return false;
    _eventNames[_eventCount++] = name;
    return true;
}

bool MeoDevice::addFeatureMethod(const char* name, MeoFeatureCallback cb) {
    if (!name || !*name || !cb || _methodCount >= MEO_MAX_FEATURE_METHODS) return false;
    _methodNames[_methodCount]    = name;
    _methodHandlers[_methodCount] = cb;
    _methodCount++;
    return true;
}

bool MeoDevice::start() {
    // Storage
    if (!_storage.begin()) {
        return false;
    }

    // BLE + Provisioning (model/manufacturer read-only via BLE)
    _ble.begin(_label ? _label : "MEO Device");
    _prov.begin(&_ble, &_storage, _model ? _model : "", _manufacturer ? _manufacturer : "");
    _prov.setAutoRebootOnProvision(true, 500);
    _prov.setRuntimeStatus(_wifiReady ? "connected" : "disconnected", "disconnected");
    _prov.startAdvertising();

    // If WiFi not configured up-front, try load from storage (set via BLE)
    if (!_wifiReady && (!_wifiSsid || !_wifiPass)) {
        String ssid, pass;
        if (_storage.loadString("wifi_ssid", ssid) && _storage.loadString("wifi_pass", pass)) {
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid.c_str(), pass.c_str());
            uint32_t start = millis();
            while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
                delay(100);
            }
            _wifiReady = (WiFi.status() == WL_CONNECTED);
        }
    }

    // Load credentials (pre-provisioned via BLE/app)
    _storage.loadString("device_id", _deviceId);
    _storage.loadString("transmit_key", _transmitKey);

    // Only proceed if both WiFi and credentials are ready
    if (!_wifiReady || !hasCredentials()) {
        _prov.setRuntimeStatus(_wifiReady ? "connected" : "disconnected", "disconnected");
        return false;
    }

    // MQTT connect + declare
    return _connectMqttAndDeclare();
}

void MeoDevice::loop() {
    _prov.loop();
    _mqtt.loop();

    // Update BLE status on change
    static wl_status_t lastWifi = WL_IDLE_STATUS;
    wl_status_t nowWifi = WiFi.status();
    if (nowWifi != lastWifi || true) {
        _prov.setRuntimeStatus(nowWifi == WL_CONNECTED ? "connected" : "disconnected",
                               _mqtt.isConnected() ? "connected" : "disconnected");
        lastWifi = nowWifi;
    }

    // Lazy reconnect when WiFi + creds available
    if (!_mqtt.isConnected() && _wifiReady && hasCredentials()) {
        _connectMqttAndDeclare();
    }
}

bool MeoDevice::publishEvent(const char* eventName,
                             const char* const* keys,
                             const char* const* values,
                             uint8_t count) {
    if (!_mqtt.isConnected()) return false;
    String topic = String("meo/") + _deviceId + "/event/" + eventName;

    StaticJsonDocument<512> doc;
    for (uint8_t i = 0; i < count; ++i) {
        doc[keys[i]] = values[i];
    }

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) return false;

    return _mqtt.publish(topic.c_str(), (const uint8_t*)buf, len, false);
}

bool MeoDevice::publishEvent(const char* eventName, const MeoEventPayload& payload) {
    if (!_mqtt.isConnected()) return false;
    String topic = String("meo/") + _deviceId + "/event/" + eventName;

    StaticJsonDocument<512> doc;
    for (const auto& kv : payload) {
        doc[kv.first] = kv.second;
    }

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) return false;

    return _mqtt.publish(topic.c_str(), (const uint8_t*)buf, len, false);
}

bool MeoDevice::sendFeatureResponse(const char* featureName,
                                    bool success,
                                    const char* message) {
    if (!_mqtt.isConnected()) return false;
    String topic = String("meo/") + _deviceId + "/event/feature_response";

    StaticJsonDocument<512> doc;
    doc["feature_name"] = featureName;
    doc["device_id"]    = _deviceId.c_str();
    doc["success"]      = success;
    if (message) doc["message"] = message;

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) return false;

    return _mqtt.publish(topic.c_str(), (const uint8_t*)buf, len, false);
}

bool MeoDevice::sendFeatureResponse(const MeoFeatureCall& call,
                                    bool success,
                                    const char* message) {
    return sendFeatureResponse(call.featureName.c_str(), success, message);
}

void MeoDevice::_updateBleStatus() {
    const char* wifi = (WiFi.status() == WL_CONNECTED) ? "connected" : "disconnected";
    const char* mqtt = _mqtt.isConnected() ? "connected" : "disconnected";
    _prov.setRuntimeStatus(wifi, mqtt);
}

bool MeoDevice::_connectMqttAndDeclare() {
    // Configure transport (host/port + credentials)
    _mqtt.configure(_gatewayHost, _mqttPort);
    _mqtt.setCredentials(_deviceId.c_str(), _transmitKey.c_str());

    // LWT: status offline retained
    {
        String willTopic = String("meo/") + _deviceId + "/status";
        _mqtt.setWill(willTopic.c_str(), "offline", 0, true);
    }

    if (!_mqtt.connect()) {
        return false;
    }

    // Subscribe to feature invokes and wire handler
    {
        String topic = String("meo/") + _deviceId + "/feature/+/invoke";
        _mqtt.subscribe(topic.c_str());
        _mqtt.setMessageHandler(&_mqttThunk, this);
    }

    // Publish online status
    {
        String statusTopic = String("meo/") + _deviceId + "/status";
        _mqtt.publish(statusTopic.c_str(), "online", true);
    }

    // Declare
    _publishDeclare();

    _updateBleStatus();
    return true;
}

bool MeoDevice::_publishDeclare() {
    if (!_mqtt.isConnected()) return false;

    String topic = String("meo/") + _deviceId + "/declare";
    StaticJsonDocument<1024> doc;

    JsonObject info = doc.createNestedObject("device_info");
    info["label"]        = _label ? _label : "";
    info["model"]        = _model ? _model : "";
    info["manufacturer"] = _manufacturer ? _manufacturer : "";
    info["connection"]   = "LAN";

    JsonArray events = doc.createNestedArray("events");
    for (uint8_t i = 0; i < _eventCount; ++i) {
        events.add(_eventNames[i]);
    }

    JsonArray methods = doc.createNestedArray("methods");
    for (uint8_t i = 0; i < _methodCount; ++i) {
        methods.add(_methodNames[i]);
    }

    char buf[1024];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) return false;

    return _mqtt.publish(topic.c_str(), (const uint8_t*)buf, len, false);
}

// Static -> instance adapter
void MeoDevice::_mqttThunk(const char* topic, const uint8_t* payload, unsigned int length, void* ctx) {
    MeoDevice* self = reinterpret_cast<MeoDevice*>(ctx);
    if (!self) return;
    self->_dispatchInvoke(topic, payload, length);
}

void MeoDevice::_dispatchInvoke(const char* topic, const uint8_t* payload, unsigned int length) {
    // Expect "meo/{device_id}/feature/{featureName}/invoke"
    const char* featureMarker = strstr(topic, "/feature/");
    const char* invokeMarker  = strstr(topic, "/invoke");
    if (!featureMarker || !invokeMarker || invokeMarker <= featureMarker) return;

    featureMarker += 9; // strlen("/feature/")
    size_t nameLen = (size_t)(invokeMarker - featureMarker);
    if (nameLen == 0 || nameLen >= 64) return;

    char featureName[64];
    memcpy(featureName, featureMarker, nameLen);
    featureName[nameLen] = '\0';

    // Parse minimal JSON
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, payload, length);
    if (err) return;

    // Build MeoFeatureCall
    MeoFeatureCall call;
    call.deviceId    = _deviceId;
    call.featureName = featureName;

    if (doc.containsKey("params") && doc["params"].is<JsonObject>()) {
        for (JsonPair kv : doc["params"].as<JsonObject>()) {
            call.params[String(kv.key().c_str())] = String(kv.value().as<const char*>());
        }
    }

    // Dispatch to registered handler
    for (uint8_t i = 0; i < _methodCount; ++i) {
        if (strcmp(featureName, _methodNames[i]) == 0) {
            if (_methodHandlers[i]) {
                _methodHandlers[i](call);
            }
            return;
        }
    }

    // No handler: optionally negative response
    sendFeatureResponse(call, false, "No handler registered");
}