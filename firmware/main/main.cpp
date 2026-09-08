#include <cinttypes>

#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char kTag[] = "lesson_01";
constexpr uint32_t kIntervalMs = 2000;
}

// ESP-IDF вызывает эту точку входа из C, поэтому используем extern "C".
extern "C" void app_main()
{
    ESP_LOGI(kTag, "Hello, Ruslan! Firmware lesson-01 v1 started.");

    uint32_t counter = 0;
    while (true) {
        const int64_t uptimeSeconds = esp_timer_get_time() / 1000000;
        ESP_LOGI(kTag, "counter=%" PRIu32 " | uptime=%" PRId64 " s",
                 counter, uptimeSeconds);
        ++counter;

        // Задача уступает процессор планировщику FreeRTOS на время ожидания.
        vTaskDelay(pdMS_TO_TICKS(kIntervalMs));
    }
}
