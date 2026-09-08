#include "modules/wifi/wifi_manager.h"
#include "modules/telemetry/telemetry.h"

extern "C" void app_main()
{
    wifi_manager::start();
    telemetry::start();
}
