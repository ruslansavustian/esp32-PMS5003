#include "wifi_manager.h"
#include "telemetry.h"

extern "C" void app_main()
{
    wifi_manager::start();
    telemetry::start();
}
