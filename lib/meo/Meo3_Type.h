#pragma once

#include <Arduino.h>
#include <string>
#include <functional>
#include <map>
#include <vector>

// Connection type mirrors org.thingai.meo.define.MConnectionType
enum class MeoConnectionType : int {
    WIFI = 0,
    BLE = 1,
    LAN  = 2,
    UART = 3,
    
    CUSTOM = 255
};

// Device info – maps conceptually to MDevice fields
struct MeoDeviceInfo {
    std::string model;
    std::string manufacturer;
    MeoConnectionType connectionType;

    MeoDeviceInfo()
        : model(""), manufacturer(""), connectionType(MeoConnectionType::LAN) {}
};

// Simple key-value payload type for events/feature params
using MeoEventPayload = std::map<std::string, std::string>;  // later we can switch to ArduinoJson

// Represent a feature invocation from the gateway
struct MeoFeatureCall {
    std::string deviceId;
    std::string featureName;
    MeoEventPayload params;   // raw string values; user can parse as needed
    // requestId removed to simplify API and payloads
};

// Callback type for feature handlers
using MeoFeatureCallback = std::function<void(const MeoFeatureCall&)>;

// Registry of supported features
struct MeoFeatureRegistry {
    std::vector<std::string> eventNames;
    std::map<std::string, MeoFeatureCallback> methodHandlers;
};

// Logging hook
using MeoLogFunction = std::function<void(const char* level, const char* message)>;