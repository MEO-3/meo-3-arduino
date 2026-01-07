#include "Meo3_Mqtt.h"
#include <WiFi.h>
#include <stdarg.h>

MeoMqttClient* MeoMqttClient::_self = nullptr;

MeoMqttClient::MeoMqttClient()
: _mqtt(_wifiClient) {
    _self = this; // one active instance
    _mqtt.setBufferSize(1024);
    _mqtt.setKeepAlive(15);
    _mqtt.setSocketTimeout(15);
    _mqtt.setCallback(&MeoMqttClient::_pubsubThunk);
}

void MeoMqttClient::setLogger(MeoLogFunction logger) {
    _logger = logger;
}

void MeoMqttClient::setDebugTags(const char* tagsCsv) {
    if (!tagsCsv) { _debugTags[0] = '\0'; return; }
    strncpy(_debugTags, tagsCsv, sizeof(_debugTags) - 1);
    _debugTags[sizeof(_debugTags) - 1] = '\0';
}

void MeoMqttClient::configure(const char* host, uint16_t port) {
    _host = host;
    _port = port;
    _mqtt.setServer(_host, _port);
    if (_logger && _debugTagEnabled("MQTT")) {
        _logf("DEBUG", "MQTT", "Configured broker %s:%u", host ? host : "", port);
    }
}

void MeoMqttClient::setCredentials(const char* deviceId, const char* transmitKey) {
    _deviceId = deviceId;
    _txKey = transmitKey;
    if (_logger && _debugTagEnabled("MQTT")) {
        _logf("DEBUG", "MQTT", "Credentials set: deviceId=%s", deviceId ? deviceId : "");
    }
}

void MeoMqttClient::setBufferSize(uint16_t bytes) {
    _mqtt.setBufferSize(bytes);
}
void MeoMqttClient::setKeepAlive(uint16_t seconds) {
    _mqtt.setKeepAlive(seconds);
}
void MeoMqttClient::setSocketTimeout(uint16_t seconds) {
    _mqtt.setSocketTimeout(seconds);
}

void MeoMqttClient::setWill(const char* topic, const char* payload, uint8_t qos, bool retain) {
    _willTopic = topic;
    _willPayload = payload;
    _willQos = qos;
    _willRetain = retain;
}

bool MeoMqttClient::connect() {
    if (WiFi.status() != WL_CONNECTED) {
        _log("ERROR", "MQTT", "WiFi not connected");
        return false;
    }
    if (_mqtt.connected()) return true;

    String clientId = _deviceId ? String("meo-") + _deviceId
                                : String("meo-device-") + String((uint32_t)millis());

    bool ok = false;
    if (_willTopic) {
        ok = _mqtt.connect(clientId.c_str(),
                           _deviceId, _txKey,
                           _willTopic, _willQos, _willRetain, _willPayload);
    } else {
        ok = _mqtt.connect(clientId.c_str(), _deviceId, _txKey);
    }

    _log(ok ? "INFO" : "ERROR", "MQTT", ok ? "Connected" : "Connect failed");
    return ok;
}

void MeoMqttClient::loop() {
    if (_mqtt.connected()) {
        _mqtt.loop();
    }
}

bool MeoMqttClient::isConnected() {
    return _mqtt.connected();
}

bool MeoMqttClient::publish(const char* topic, const uint8_t* payload, size_t len, bool retained) {
    if (!_mqtt.connected()) return false;
    if (_logger && _debugTagEnabled("MQTT")) {
        _logf("DEBUG", "MQTT", "Publish %s len=%u retained=%d", topic ? topic : "", (unsigned)len, retained);
    }
    return _mqtt.publish(topic, payload, len, retained);
}

bool MeoMqttClient::publish(const char* topic, const char* payload, bool retained) {
    if (!_mqtt.connected()) return false;
    if (_logger && _debugTagEnabled("MQTT")) {
        _logf("DEBUG", "MQTT", "Publish %s str retained=%d", topic ? topic : "", retained);
    }
    return _mqtt.publish(topic, payload, retained);
}

bool MeoMqttClient::subscribe(const char* topic, uint8_t qos) {
    if (!_mqtt.connected()) return false;
    bool ok = _mqtt.subscribe(topic, qos);
    if (_logger && _debugTagEnabled("MQTT")) {
        _logf(ok ? "DEBUG" : "ERROR", "MQTT", "%s subscribe %s",
              ok ? "OK" : "FAIL", topic ? topic : "");
    }
    return ok;
}

void MeoMqttClient::setMessageHandler(OnMessageFn fn, void* ctx) {
    _onMessage = fn;
    _onMessageCtx = ctx;
}

void MeoMqttClient::_pubsubThunk(char* topic, uint8_t* payload, unsigned int length) {
    if (_self) _self->_invokeMessageHandler(topic, payload, length);
}

void MeoMqttClient::_invokeMessageHandler(char* topic, uint8_t* payload, unsigned int length) {
    if (_onMessage) {
        if (_logger && _debugTagEnabled("MQTT")) {
            _logf("DEBUG", "MQTT", "Incoming %s len=%u", topic ? topic : "", length);
        }
        _onMessage(topic, payload, length, _onMessageCtx);
    }
}

bool MeoMqttClient::_debugTagEnabled(const char* tag) const {
    if (!_debugTags[0]) return false;
    const char* p = strstr(_debugTags, tag);
    if (!p) return false;
    bool leftOk  = (p == _debugTags) || (*(p - 1) == ',');
    const char* end = p + strlen(tag);
    bool rightOk = (*end == '\0') || (*end == ',');
    return leftOk && rightOk;
}

void MeoMqttClient::_log(const char* level, const char* tag, const char* msg) const {
    if (!_logger) return;
    char buf[256];
    snprintf(buf, sizeof(buf), "[%s] %s", tag ? tag : "MQTT", msg ? msg : "");
    _logger(level, buf);
}

void MeoMqttClient::_logf(const char* level, const char* tag, const char* fmt, ...) const {
    if (!_logger) return;
    char msg[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    _log(level, tag, msg);
}