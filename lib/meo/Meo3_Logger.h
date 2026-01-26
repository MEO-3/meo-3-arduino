#include <Arduino.h>
#include <WiFi.h>
#include <string>

void MeoLog(const char* level, const char* tag, const char* msg) {
    char buf[256];
    snprintf(buf, sizeof(buf), "[%s] %s", tag ? tag : "DEVICE", msg ? msg : "");
    Serial.print("[");
    Serial.print(level);
    Serial.print("] ");
    Serial.println(buf);
}

void MeoLogf(const char* level, const char* tag, const char* fmt, ...) {
    char msg[192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(msg, sizeof(msg), fmt, ap);
    va_end(ap);
    MeoLog(level, tag, msg);
}