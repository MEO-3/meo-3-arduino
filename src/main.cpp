#include <Arduino.h>
#include <Meo3_Device.h>

#define LED_BUILTIN 8

MeoDevice meo;

// Example feature callback
void onTurnOn(const MeoFeatureCallback& call) {
    Serial.println("Feature 'turn_on' invoked");
    digitalWrite(LED_BUILTIN, HIGH);
    meo.sendFeatureResponse("turn_on_led", true, "LED turned on");
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
    meo.start();
}

void loop() {
    meo.loop();
}