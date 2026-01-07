#include "Meo3_Registration.h"
#include <WiFi.h>
#include <WiFiUdp.h>
#include <ArduinoJson.h>

static const uint16_t MEO_REG_LISTEN_PORT = 8091;
static const uint16_t MEO_REG_DISCOVERY_PORT = 8901; // UDP broadcast port on gateway side (for example)
static const char*    MEO_REG_DISCOVERY_MAGIC = "MEO3_DISCOVERY_V1";

MeoRegistrationClient::MeoRegistrationClient()
    : _port(MEO_REG_DISCOVERY_PORT),
      _logger(nullptr) {}

void MeoRegistrationClient::setGateway(const char* host, uint16_t port) {
    _gatewayHost = host;
    _port = port;
}

void MeoRegistrationClient::setLogger(MeoLogFunction logger) {
    _logger = logger;
}

bool MeoRegistrationClient::registerIfNeeded(const MeoDeviceInfo& devInfo,
                                             const MeoFeatureRegistry& features,
                                             String& deviceIdOut,
                                             String& transmitKeyOut) {
    if (deviceIdOut.length() > 0 && transmitKeyOut.length() > 0) {
        // already have credentials
        return true;
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (_logger) _logger("ERROR", "WiFi not connected; cannot register");
        return false;
    }

    // 1) Send broadcast to announce ourselves
    if (!_sendBroadcast(devInfo, features)) {
        if (_logger) _logger("ERROR", "Failed to send registration broadcast");
        return false;
    }

    // 2) Listen on TCP 8091 for gateway response
    String responseJson;
    if (!_waitForRegistrationResponse(responseJson)) {
        if (_logger) _logger("ERROR", "Did not receive registration response");
        return false;
    }

    // 3) Parse response
    return _parseRegistrationResponse(responseJson, deviceIdOut, transmitKeyOut);
}

bool MeoRegistrationClient::_sendBroadcast(const MeoDeviceInfo& devInfo,
                                           const MeoFeatureRegistry& features) {
    WiFiUDP udp;
    if (!udp.begin(MEO_REG_DISCOVERY_PORT)) {
        if (_logger) _logger("ERROR", "Failed to open UDP for discovery");
        return false;
    }

    StaticJsonDocument<1024> doc;
    doc["magic"]        = MEO_REG_DISCOVERY_MAGIC;  // so gateway can filter
    doc["label"]        = devInfo.label;
    doc["model"]        = devInfo.model;
    doc["manufacturer"] = devInfo.manufacturer;
    doc["connectionType"]= static_cast<int>(devInfo.connectionType);
    doc["mac"]          = WiFi.macAddress();
    doc["ip"]           = WiFi.localIP().toString();
    doc["listen_port"]  = MEO_REG_LISTEN_PORT;      // tell gateway where to reply

    JsonArray events  = doc.createNestedArray("featureEvents");
    for (const auto& e : features.eventNames) {
        events.add(e);
    }
    JsonArray methods = doc.createNestedArray("featureMethods");
    for (const auto& kv : features.methodHandlers) {
        methods.add(kv.first);
    }

    char buffer[1024];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    if (len == 0) {
        if (_logger) _logger("ERROR", "Failed to serialize discovery JSON");
        udp.stop();
        return false;
    }

    IPAddress broadcastIP = ~WiFi.subnetMask() | WiFi.gatewayIP(); // standard broadcast calc
    if (_logger) {
        String msg = "Sending discovery broadcast to ";
        msg += broadcastIP.toString();
        msg += ":";
        msg += MEO_REG_DISCOVERY_PORT;
        _logger("INFO", msg.c_str());
    }

    udp.beginPacket(broadcastIP, MEO_REG_DISCOVERY_PORT);
    udp.write((const uint8_t*)buffer, len);
    udp.endPacket();
    udp.stop();

    return true;
}

bool MeoRegistrationClient::_waitForRegistrationResponse(String& responseJson) {
    WiFiServer server(MEO_REG_LISTEN_PORT);
    server.begin();

    if (_logger) _logger("INFO", "Listening for registration response on TCP port 8091");

    unsigned long start = millis();
    const unsigned long timeoutMs = 15000;  // 15s

    while ((millis() - start) < timeoutMs) {
        WiFiClient client = server.available();
        if (!client) {
            delay(50);
            continue;
        }

        if (_logger) _logger("INFO", "Gateway connected for registration");

        // Read until newline or timeout
        responseJson = "";
        unsigned long connStart = millis();
        while (client.connected() && (millis() - connStart) < 5000) {
            while (client.available()) {
                char c = client.read();
                if (c == '\n') {
                    client.stop();
                    server.stop();
                    if (_logger) {
                        String msg = "Received registration response: ";
                        msg += responseJson;
                        _logger("DEBUG", msg.c_str());
                    }
                    return true;
                }
                responseJson += c;
            }
            delay(10);
        }

        client.stop();
    }

    server.stop();
    if (_logger) _logger("ERROR", "Timeout waiting for registration TCP connection");
    return false;
}

bool MeoRegistrationClient::_parseRegistrationResponse(const String& json,
                                                       String& deviceIdOut,
                                                       String& transmitKeyOut) {
    StaticJsonDocument<256> doc;
    DeserializationError err = deserializeJson(doc, json);
    if (err) {
        if (_logger) {
            String msg = "Failed to parse registration response: ";
            msg += err.c_str();
            _logger("ERROR", msg.c_str());
        }
        return false;
    }

    if (!doc.containsKey("device_id") || !doc.containsKey("transmit_key")) {
        if (_logger) _logger("ERROR", "Registration response missing fields");
        return false;
    }

    deviceIdOut    = doc["device_id"].as<const char*>();
    transmitKeyOut = doc["transmit_key"].as<const char*>();
    return true;
}