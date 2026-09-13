#include "wifi_manager.h"
#include "telemetry.h"
#include "wifi_config.generated.h"
#include "api_config.generated.h"
#include "measurement_source.h"
#include "esp_log.h"
#include "config/sensor_config.h"

// Точка входа: ESP-IDF вызывает эту функцию после подготовки системы.
extern "C" void app_main()
{
    if (!measurement_source::start({sensor_config::kRxGpio})) {
        ESP_LOGE("main", "PMS5003 startup failed; telemetry not started");
        return;
    }
    wifi_manager::start({wifi_config::kSsid, wifi_config::kPassword});
    telemetry::start({
        api_config::kEndpoint,
        api_config::kDeviceId,
        api_config::kDeviceToken,
        api_config::kIntervalSeconds,
        api_config::kAllowInsecureHttp,
    });
}
