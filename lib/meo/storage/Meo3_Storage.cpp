#include "Meo3_Storage.h"

static constexpr const char* MEO_PREFS_NAMESPACE = "meo";

MeoStorage::MeoStorage()
: _initialized(false) {}

bool MeoStorage::begin() {
    if (_initialized) return true;
    // Preferences::begin returns bool on ESP32 Arduino core
    bool ok = _prefs.begin(MEO_PREFS_NAMESPACE, /*readOnly*/ false);
    _initialized = ok;
    return ok;
}

bool MeoStorage::loadBytes(const char* key, uint8_t* buffer, size_t length) {
    if (!_initialized || !key || !buffer || length == 0) return false;

    size_t storedLen = _prefs.getBytesLength(key);
    if (storedLen == 0) return false;            // key not found
    if (storedLen > length) return false;        // caller buffer too small

    size_t got = _prefs.getBytes(key, buffer, storedLen);
    return (got == storedLen);
}

bool MeoStorage::saveBytes(const char* key, const uint8_t* data, size_t length) {
    if (!_initialized || !key || !data || length == 0) return false;
    size_t written = _prefs.putBytes(key, data, length);
    return (written == length);
}

bool MeoStorage::loadString(const char* key, String& valueOut) {
    if (!_initialized || !key) return false;
    if (!_prefs.isKey(key)) return false;
    valueOut = _prefs.getString(key, "");
    return true; // empty string is allowed if key exists
}

bool MeoStorage::saveString(const char* key, const String& value) {
    if (!_initialized || !key) return false;

    // Avoid flash wear by skipping redundant writes
    if (_prefs.isKey(key)) {
        String current = _prefs.getString(key, "");
        if (current == value) return true;
    }
    size_t written = _prefs.putString(key, value);
    return (written > 0);
}

// NEW: C-string helpers
bool MeoStorage::saveCString(const char* key, const char* value) {
    if (!_initialized || !key || !value) return false;

    // Avoid redundant write
    if (_prefs.isKey(key)) {
        String current = _prefs.getString(key, "");
        if (current.equals(value)) return true;
    }
    // Preferences::putString(const char* key, const char* value)
    size_t written = _prefs.putString(key, value);
    return (written > 0);
}

bool MeoStorage::loadCString(const char* key, char* buffer, size_t bufferLen) {
    if (!_initialized || !key || !buffer || bufferLen == 0) return false;
    if (!_prefs.isKey(key)) return false;

    // Read to a temporary String to check size and avoid truncation
    String s = _prefs.getString(key, "");
    size_t needed = s.length() + 1; // include NUL terminator
    if (needed > bufferLen) {
        // Caller buffer too small
        return false;
    }
    // Safe copy including terminator
    memcpy(buffer, s.c_str(), needed);
    return true;
}

bool MeoStorage::loadShort(const char* key, int16_t& valueOut) {
    if (!_initialized || !key) return false;
    if (!_prefs.isKey(key)) return false;
    valueOut = _prefs.getShort(key, 0);
    return true;
}

bool MeoStorage::saveShort(const char* key, int16_t value) {
    if (!_initialized || !key) return false;

    // Avoid redundant write
    if (_prefs.isKey(key)) {
        int16_t cur = _prefs.getShort(key, 0);
        if (cur == value) return true;
    }
    size_t written = _prefs.putShort(key, value);
    return (written == sizeof(int16_t));
}

bool MeoStorage::clearKey(const char* key) {
    if (!_initialized || !key) return false;
    // Preferences::remove returns bool (true if key was removed)
    return _prefs.remove(key);
}

bool MeoStorage::clearAll() {
    if (!_initialized) return false;
    // Preferences::clear returns bool (true if any key was removed)
    return _prefs.clear();
}