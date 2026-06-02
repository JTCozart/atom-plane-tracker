#include "InsightsReporter.h"

// sizeof("") == 1 (null terminator only), so this guards against empty local builds
#if defined(ESP_INSIGHT_AUTH_KEY) && sizeof(ESP_INSIGHT_AUTH_KEY) > 1

extern "C" {
#include <esp_insights.h>
#include <esp_diagnostics.h>
}

void InsightsReporter::begin() {
    esp_insights_config_t cfg = {
        .log_type = ESP_DIAG_LOG_TYPE_ERROR | ESP_DIAG_LOG_TYPE_WARNING | ESP_DIAG_LOG_TYPE_EVENT,
        .auth_key = ESP_INSIGHT_AUTH_KEY,
    };
    esp_insights_init(&cfg);
}

#else

void InsightsReporter::begin() {}

#endif
