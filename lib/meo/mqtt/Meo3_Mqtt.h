#pragma once

#include <Arduino.h>
#include <WiFiClientSecure.h>
#include <PubSubClient.h>
#include "../Meo3_Type.h" // MeoLogFunction

/**
 * MeoMqtt: minimal MQTT transport wrapper around PubSubClient.
 * - Keeps RAM/flash low
 * - Clean separation from device/feature logic
 * - Delivers raw messages via a lightweight function pointer callback
 */
class MeoMqttClient {
public:
    typedef void (*OnMessageFn)(const char* topic, const uint8_t* payload, unsigned int length, void* ctx);

    MeoMqttClient();

    // Logging
    void setLogger(MeoLogFunction logger);
    void setDebugTags(const char* tagsCsv); // enables DEBUG for "MQTT" when tag present

    // Configure broker host and port
    void configure(const char* host, uint16_t port = 1883);

    // Set device credentials (used as username/password)
    void setCredentials(const char* deviceId, const char* transmitKey);

    // Optional: tune internal buffers/timeouts
    void setBufferSize(uint16_t bytes);     // default 1024
    void setKeepAlive(uint16_t seconds);    // default 15
    void setSocketTimeout(uint16_t seconds);// default 15

    // Optional Last Will
    void setWill(const char* topic, const char* payload, uint8_t qos = 0, bool retain = true);

    // Connect to broker; returns true on success
    bool connect();

    // Must be called frequently to process incoming/outgoing MQTT traffic
    void loop();

    bool isConnected();

    // Raw publish/subscribe
    bool publish(const char* topic, const uint8_t* payload, size_t len, bool retained = false);
    bool publish(const char* topic, const char* payload, bool retained = false);
    bool subscribe(const char* topic, uint8_t qos = 0);

    // Set message handler (function pointer)
    void setMessageHandler(OnMessageFn fn, void* ctx);

    // Accessors
    const char* host() const { return _host; }
    uint16_t    port() const { return _port; }
    const char* deviceId() const { return _deviceId; }

private:
    const char*  _host = nullptr;
    uint16_t     _port = 1883;
    const char*  _deviceId = nullptr;
    const char*  _txKey = nullptr;

    const char*  _willTopic = nullptr;
    const char*  _willPayload = nullptr;
    uint8_t      _willQos = 0;
    bool         _willRetain = true;

    WiFiClientSecure   _wifiClient;
    PubSubClient _mqtt;

    OnMessageFn  _onMessage = nullptr;
    void*        _onMessageCtx = nullptr;

    // Logging
    MeoLogFunction _logger = nullptr;
    char           _debugTags[96] = {0};

    static MeoMqttClient* _self;
    static void _pubsubThunk(char* topic, uint8_t* payload, unsigned int length);
    void _invokeMessageHandler(char* topic, uint8_t* payload, unsigned int length);

    bool _debugTagEnabled(const char* tag) const;
    void _log(const char* level, const char* tag, const char* msg) const;
    void _logf(const char* level, const char* tag, const char* fmt, ...) const;
};