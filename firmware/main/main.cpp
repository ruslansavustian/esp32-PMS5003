#include "wifi_manager.h"
#include "telemetry.h"
#include "wifi_config.generated.h"
#include "api_config.generated.h"

extern "C" void app_main()
{
    // Generated configuration contains static strings, valid for the whole boot.
    wifi_manager::start({wifi_config::kSsid, wifi_config::kPassword});
    telemetry::start({
        api_config::kEndpoint,
        api_config::kDeviceId,
        api_config::kDeviceToken,
        api_config::kIntervalSeconds,
        api_config::kAllowInsecureHttp,
    });
}
