#include "Meo3_Device.h"
#include <ArduinoJson.h>

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

bool MeoDevice::addFeatureMethod(const char* name, MeoFeatureCallback cb, void* cbCtx) {
    if (!name || !*name || !cb || _methodCount >= MEO_MAX_FEATURE_METHODS) return false;
    _methodNames[_methodCount] = name;
    _methodHandlers[_methodCount] = cb;
    _methodCtx[_methodCount] = cbCtx;
    _methodCount++;
    return true;
}

bool MeoDevice::start() {
    // Init storage
    _storage.begin();

    // Init BLE and provisioning (model/manufacturer shown as RO fields)
    _ble.begin(_label ? _label : "MEO Device");
    _prov.begin(&_ble, &_storage, _model ? _model : "", _manufacturer ? _manufacturer : "");
    _prov.setAutoRebootOnProvision(true, 500);
    _prov.startAdvertising();

    // Load credentials provisioned via BLE/app
    _storage.loadString("device_id", _deviceId);
    _storage.loadString("transmit_key", _transmitKey);

    // If WiFi not provided upfront, try loading from storage (BLE wrote them)
    if (!_wifiSsid || !_wifiPass) {
        String ssid, pass;
        if (_storage.loadString("wifi_ssid", ssid) && _storage.loadString("wifi_pass", pass)) {
            _wifiSsid = ssid.c_str();
            _wifiPass = pass.c_str();
            // Connect WiFi
            WiFi.mode(WIFI_STA);
            WiFi.begin(ssid.c_str(), pass.c_str());
            uint32_t startMs = millis();
            while (WiFi.status() != WL_CONNECTED && (millis() - startMs) < 15000) {
                delay(100);
            }
            _wifiReady = (WiFi.status() == WL_CONNECTED);
        }
    }

    // Update BLE status
    _prov.setRuntimeStatus(_wifiReady ? "connected" : "disconnected",
                           "unknown");

    // Only proceed to MQTT if we have both WiFi and credentials
    if (!hasCredentials() || !_wifiReady) {
        // Stay in provisioning/awaiting connectivity
        return false;
    }

    // Connect MQTT and declare
    return _connectMqttAndDeclare();
}

void MeoDevice::loop() {
    _prov.loop();
    _mqtt.loop();

    // Update BLE status when connectivity changes
    static wl_status_t last = WL_IDLE_STATUS;
    wl_status_t now = WiFi.status();
    if (now != last || true) {
        _prov.setRuntimeStatus(now == WL_CONNECTED ? "connected" : "disconnected",
                               _mqtt.isConnected() ? "connected" : "disconnected");
        last = now;
    }

    // Lazy connect when WiFi and creds become ready
    if (!_mqtt.isConnected() && _wifiReady && hasCredentials()) {
        _connectMqttAndDeclare();
    }
}

bool MeoDevice::publishEvent(const char* eventName,
                             const char* const* keys,
                             const char* const* values,
                             uint8_t count) {
    return _feature.publishEvent(eventName, keys, values, count);
}

bool MeoDevice::sendFeatureResponse(const char* featureName,
                                    bool success,
                                    const char* message) {
    return _feature.sendFeatureResponse(featureName, success, message);
}

void MeoDevice::_updateBleStatus() {
    _prov.setRuntimeStatus(_wifiReady ? "connected" : "disconnected",
                           _mqtt.isConnected() ? "connected" : "disconnected");
}

bool MeoDevice::_connectMqttAndDeclare() {
    // Configure MQTT transport
    _mqtt.configure(_gatewayHost, _mqttPort);
    _mqtt.setCredentials(_deviceId.c_str(), _transmitKey.c_str());

    // LWT: status offline
    String willTopic = String("meo/") + _deviceId + "/status";
    _mqtt.setWill(willTopic.c_str(), "offline", 0, true);

    if (!_mqtt.connect()) {
        return false;
    }

    // Attach feature layer and subscribe
    _feature.attach(&_mqtt, _deviceId.c_str());
    _feature.beginFeatureSubscribe(
        [](const char* featureName, const char* deviceId,
           const char* const* keys, const char* const* values, uint8_t count, void* ctx) {
            reinterpret_cast<MeoDevice*>(ctx)->_onFeatureInvoke(featureName, deviceId, keys, values, count, ctx);
        },
        this
    );

    // Publish status and declare
    _feature.publishStatus("online");
    _publishDeclare();

    _updateBleStatus();
    return true;
}

void MeoDevice::_onFeatureInvoke(const char* featureName,
                                 const char* deviceId,
                                 const char* const* keys,
                                 const char* const* values,
                                 uint8_t count,
                                 void* ctx) {
    for (uint8_t i = 0; i < _methodCount; ++i) {
        if (strcmp(featureName, _methodNames[i]) == 0) {
            _methodHandlers[i](featureName, _deviceId.c_str(), keys, values, count, _methodCtx[i]);
            return;
        }
    }
    // Optional: respond with failure if no handler
    sendFeatureResponse(featureName, false, "No handler registered");
}

bool MeoDevice::_publishDeclare() {
    // meo/{device_id}/declare
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