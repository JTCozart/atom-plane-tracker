#pragma once
#include <Arduino.h>

class AnalyticsReporter {
public:
    static void reportBoot();
    static void reportHeartbeat();
    static void reportError(const char* tag, const char* message);

private:
    static void send(const char* event, const char* extraProps = nullptr);
};
