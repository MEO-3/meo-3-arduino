#include "Meo3_Device.h"
#include <ArduinoJson.h>
#include <string.h>
#include <stdarg.h>
#include <esp_system.h>

MeoDevice::MeoDevice() {}

void MeoDevice::setLogger(MeoLogFunction logger) {
    _logger = logger;
    // Forward logger to submodules
    _mqtt.setLogger(logger);
    _prov.setLogger(logger);
}

void MeoDevice::setDebugTags(const char* tagsCsv) {
    if (!tagsCsv) { _debugTags[0] = '\0'; return; }
    strncpy(_debugTags, tagsCsv, sizeof(_debugTags) - 1);
    _debugTags[sizeof(_debugTags) - 1] = '\0';
    // Forward to submodules
    _mqtt.setDebugTags(tagsCsv);
    _prov.setDebugTags(tagsCsv);
}

void MeoDevice::setDeviceInfo(const char* model,
                              const char* manufacturer) {
    _model = model;
    _manufacturer = manufacturer;
    if (_logger && _debugTagEnabled("DEVICE")) {
        _logf("DEBUG", "DEVICE", "Device info set: model=%s manufacturer=%s",
               model ? model : "", manufacturer ? manufacturer : "");
    }
}

void MeoDevice::beginWifi(const char* ssid, const char* pass) {
    _wifiSsid = ssid;
    _wifiPass = pass;

    _logf("INFO", "DEVICE", "Connecting WiFi SSID=%s", ssid ? ssid : "");
    WiFi.mode(WIFI_STA);
    WiFi.begin(_wifiSsid, _wifiPass);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 15000) {
        delay(100);
    }
    _wifiReady = (WiFi.status() == WL_CONNECTED);
    _logf(_wifiReady ? "INFO" : "ERROR", "DEVICE", "WiFi %s", _wifiReady ? "connected" : "failed");
}

void MeoDevice::setGateway(const char* host, uint16_t mqttPort) {
    _gatewayHost = host;
    _mqttPort = mqttPort;
    _logf("INFO", "DEVICE", "Gateway set: %s:%u", host ? host : "", mqttPort);
}

bool MeoDevice::addFeatureEvent(const char* name) {
    if (!name || !*name || _eventCount >= MEO_MAX_FEATURE_EVENTS) return false;
    _eventNames[_eventCount++] = name;
    if (_logger && _debugTagEnabled("DEVICE")) {
        _logf("DEBUG", "DEVICE", "Feature event added: %s", name);
    }
    return true;
}

bool MeoDevice::addFeatureMethod(const char* name, MeoFeatureCallback cb) {
    if (!name || !*name || !cb || _methodCount >= MEO_MAX_FEATURE_METHODS) return false;
    _methodNames[_methodCount]    = name;
    _methodHandlers[_methodCount] = cb;
    _methodCount++;
    if (_logger && _debugTagEnabled("DEVICE")) {
        _logf("DEBUG", "DEVICE", "Feature method added: %s", name);
    }
    return true;
}

