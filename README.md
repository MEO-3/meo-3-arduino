# MEO 3 Arduino Library (ESP32)

MEO 3 Arduino is a minimal, production‑oriented SDK for connecting ESP32 devices to the MEO Open Service via MQTT. It focuses on:
- Simple feature method callbacks (MeoFeatureCall)
- Lightweight event publishing (MeoEventPayload)
- Built‑in BLE provisioning for Wi‑Fi and device credentials
- Clear logging with optional debug tags

This README reflects the current APIs in the repository.

---

## Features

- Wi‑Fi and MQTT connection management
- BLE provisioning service to set SSID/PASS and credentials
- Feature Methods (server → device) via callbacks
- Feature Events (device → server) via MQTT topics
- JSON payloads with small, fixed buffers
- Logger hook and tag‑filtered DEBUG logs: DEVICE, MQTT, PROV

---

## Requirements

- ESP32 board (Arduino framework)
- MQTT broker (used by MEO Open Service)
- Arduino libraries:
  - WiFi (ESP32)
  - PubSubClient
  - ArduinoJson
  - NimBLE-Arduino (via Meo3_Ble/Meo3_BleProvision)

---

## Install

- Clone or download this repository into Arduino libraries:
  - Documents/Arduino/libraries/meo-3-arduino (typical)
- Restart Arduino IDE

---

## Quick Start

Minimal device that declares one feature method and publishes an event.

```cpp
#include <Arduino.h>
#include "Meo3_Device.h"

#define LED_BUILTIN 8

MeoDevice meo;

// Feature method callback (server → device)
void onTurnOn(const MeoFeatureCall& call) {
  Serial.println("Feature 'turn_on_led' invoked");
  digitalWrite(LED_BUILTIN, HIGH);

  // Inspect params (String → String map)
  for (const auto& kv : call.params) {
    Serial.print("  ");
    Serial.print(kv.first);
    Serial.print(" = ");
    Serial.println(kv.second);
  }

  meo.sendFeatureResponse(call, true, "LED turned on");
}

void setup() {
  Serial.begin(115200);
  delay(500);

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, LOW);

  // Optional: structured logging
  meo.setLogger([](const char* level, const char* msg) {
    Serial.printf("[%s] %s\n", level, msg);
  });
  meo.setDebugTags("DEVICE,MQTT,PROV"); // enable DEBUG logs for selected tags

  // Device metadata (no label in current API)
  meo.setDeviceInfo("Test MEO Module", "ThingAI Lab");

  // Gateway (use IP if .local is unreliable on your LAN)
  meo.setGateway("meo-open-service", 1883);

  // Wi‑Fi now, or provision via BLE if you skip this
  // meo.beginWifi("YOUR_WIFI_SSID", "YOUR_WIFI_PASS");

  // Declare features
  meo.addFeatureMethod("turn_on_led", onTurnOn);
  meo.addFeatureEvent("sensor_update");

  // Start: BLE provisioning (if needed), Wi‑Fi/MQTT connect, declare
  meo.start();
}

void loop() {
  meo.loop();

  // Publish an event periodically
  static uint32_t last = 0;
  if (millis() - last > 5000 && meo.isMqttConnected()) {
    last = millis();
    MeoEventPayload p;
    p["temperature"] = String(random(200, 300) / 10.0); // 20.0–30.0
    p["humidity"]    = String(random(400, 600) / 10.0); // 40.0–60.0
    meo.publishEvent("sensor_update", p);
  }
}
```

---

## BLE Provisioning Flow

If Wi‑Fi or credentials are missing, the device:
1. Starts BLE advertising a provisioning service.
2. Exposes characteristics for:
   - wifi_ssid (RW)
   - wifi_pass (WO)
   - device_id (RW)
   - tx_key (WO)
   - model (RO)
   - manufacturer (RO)
   - progress/status (R + Notify)
3. After both SSID and PASS are written, the device connects to Wi‑Fi.
4. Once Wi‑Fi is connected, BLE advertising is stopped automatically (to reduce radio contention).
   - To re‑provision later, reboot the device to re‑enable BLE provisioning.

Notes:
- Write device_id and tx_key from your mobile tool/app. The library reads tx_key as the MQTT password.
- Avoid trailing spaces and non‑printable characters when writing strings.

---

## MQTT Topics

For device_id = BACDIEIFIEE:

- Status (retain recommended by the broker):
  - meo/BACDIEIFIEE/status → "online" | "offline"
- Declare (capabilities on connect):
  - meo/BACDIEIFIEE/declare → { device_info, events[], methods[] }
