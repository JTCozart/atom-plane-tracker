#include "InsightsReporter.h"

#ifndef ESP_INSIGHT_AUTH_KEY
#define ESP_INSIGHT_AUTH_KEY ""
#endif

extern "C" {
#include <esp_insights.h>
}

void InsightsReporter::begin() {
    static constexpr char kKey[] = ESP_INSIGHT_AUTH_KEY;
    if (kKey[0] == '\0') return;

    esp_insights_config_t cfg = {
        .log_type = ESP_DIAG_LOG_TYPE_ERROR | ESP_DIAG_LOG_TYPE_WARNING | ESP_DIAG_LOG_TYPE_EVENT,
        .auth_key = kKey,
    };
    esp_insights_init(&cfg);
}
