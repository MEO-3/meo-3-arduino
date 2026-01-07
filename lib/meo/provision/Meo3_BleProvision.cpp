#include "Meo3_BleProvision.h"

bool MeoBleProvision::begin(MeoBle* ble, MeoStorage* storage, const char* devModel, const char* devManufacturer) {
    _ble = ble;
    _storage = storage;
    _devModel = devModel;
    _devManuf = devManufacturer;

    if (!_ble || !_storage || !_storage->begin()) return false;
    if (!_createServiceAndCharacteristics()) return false;

    _bindWriteHandlers();
    _svc->start();

    _loadInitialValues();

    return true;
}

bool MeoBleProvision::_createServiceAndCharacteristics() {
    _svc = _ble->createService(MEO_BLE_PROV_SERV_UUID);
    if (!_svc) return false;


    // Per your spec: SSID RW, PASS WO, Model/Manuf RO, DevID RW, TxKey WO, Prog R+Notify
    _chSsid  = _ble->createCharacteristic(_svc, CH_UUID_WIFI_SSID, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    _chPass  = _ble->createCharacteristic(_svc, CH_UUID_WIFI_PASS, NIMBLE_PROPERTY::WRITE);
    _chModel = _ble->createCharacteristic(_svc, CH_UUID_DEV_MODEL, NIMBLE_PROPERTY::READ);
    _chManuf = _ble->createCharacteristic(_svc, CH_UUID_DEV_MANUF, NIMBLE_PROPERTY::READ);
    _chDevId = _ble->createCharacteristic(_svc, CH_UUID_DEV_ID,   NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::WRITE);
    _chTxKey = _ble->createCharacteristic(_svc, CH_UUID_TX_KEY,   NIMBLE_PROPERTY::WRITE);
    _chProg  = _ble->createCharacteristic(_svc, CH_UUID_PROV_PROG, NIMBLE_PROPERTY::READ | NIMBLE_PROPERTY::NOTIFY);

    return _chSsid && _chPass && _chModel && _chManuf && _chDevId && _chTxKey && _chProg;
}

void MeoBleProvision::_bindWriteHandlers() {
    _ble->setCharWriteHandler(_chSsid,  &MeoBleProvision::_onWriteStatic, this);
    _ble->setCharWriteHandler(_chPass,  &MeoBleProvision::_onWriteStatic, this);
    _ble->setCharWriteHandler(_chDevId, &MeoBleProvision::_onWriteStatic, this);
    _ble->setCharWriteHandler(_chTxKey, &MeoBleProvision::_onWriteStatic, this);
}

void MeoBleProvision::startAdvertising() { if (_ble) _ble->startAdvertising(); }
void MeoBleProvision::stopAdvertising()  { if (_ble) _ble->stopAdvertising();  }

void MeoBleProvision::setRuntimeStatus(const char* wifi, const char* mqtt) {
    if (wifi) _wifiStatus = wifi;
    if (mqtt) _mqttStatus = mqtt;
}

void MeoBleProvision::setAutoRebootOnProvision(bool enable, uint32_t delayMs) {
    _autoReboot = enable;
    _rebootDelayMs = delayMs;
}

void MeoBleProvision::loop() {
    // Status notify every ~2 seconds
    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 2000) {
        _updateStatus();
        lastStatus = millis();
    }
    // Execute scheduled reboot
    if (_rebootScheduled && millis() >= _rebootAtMs) {
        _log("INFO", "Reboot now");
        delay(100);
        ESP.restart();
    }
}

void MeoBleProvision::_loadInitialValues() {
    String tmp;
    if (_storage->loadString("wifi_ssid", tmp))    _chSsid->setValue(tmp.c_str());
    if (_storage->loadString("device_id", tmp))    _chDevId->setValue(tmp.c_str());
    if (_devModel && _devModel[0])                 _chModel->setValue(_devModel);
    if (_devManuf && _devManuf[0])                 _chManuf->setValue(_devManuf);
}

void MeoBleProvision::_scheduleRebootIfReady() {
    if (!_autoReboot) return;

    if (_ssidWritten && _passWritten && !_rebootScheduled) {
        _rebootScheduled = true;
        _rebootAtMs = millis() + _rebootDelayMs;
        _log("INFO", "Provisioning complete; scheduling reboot");
    }
}

void MeoBleProvision::_onWriteStatic(NimBLECharacteristic* ch, void* ctx) {
    reinterpret_cast<MeoBleProvision*>(ctx)->_onWrite(ch);
}

void MeoBleProvision::_onWrite(NimBLECharacteristic* ch) {
    const NimBLEUUID& uuid = ch->getUUID();
    std::string val = ch->getValue();
    const char* cstr = val.c_str();

    if (uuid.equals(NimBLEUUID(CH_UUID_WIFI_SSID))) {
        _storage->saveString("wifi_ssid", String(cstr));
        _ssidWritten = true;
        _log("INFO", "SSID updated");
        _scheduleRebootIfReady();
        return;
    }
    if (uuid.equals(NimBLEUUID(CH_UUID_WIFI_PASS))) {
        _storage->saveString("wifi_pass", String(cstr));
        _passWritten = true;
        _log("INFO", "PASS updated");
        _scheduleRebootIfReady();
        return;
    }
    if (uuid.equals(NimBLEUUID(CH_UUID_DEV_ID))) {
        _storage->saveString("device_id", String(cstr));
        _log("INFO", "Device ID updated");
        return;
    }
    if (uuid.equals(NimBLEUUID(CH_UUID_TX_KEY))) {
        _storage->saveString("tx_key", String(cstr));
        _log("INFO", "Transmit Key updated");
        return;
    }
}

void MeoBleProvision::_updateStatus() {
    snprintf(_statusBuf, sizeof(_statusBuf),
             "WiFi: %s\nMQTT: %s",
             _wifiStatus, _mqttStatus);
    if (_chProg) {
        _chProg->setValue(_statusBuf);
        _chProg->notify();
    }
}

void MeoBleProvision::_log(const char* level, const char* msg) const {
    #if defined(ARDUINO)
        Serial.printf("[%s] %s\n", level, msg);
    #else
        (void)level; (void)msg;
    #endif
}