bool MeoDevice::start() {
    // Storage
    if (!_storage.begin()) {
        _log("ERROR", "DEVICE", "Storage init failed");
        return false;
    }

    // BLE + Provisioning (model/manufacturer read-only via BLE)
    _ble.begin(_model ? _model : "MEO Device");
    _prov.setLogger(_logger);
    _prov.setDebugTags(_debugTags);
    _prov.begin(&_ble, &_storage, _model ? _model : "", _manufacturer ? _manufacturer : "");
    _prov.setAutoRebootOnProvision(true, 500);
    _prov.setRuntimeStatus(_wifiReady ? "connected" : "disconnected", "disconnected");
    _prov.startAdvertising();
    _log("INFO", "DEVICE", "BLE provisioning started");

    // If WiFi not configured up-front, try load from storage (set via BLE)
    if (!_wifiReady && (!_wifiSsid || !_wifiPass)) {
        String ssid, pass;
        if (_storage.loadString("wifi_ssid", ssid) && _storage.loadString("wifi_pass", pass)) {
            _logf("INFO", "DEVICE", "WiFi creds loaded from storage: SSID=%s", ssid.c_str());
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
    _storage.loadString("tx_key", _transmitKey);
    // Load optional user id (top-level MQTT namespace)
    _storage.loadString("user_id", _userId);
    // device_id from ESP MAC (Ethernet MAC preferred)
    uint8_t mac_raw[6] = {0};
    esp_err_t r = esp_read_mac(mac_raw, ESP_MAC_ETH);
    if (r != ESP_OK) {
        esp_read_mac(mac_raw, ESP_MAC_WIFI_STA);
    }
    char macbuf[13]; // 12 hex chars + null
    snprintf(macbuf, sizeof(macbuf), "%02X%02X%02X%02X%02X%02X",
                mac_raw[0], mac_raw[1], mac_raw[2], mac_raw[3], mac_raw[4], mac_raw[5]);
    _deviceId = String(macbuf);
    if (_logger && _debugTagEnabled("DEVICE")) {
        _logf("DEBUG", "DEVICE", "Generated device_id from MAC: %s", macbuf);
    }
    _logf("INFO", "DEVICE", "Credentials %s", hasCredentials() ? "present" : "missing");

    // Only proceed if both WiFi and credentials are ready
    if (!_wifiReady || !hasCredentials()) {
        _prov.setRuntimeStatus(_wifiReady ? "connected" : "disconnected", "disconnected");
        _log("WARN", "DEVICE", "Waiting for WiFi/credentials via BLE provisioning");
        return false;
    }

    // PATCH: stop BLE advertising once WiFi is connected (if BLE was already advertising)
    // if (_wifiReady) {
    //     _prov.stopAdvertising();
    //     _log("INFO", "DEVICE", "WiFi connected; stopped BLE advertising");
    // }

    // MQTT connect + declare
    return _connectMqttAndDeclare();
}

void MeoDevice::loop() {
    // _prov.loop(); // BLE provisioning loop unused because after mqtt connect success we stop advertising
    _mqtt.loop();

    // Update BLE status on change
    static wl_status_t lastWifi = WL_IDLE_STATUS;
    wl_status_t nowWifi = WiFi.status();
    if (nowWifi != lastWifi) {
        _prov.setRuntimeStatus(nowWifi == WL_CONNECTED ? "connected" : "disconnected",
                               _mqtt.isConnected() ? "connected" : "disconnected");
        lastWifi = nowWifi;
        if (_logger && _debugTagEnabled("DEVICE")) {
            _logf("DEBUG", "DEVICE", "Status WiFi=%s MQTT=%s",
                  nowWifi == WL_CONNECTED ? "connected" : "disconnected",
                  _mqtt.isConnected() ? "connected" : "disconnected");
        }
    }

    // Lazy reconnect when WiFi + creds available
    if (!_mqtt.isConnected() && _wifiReady && hasCredentials()) {
        _log("WARN", "DEVICE", "MQTT disconnected; attempting reconnect");
        _connectMqttAndDeclare();
    }
}

bool MeoDevice::publishEvent(const char* eventName,
                             const char* const* keys,
                             const char* const* values,
                             uint8_t count) {
    if (!_mqtt.isConnected()) return false;
    String base = String("meo/");
    if (_userId.length()) base += _userId + "/";
    String topic = base + _deviceId + "/event/" + eventName;

    StaticJsonDocument<512> doc;
    for (uint8_t i = 0; i < count; ++i) {
        doc[keys[i]] = values[i];
    }

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) return false;

    if (_logger && _debugTagEnabled("DEVICE")) {
        _logf("DEBUG", "DEVICE", "Publish event %s len=%u", eventName, (unsigned)len);
    }
    return _mqtt.publish(topic.c_str(), (const uint8_t*)buf, len, false);
}

bool MeoDevice::publishEvent(const char* eventName, const MeoEventPayload& payload) {
    if (!_mqtt.isConnected()) return false;
    String base = String("meo/");
    if (_userId.length()) base += _userId + "/";
    String topic = base + _deviceId + "/event/" + eventName;

    StaticJsonDocument<512> doc;
    for (const auto& kv : payload) {
        doc[kv.first] = kv.second;
    }

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) return false;

    if (_logger && _debugTagEnabled("DEVICE")) {
        _logf("DEBUG", "DEVICE", "Publish event %s len=%u", eventName, (unsigned)len);
    }
    return _mqtt.publish(topic.c_str(), (const uint8_t*)buf, len, false);
}

