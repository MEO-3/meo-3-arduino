#pragma once

#include <Arduino.h>
#include <Preferences.h>

class MeoStorage {
public:
    MeoStorage();

    // Initialize underlying storage (Preferences/NVS)
    // For future extensibility, this could take a namespace, e.g., begin(const char* ns = "meo")
    bool begin();

    bool loadBytes(const char* key, uint8_t* buffer, size_t length);
    bool saveBytes(const char* key, const uint8_t* data, size_t length);

    bool loadString(const char* key, String& valueOut);
    bool saveString(const char* key, const String& value);

    bool loadShort(const char* key, int16_t& valueOut);
    bool saveShort(const char* key, int16_t value);

    // Remove a single key
    bool clearKey(const char* key);
    // Clear all stored keys/values
    bool clearAll();

private:
    bool _initialized;
    Preferences _prefs;
};