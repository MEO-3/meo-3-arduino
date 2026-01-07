#pragma once

#include <Arduino.h>

class MeoStorage {
public:
    MeoStorage();

    bool begin();  // initialize NVS/EEPROM

    bool loadCredentials(String& deviceIdOut, String& transmitKeyOut);
    bool saveCredentials(const String& deviceId, const String& transmitKey);
    bool clearCredentials();

private:
    bool _initialized;
};