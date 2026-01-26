#include "Meo3_BleProvision.h"
#include <stdarg.h>
#include <esp_system.h>
#include <string>
#include <cctype>

void MeoBleProvision::setLogger(MeoLogFunction logger) {
    _logger = logger;
}
void MeoBleProvision::setDebugTags(const char* tagsCsv) {
    if (!tagsCsv) { _debugTags[0] = '\0'; return; }
    strncpy(_debugTags, tagsCsv, sizeof(_debugTags) - 1);
    _debugTags[sizeof(_debugTags) - 1] = '\0';
}

bool MeoBleProvision::begin(MeoBle* ble, MeoStorage* storage,
                            const char* devModel, const char* devManufacturer) {
    _ble = ble;
    _storage = storage;
    _devModel = devModel;
    _devManuf = devManufacturer;
    if (!_ble || !_storage || !_storage->begin()) return false;
    if (!_createServiceAndCharacteristics()) return false;
    _bindWriteHandlers();
    _svc->start();
    _loadInitialValues();
    _updateStatus();
    _logger("INFO", "BLE Provisioning service started");
    return true;
}

void MeoBleProvision::setCloudCompatibleInfo(const char* productId, const char* buildInfo) {
    _devProductId = productId;
    _buildInfo = buildInfo;
}