bool MeoDevice::sendFeatureResponse(const char* featureName,
                                    bool success,
                                    const char* message) {
    if (!_mqtt.isConnected()) return false;
    String base = String("meo/");
    if (_userId.length()) base += _userId + "/";
    String topic = base + _deviceId + "/event/feature_response";

    StaticJsonDocument<512> doc;
    doc["feature_name"] = featureName;
    doc["device_id"]    = _deviceId.c_str();
    doc["success"]      = success;
    if (message) doc["message"] = message;

    char buf[512];
    size_t len = serializeJson(doc, buf, sizeof(buf));
    if (len == 0) return false;

    if (_logger && _debugTagEnabled("DEVICE")) {
        _logf("DEBUG", "DEVICE", "Publish feature_response for %s", featureName);
    }
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
    _mqtt.setLogger(_logger);
    _mqtt.setDebugTags(_debugTags);

    // LWT: status offline retained
    {
        String base = String("meo/");
        if (_userId.length()) base += _userId + "/";
        String willTopic = base + _deviceId + "/status";
        _mqtt.setWill(willTopic.c_str(), "offline", 0, false);
    }

    if (!_mqtt.connect()) {
        _log("ERROR", "DEVICE", "MQTT connect failed");
        return false;
    }
    _log("INFO", "DEVICE", "MQTT connected");

    // Subscribe to feature invokes and wire handler
    {
        String base = String("meo/");
        if (_userId.length()) base += _userId + "/";
        String topic = base + _deviceId + "/feature/+/invoke";
        _mqtt.subscribe(topic.c_str());
        _mqtt.setMessageHandler(&_mqttThunk, this);
        if (_logger && _debugTagEnabled("DEVICE")) {
            _logf("DEBUG", "DEVICE", "Subscribed to %s", topic.c_str());
        }
    }

    // Publish online status
    {
        String base = String("meo/");
        if (_userId.length()) base += _userId + "/";
        String statusTopic = base + _deviceId + "/status";
        _mqtt.publish(statusTopic.c_str(), "online", true);
    }

    // Declare
    _publishDeclare();

    _updateBleStatus();
    return true;
}

bool MeoDevice::_publishDeclare() {
    if (!_mqtt.isConnected()) return false;

    String base = String("meo/");
    if (_userId.length()) base += _userId + "/";
    String topic = base + _deviceId + "/declare";
    StaticJsonDocument<1024> doc;

    JsonObject info = doc.createNestedObject("device_info");
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

    if (_logger && _debugTagEnabled("DEVICE")) {
        _logf("DEBUG", "DEVICE", "Publish declare len=%u", (unsigned)len);
    }
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
            if (_logger && _debugTagEnabled("DEVICE")) {
                _logf("DEBUG", "DEVICE", "Invoke %s with %u params", featureName, (unsigned)call.params.size());
            }
            if (_methodHandlers[i]) {
                _methodHandlers[i](call);
            }
            return;
        }
    }

    // No handler: optionally negative response
    sendFeatureResponse(call, false, "No handler registered");
}

bool MeoDevice::_debugTagEnabled(const char* tag) const {
    if (!_debugTags[0]) return false; // no debug tags -> no DEBUG logs
    // simple substring match in CSV
    const char* p = strstr(_debugTags, tag);
    if (!p) return false;
    // ensure token boundary (start or preceded by comma) and followed by comma or end
    bool leftOk  = (p == _debugTags) || (*(p - 1) == ',');
    const char* end = p + strlen(tag);
    bool rightOk = (*end == '\0') || (*end == ',');
    return leftOk && rightOk;
}

void MeoDevice::_log(const char* level, const char* tag, const char* msg) const {
    if (!_logger) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "[%s] %s", tag ? tag : "DEVICE", msg ? msg : "");
    _logger(level, buf);
}

void MeoDevice::_logf(const char* level, const char* tag, const char* fmt, ...) const {
    if (!_logger) return;
    char msg[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    _log(level, tag, msg);
}