- Events (device → server):
  - meo/BACDIEIFIEE/event/{eventName} → { ...payload }
- Feature invoke (server → device):
  - meo/BACDIEIFIEE/feature/{featureName}/invoke → { params: { k: v, ... } }
- Feature response (device → server):
  - meo/BACDIEIFIEE/event/feature_response → { feature_name, device_id, success, message? }

No request_id is used in this SDK.

---

## Logging

Enable structured logs and selective DEBUG:

```cpp
meo.setLogger([](const char* level, const char* msg){
  Serial.printf("[%s] %s\n", level, msg);
});
meo.setDebugTags("DEVICE,MQTT,PROV"); // only these tags emit DEBUG
```

- Tags the library uses:
  - DEVICE: device lifecycle, feature registry, declare
  - MQTT: broker config, connect, publish/subscribe
  - PROV: BLE provisioning status/changes

INFO/WARN/ERROR are always logged when a logger is set; DEBUG respects tag filtering.

---

## API Overview

Types (lib/meo/Meo3_Type.h):
- MeoEventPayload = std::map<String, String>
- struct MeoFeatureCall { String deviceId; String featureName; MeoEventPayload params; }
- using MeoFeatureCallback = std::function<void(const MeoFeatureCall&)>;

MeoDevice (lib/meo/Meo3_Device.h):
- Logging
  - setLogger(MeoLogFunction)
  - setDebugTags(const char* csvTags) // e.g., "DEVICE,MQTT"
- Identity and connection
  - setDeviceInfo(const char* model, const char* manufacturer)
  - setGateway(const char* host, uint16_t port = 1883)
  - beginWifi(const char* ssid, const char* pass) // optional if provisioning via BLE
- Features
  - addFeatureEvent(const char* name)
  - addFeatureMethod(const char* name, MeoFeatureCallback cb)
- Lifecycle
  - start()
  - loop()
- Publish/Respond
  - publishEvent(const char* eventName, const char* const* keys, const char* const* values, uint8_t count)
  - publishEvent(const char* eventName, const MeoEventPayload& payload)
  - sendFeatureResponse(const char* featureName, bool ok, const char* message)
  - sendFeatureResponse(const MeoFeatureCall& call, bool ok, const char* message)
- Status
  - bool isMqttConnected()
  - bool hasCredentials()

Behavioral notes:
- When Wi‑Fi is connected during start(), BLE advertising is stopped automatically.
- loop() keeps MQTT alive and tries lazy reconnect if Wi‑Fi and credentials are present.

---

## Troubleshooting

- Broker shows “Bad socket read/write … Malformed UTF‑8”
  - Ensure device_id and topics contain only printable ASCII; avoid CR/LF or hidden characters.
  - Re‑write device_id via provisioning without trailing spaces.
- mDNS name doesn’t resolve (meo-open-service or meo-open-service.local)
  - Use the gateway’s IP address with setGateway("192.168.x.x", 1883).
- MQTT doesn’t reconnect after power loss
  - Confirm Wi‑Fi is connected and credentials (device_id, tx_key) exist.
  - Increase Serial logging with setDebugTags("DEVICE,MQTT").
- No feature callback fires
  - Verify method name in addFeatureMethod matches server invoke topic feature.
  - Confirm subscribe success in logs (Subscribed to meo/{id}/feature/+/invoke).

---

## Example: Two features, one event

```cpp
#include <Arduino.h>
#include "Meo3_Device.h"

MeoDevice meo;

void onTurnOn(const MeoFeatureCall& call) {
  meo.sendFeatureResponse(call, true, "turn_on OK");
}
void onTurnOff(const MeoFeatureCall& call) {
  meo.sendFeatureResponse(call, true, "turn_off OK");
}

void setup() {
  Serial.begin(115200);
  meo.setLogger([](const char* level, const char* msg){ Serial.printf("[%s] %s\n", level, msg); });
  meo.setDebugTags("DEVICE,MQTT");

  meo.setDeviceInfo("MyModel", "MyCompany");
  meo.setGateway("192.168.1.10", 1883);
  // meo.beginWifi("SSID", "PASS"); // or provision via BLE

  meo.addFeatureMethod("turn_on",  onTurnOn);
  meo.addFeatureMethod("turn_off", onTurnOff);
  meo.addFeatureEvent("sensor_update");

  meo.start();
}

void loop() {
  meo.loop();
}
```

---

## License

aGPLv3 (see LICENSE if provided in the repository)

---