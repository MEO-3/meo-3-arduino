#include "Meo3_Device.h"
#include <WiFi.h>

MeoDevice::MeoDevice()
    : _registrationPort(8901),
      _mqttPort(1883),
      _logger(nullptr),
      _wifiReady(false),
      _registered(false),
      _mqttReady(false) {}

void MeoDevice::beginWifi(const char* ssid, const char* password) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);

    _log("INFO", "Connecting to WiFi...");
    unsigned long start = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - start) < 20000) {
        delay(500);
        _log("INFO", ".");
    }

    if (WiFi.status() == WL_CONNECTED) {
        _wifiReady = true;
        String msg = "WiFi connected, IP: " + WiFi.localIP().toString();
        _log("INFO", msg.c_str());
    } else {
        _wifiReady = false;
        _log("ERROR", "Failed to connect to WiFi");
    }

    _storage.begin();
}

void MeoDevice::setGateway(const char* host, uint16_t registrationPort, uint16_t mqttPort) {
    _gatewayHost = host;
    _registrationPort = registrationPort;
    _mqttPort = mqttPort;

    _registration.setGateway(host, registrationPort);
    _mqtt.configure(host, mqttPort, _deviceId, _transmitKey, &_featureRegistry);
}

void MeoDevice::begin(const char* host, uint16_t mqttPort) {
    setGateway(host, 8901, mqttPort);
}

void MeoDevice::setDeviceInfo(const char* label,
                              const char* model,
                              const char* manufacturer,
                              MeoConnectionType connectionType) {
    _deviceInfo.label = label;
    _deviceInfo.model = model;
    _deviceInfo.manufacturer = manufacturer;
    _deviceInfo.connectionType = connectionType;
}

void MeoDevice::addFeatureEvent(const char* eventName) {
    _featureRegistry.eventNames.push_back(String(eventName));
}

void MeoDevice::addFeatureMethod(const char* methodName, MeoFeatureCallback callback) {
    _featureRegistry.methodHandlers[String(methodName)] = callback;
}

bool MeoDevice::start() {
    if (!_wifiReady) {
        _log("ERROR", "WiFi not ready; call beginWifi() first");
        return false;
    }

    // Try to load credentials from storage
    if (_storage.loadCredentials(_deviceId, _transmitKey)) {
        _log("INFO", "Loaded existing credentials");
        _registered = true;
    } else {
        _log("INFO", "No stored credentials, registering with gateway...");
        if (!_registration.registerIfNeeded(_deviceInfo, _featureRegistry, _deviceId, _transmitKey)) {
            _log("ERROR", "Registration failed");
            return false;
        }
        _storage.saveCredentials(_deviceId, _transmitKey);
        _registered = true;
        _log("INFO", "Registered and saved credentials");
    }

    // Configure MQTT with final deviceId/transmitKey
    _mqtt.setLogger(_logger);
    _mqtt.configure(_gatewayHost.c_str(), _mqttPort, _deviceId, _transmitKey, &_featureRegistry);

    if (!_mqtt.connect()) {
        _log("ERROR", "Failed to connect to MQTT");
        _mqttReady = false;
        return false;
    }

    _mqttReady = true;
    _log("INFO", "MQTT connected");
    return true;
}

void MeoDevice::loop() {
    if (!_wifiReady) {
        return;
    }

    if (!_registered) {
        // Try registration lazily if start() was not called (or failed)
        start();
    }

    if (_registered && !_mqttReady) {
        // Try reconnect MQTT
        if (_mqtt.connect()) {
            _mqttReady = true;
            _log("INFO", "MQTT reconnected");
        }
    }

    if (_mqttReady) {
        _mqtt.loop();
    }
}

bool MeoDevice::isRegistered() const {
    return _registered;
}

bool MeoDevice::isMqttConnected() const {
    return _mqttReady && _mqtt.isConnected();
}

bool MeoDevice::publishEvent(const char* eventName, const MeoEventPayload& payload) {
    if (!_mqttReady) {
        _log("WARN", "MQTT not ready, cannot publish event");
        return false;
    }
    return _mqtt.publishEvent(eventName, payload);
}

bool MeoDevice::sendFeatureResponse(const MeoFeatureCall& call, bool success, const char* message) {
    if (!_mqttReady) {
        _log("WARN", "MQTT not ready, cannot send feature response");
        return false;
    }
    return _mqtt.sendFeatureResponse(call, success, message);
}

void MeoDevice::setLogger(MeoLogFunction logger) {
    _logger = logger;
    _registration.setLogger(logger);
    _mqtt.setLogger(logger);
}

void MeoDevice::_log(const char* level, const char* msg) {
    if (_logger) {
        _logger(level, msg);
    } else {
        // Fallback to Serial for debugging
        Serial.print("[");
        Serial.print(level);
        Serial.print("] ");
        Serial.println(msg);
    }
}