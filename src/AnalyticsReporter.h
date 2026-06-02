#pragma once
#include <Arduino.h>

class AnalyticsReporter {
public:
    static void reportBoot();
    static void reportHeartbeat();

private:
    static void send(const char* event);
};
