#include "Meo3_Mqtt.h"
#include <WiFi.h>
#include <stdarg.h>

MeoMqttClient* MeoMqttClient::_self = nullptr;


// ISRG Root X1 - Let's Encrypt Root CA
const char* rootCa = "-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";


MeoMqttClient::MeoMqttClient() {
    _self = this; // one active instance
    _wifiClient.setCACert(rootCa);
    _mqtt.setClient(_wifiClient);
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
                           "edgemqtt", _txKey,
                           _willTopic, _willQos, _willRetain, _willPayload);
    } else {
        ok = _mqtt.connect(clientId.c_str(), "edgemqtt", _txKey);
    }

    _log("DEBUG", clientId.c_str(), _txKey);
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