bool MeoBleProvision::_createServiceAndCharacteristics() {
    _svc = _ble->createService(MEO_BLE_PROV_SERV_UUID);
    if (!_svc) return false;

    // Per your spec: SSID RW, PASS WO, Model/Manuf RO, DevID RW, TxKey WO, Prog R+Notify
    _chSsid      = _ble->createCharacteristic(_svc, CH_UUID_WIFI_SSID,  NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    _chPass      = _ble->createCharacteristic(_svc, CH_UUID_WIFI_PASS,  NIMBLE_PROPERTY::WRITE);
    _chWifiList  = _ble->createCharacteristic(_svc, CH_UUID_WIFI_LIST,  NIMBLE_PROPERTY::READ);
    _chUserId    = _ble->createCharacteristic(_svc, CH_UUID_USER_ID,    NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    _chProductId = _ble->createCharacteristic(_svc, CH_UUID_PRODUCT_ID, NIMBLE_PROPERTY::READ);
    _chBuildInfo = _ble->createCharacteristic(_svc, CH_UUID_BUILD_INFO, NIMBLE_PROPERTY::READ);
    _chMacAddr   = _ble->createCharacteristic(_svc, CH_UUID_MAC_ADDR,   NIMBLE_PROPERTY::READ);
    _chModel     = _ble->createCharacteristic(_svc, CH_UUID_DEV_MODEL,  NIMBLE_PROPERTY::READ);
    _chManuf     = _ble->createCharacteristic(_svc, CH_UUID_DEV_MANUF,  NIMBLE_PROPERTY::READ);
    
    _chTxKey     = _ble->createCharacteristic(_svc, CH_UUID_TX_KEY,     NIMBLE_PROPERTY::WRITE);

    return _chSsid && _chPass && _chWifiList && _chModel && _chManuf && _chProductId && _chBuildInfo && _chMacAddr && _chUserId && _chTxKey;
}

void MeoBleProvision::_bindWriteHandlers() {
    _ble->setCharWriteHandler(_chSsid,  &MeoBleProvision::_onWriteStatic, this);
    _ble->setCharWriteHandler(_chPass,  &MeoBleProvision::_onWriteStatic, this);
    _ble->setCharWriteHandler(_chUserId, &MeoBleProvision::_onWriteStatic, this);
    _ble->setCharWriteHandler(_chTxKey, &MeoBleProvision::_onWriteStatic, this);
}

void MeoBleProvision::startAdvertising() { if (_ble) _ble->startAdvertising(); }
void MeoBleProvision::stopAdvertising()  { if (_ble) _ble->stopAdvertising();  }

void MeoBleProvision::loop() {
    // Status notify every ~2 seconds
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 2000) {
        _updateStatus();
        lastStatus = millis();
    }
    // Execute scheduled reboot
    if (_autoReboot && _rebootScheduled && millis() >= _rebootAtMs) {
        _logger("INFO", "Reboot now");
        delay(100);
        ESP.restart();
    }
}

void MeoBleProvision::setRuntimeStatus(const char* wifi, const char* mqtt) {
    _wifiStatus = wifi ? wifi : _wifiStatus;
    _mqttStatus = mqtt ? mqtt : _mqttStatus;
}

void MeoBleProvision::setAutoRebootOnProvision(bool enable, uint32_t delayMs) {
    _autoReboot = enable;
    _rebootDelayMs = delayMs;
}

void MeoBleProvision::_loadInitialValues() {
    std::string tmp;
    if (_storage->loadString("wifi_ssid", tmp))    _chSsid->setValue(tmp.c_str());
    if (_storage->loadString("user_id", tmp))   _chUserId->setValue(tmp.c_str());
    // Initialize WiFi list characteristic (read-only). Library does not persist list.
    if (_chWifiList) _chWifiList->setValue("");
    if (_chProductId) _chProductId->setValue(_devProductId ? _devProductId : "unknown");
    if (_chBuildInfo) _chBuildInfo->setValue(_buildInfo ? _buildInfo : "unknown");

    // MAC address: use the device's Ethernet MAC (ESP MAC), not BLE
    if (_chMacAddr) {
        uint8_t mac_raw[6] = {0};
        esp_err_t r = esp_read_mac(mac_raw, ESP_MAC_ETH);
        if (r != ESP_OK) {
            // fallback to WiFi STA MAC if Ethernet MAC unavailable
            esp_read_mac(mac_raw, ESP_MAC_WIFI_STA);
        }
        char macbuf[18];
        snprintf(macbuf, sizeof(macbuf), "%02X:%02X:%02X:%02X:%02X:%02X",
                 mac_raw[0], mac_raw[1], mac_raw[2], mac_raw[3], mac_raw[4], mac_raw[5]);
        _chMacAddr->setValue(macbuf);
    }

    if (_devModel && _devModel[0])  _chModel->setValue(_devModel);
    if (_devManuf && _devManuf[0])  _chManuf->setValue(_devManuf);
}

void MeoBleProvision::_scheduleRebootIfReady() {
    if (!_autoReboot) return;
    if (_ssidWritten && _passWritten && !_rebootScheduled) {
        _rebootScheduled = true;
        _rebootAtMs = millis() + _rebootDelayMs;
        _logger("INFO", "Provisioning complete; scheduling reboot");
    }
}

void MeoBleProvision::_onWriteStatic(NimBLECharacteristic* ch, void* ctx) {
    reinterpret_cast<MeoBleProvision*>(ctx)->_onWrite(ch);
}

void MeoBleProvision::_onWrite(NimBLECharacteristic* ch) {
    const NimBLEUUID& uuid = ch->getUUID();
    std::string s = ch->getValue();
    // trim whitespace (including CR/LF) in-place
    while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    size_t start = 0; while (start < s.size() && std::isspace((unsigned char)s[start])) ++start;
    if (start > 0) s.erase(0, start);

    if (uuid.equals(NimBLEUUID(CH_UUID_WIFI_SSID))) {
        _storage->saveString("wifi_ssid", s);
        _ssidWritten = true;
        _logger("INFO", "SSID updated");
        _scheduleRebootIfReady();
        return;
    }
    if (uuid.equals(NimBLEUUID(CH_UUID_WIFI_PASS))) {
        _storage->saveString("wifi_pass", s);
        _passWritten = true;
        _logger("INFO", "PASS updated");
        _scheduleRebootIfReady();
        return;
    }
    if (uuid.equals(NimBLEUUID(CH_UUID_USER_ID))) {
        _storage->saveString("user_id", s);
        _logger("INFO", "User ID updated");
        return;
    }
    if (uuid.equals(NimBLEUUID(CH_UUID_TX_KEY))) {
        _storage->saveString("tx_key", s);
        _logger("INFO", "Transmit Key updated");
        return;
    }
}

void MeoBleProvision::_updateStatus() {
    snprintf(_statusBuf, sizeof(_statusBuf),
             "WiFi: %s, MQTT: %s",
             _wifiStatus, _mqttStatus);
    // Progress characteristic removed; only log DEBUG when enabled.
    if (_logger && _debugTagEnabled("PROV")) {
        _logger("DEBUG", _statusBuf);
    }
}

bool MeoBleProvision::_debugTagEnabled(const char* tag) const {
    if (!_debugTags[0]) return false;
    const char* p = strstr(_debugTags, tag);
    if (!p) return false;
    bool leftOk  = (p == _debugTags) || (*(p - 1) == ',');
    const char* end = p + strlen(tag);
    bool rightOk = (*end == '\0') || (*end == ',');
    return leftOk && rightOk;
}