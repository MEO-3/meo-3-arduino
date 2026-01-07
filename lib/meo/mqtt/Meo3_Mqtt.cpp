#include "Meo3_Mqtt.h"
#include <WiFi.h>

MeoMqttClient* MeoMqttClient::_self = nullptr;

MeoMqttClient::MeoMqttClient()
: _mqtt(_wifiClient) {
    _self = this; // one active instance
    _mqtt.setBufferSize(1024);
    _mqtt.setKeepAlive(15);
    _mqtt.setSocketTimeout(15);
    _mqtt.setCallback(&MeoMqttClient::_pubsubThunk);
}

void MeoMqttClient::configure(const char* host, uint16_t port) {
    _host = host;
    _port = port;
    _mqtt.setServer(_host, _port);
}

void MeoMqttClient::setCredentials(const char* deviceId, const char* transmitKey) {
    _deviceId = deviceId;
    _txKey = transmitKey;
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
        return false;
    }
    if (_mqtt.connected()) return true;

    // Client ID derived from deviceId if set, otherwise a generic one
    String clientId = _deviceId ? String("meo-") + _deviceId
                                : String("meo-device-") + String((uint32_t)millis());

    if (_willTopic) {
        return _mqtt.connect(clientId.c_str(),
                             _deviceId,          // username (can be null)
                             _txKey,             // password (can be null)
                             _willTopic, _willQos, _willRetain, _willPayload);
    }

    return _mqtt.connect(clientId.c_str(),
                         _deviceId,
                         _txKey);
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
    return _mqtt.publish(topic, payload, len, retained);
}

bool MeoMqttClient::publish(const char* topic, const char* payload, bool retained) {
    if (!_mqtt.connected()) return false;
    return _mqtt.publish(topic, payload, retained);
}

bool MeoMqttClient::subscribe(const char* topic, uint8_t qos) {
    if (!_mqtt.connected()) return false;
    return _mqtt.subscribe(topic, qos);
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
        _onMessage(topic, payload, length, _onMessageCtx);
    }
}