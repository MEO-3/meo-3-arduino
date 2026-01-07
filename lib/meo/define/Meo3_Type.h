#pragma once

#include <Arduino.h>
#include <functional>
#include <map>
#include <vector>

// Connection type mirrors org.thingai.meo.define.MConnectionType
enum class MeoConnectionType : int {
    LAN  = 0,
    UART = 1
};

// Device info – maps conceptually to MDevice fields
struct MeoDeviceInfo {
    String label;
    String model;
    String manufacturer;
    MeoConnectionType connectionType;

    MeoDeviceInfo()
        : label(""), model(""), manufacturer(""), connectionType(MeoConnectionType::LAN) {}
};

// Simple key-value payload type for events/feature params
using MeoEventPayload = std::map<String, String>;  // later we can switch to ArduinoJson

// Represent a feature invocation from the gateway
struct MeoFeatureCall {
    String deviceId;
    String featureName;
    MeoEventPayload params;   // raw string values; user can parse as needed
    String requestId;         // if you define correlation IDs
};

// Callback type for feature handlers
using MeoFeatureCallback = std::function<void(const MeoFeatureCall&)>;

// Registry of supported features
struct MeoFeatureRegistry {
    std::vector<String> eventNames;
    std::map<String, MeoFeatureCallback> methodHandlers;
};

// Logging hook
using MeoLogFunction = std::function<void(const char* level, const char* message)>;