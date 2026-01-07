#include "Meo3_Storage.h"
#include <Preferences.h>

static const char* NAMESPACE = "meo3";
static const char* KEY_DEVICE_ID = "device_id";
static const char* KEY_TX_KEY    = "tx_key";

MeoStorage::MeoStorage()
    : _initialized(false) {}

bool MeoStorage::begin() {
    // Preferences opens NVS namespace lazily; we just mark initialized here.
    _initialized = true;
    return true;
}

bool MeoStorage::loadCredentials(String& deviceIdOut, String& transmitKeyOut) {
    if (!_initialized && !begin()) {
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(NAMESPACE, true)) { // read-only
        return false;
    }

    String id = prefs.getString(KEY_DEVICE_ID, "");
    String key = prefs.getString(KEY_TX_KEY, "");
    prefs.end();

    if (id.length() == 0 || key.length() == 0) {
        return false;
    }

    deviceIdOut = id;
    transmitKeyOut = key;
    return true;
}

bool MeoStorage::saveCredentials(const String& deviceId, const String& transmitKey) {
    if (!_initialized && !begin()) {
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) { // read-write
        return false;
    }

    bool ok1 = prefs.putString(KEY_DEVICE_ID, deviceId) > 0;
    bool ok2 = prefs.putString(KEY_TX_KEY, transmitKey) > 0;
    prefs.end();
    return ok1 && ok2;
}

bool MeoStorage::clearCredentials() {
    if (!_initialized && !begin()) {
        return false;
    }

    Preferences prefs;
    if (!prefs.begin(NAMESPACE, false)) {
        return false;
    }

    prefs.remove(KEY_DEVICE_ID);
    prefs.remove(KEY_TX_KEY);
    prefs.end();
    return true;
}