#include <Arduino.h>
#include <Meo3_Device.h>

#define LED_BUILTIN 8

MeoDevice meo;

// Example feature callback
void onTurnOn(const MeoFeatureCall& call) {
    Serial.println("Feature 'turn_on_led' invoked");
    digitalWrite(LED_BUILTIN, HIGH);

    int first = 0, second = 0;
    for (const auto& kv : call.params) {
        Serial.printf("  %s = %s\n", kv.first.c_str(), kv.second.c_str());
        if (kv.first == "first")  first  = kv.second.toInt();
        if (kv.first == "second") second = kv.second.toInt();
    }
    char msg[64];
    snprintf(msg, sizeof(msg), "LED on, sum=%d", first + second);
    meo.sendFeatureResponse(call, true, msg);
}

// Optional logger
void meoLogger(const char* level, const char* message) {
    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(message);
}

void setup() {
    Serial.begin(115200);
    delay(2000);

    pinMode(LED_BUILTIN, OUTPUT);

    meo.setDeviceInfo("DIY Sensor", "Test MEO Module", "ThingAI Lab");
    meo.setGateway("meo-open-service.local", 1883);

    meo.addFeatureMethod("turn_on_led", onTurnOn);
    meo.addFeatureEvent("humid_temp_update");

    meo.start();
}

void loop() {
    meo.loop();

    static uint32_t last = 0;
    if (millis() - last > 5000 && meo.isMqttConnected()) {
        last = millis();
        MeoEventPayload p;
        p["temperature"] = String(random(200, 300) / 10);
        p["humidity"]    = String(random(400, 600) / 10);
        meo.publishEvent("humid_temp_update", p);
    }
}