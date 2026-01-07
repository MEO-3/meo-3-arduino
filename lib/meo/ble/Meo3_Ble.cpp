#include "Meo3_Ble.h"

MeoBle::MeoBle() = default;

bool MeoBle::begin(const char* deviceName) {
    NimBLEDevice::init(deviceName && deviceName[0] ? deviceName : "MEO Device");
    // Optional minimal security; can be extended in future features
    // NimBLEDevice::setSecurityAuth(true, true, true);
    // NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);

    _server = NimBLEDevice::createServer();
    return (_server != nullptr);
}

void MeoBle::startAdvertising() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) adv->start();
}

void MeoBle::stopAdvertising() {
    NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
    if (adv) adv->stop();
}

NimBLEService* MeoBle::createService(const char* serviceUuid) {
    if (!_server) return nullptr;
    NimBLEService* svc = _server->createService(serviceUuid);
    if (svc) {
        NimBLEAdvertising* adv = NimBLEDevice::getAdvertising();
        if (adv) adv->addServiceUUID(svc->getUUID());
    }
    return svc;
}

NimBLECharacteristic* MeoBle::createCharacteristic(NimBLEService* svc,
                                                   const char* charUuid,
                                                   uint32_t properties) {
    if (!svc) return nullptr;
    return svc->createCharacteristic(charUuid, properties);
}

void MeoBle::setCharWriteHandler(NimBLECharacteristic* ch, OnWriteFn fn, void* userCtx) {
    if (!ch || !fn) return;
    ch->setCallbacks(new _Callbacks(fn, userCtx));
}

NimBLEServer* MeoBle::server() const {
    return _server;
}

// _Callbacks implementation
MeoBle::_Callbacks::_Callbacks(OnWriteFn fn, void* ctx)
: _fn(fn), _ctx(ctx) {}

void MeoBle::_Callbacks::onWrite(NimBLECharacteristic* ch) {
    if (_fn) _fn(ch, _ctx);
}