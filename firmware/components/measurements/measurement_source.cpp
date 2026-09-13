#include "measurement_source.h"
#include "esp_log.h"
#include "pms5003_parser.h"
#include "sample_store.h"

#include <cinttypes>
#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

// Приём датчика: вспомогательные данные и задача чтения.
namespace {
constexpr auto kPort = UART_NUM_2;
constexpr char kTag[] = "pms5003";
constexpr int kBaudRate = 9600;
constexpr int kRxBufferBytes = 2048;
constexpr int kTxBufferBytes = 0;
constexpr int kEventQueueSize = 32;
constexpr size_t kReadBufferBytes = 128;
constexpr uint32_t kTaskStackBytes = 4096;
constexpr UBaseType_t kTaskPriority = 5;
constexpr uint32_t kEventWaitMs = 1000;
constexpr int64_t kFrameGapTimeoutUs = 1'000'000;
constexpr int64_t kLogIntervalUs = 5'000'000;
QueueHandle_t events = nullptr;
portMUX_TYPE lock = portMUX_INITIALIZER_UNLOCKED;
pms5003::SampleStore samples;
bool started = false;

void invalidate() {
    portENTER_CRITICAL(&lock);
    samples.invalidate();
    portEXIT_CRITICAL(&lock);
}

void run(void *) {
    pms5003::Parser parser;
    uint8_t readBuffer[kReadBufferBytes];
    int64_t lastDataAtUs = 0;
    int64_t lastReportAtUs = 0;
    uint32_t frames = 0;
    uint32_t uart_errors = 0;
    while (true) {
        uart_event_t event{};
        if (xQueueReceive(events, &event, pdMS_TO_TICKS(kEventWaitMs)) == pdTRUE) {
            if (event.type == UART_DATA) {
                const auto nowUs = esp_timer_get_time();
                // Do not join a long-abandoned fragment to a later transmission.
                if (nowUs - lastDataAtUs >= kFrameGapTimeoutUs) {
                    parser.reset();
                }
                lastDataAtUs = nowUs;
                size_t bytesRemaining = event.size;
                while (bytesRemaining > 0) {
                    const size_t bytesToRead =
                        bytesRemaining < sizeof(readBuffer) ? bytesRemaining : sizeof(readBuffer);
                    const int bytesRead = uart_read_bytes(kPort, readBuffer, bytesToRead, 0);
                    if (bytesRead <= 0) {
                        ++uart_errors;
                        parser.reset();
                        invalidate();
                        uart_flush_input(kPort);
                        xQueueReset(events);
                        break;
                    }
                    bytesRemaining -= static_cast<size_t>(bytesRead);
                    for (int i = 0; i < bytesRead; ++i) {
                        pms5003::Sample sample{};
                        if (parser.push(readBuffer[i], sample)) {
                            ++frames;
                            portENTER_CRITICAL(&lock);
                            samples.accept(sample, esp_timer_get_time());
                            portEXIT_CRITICAL(&lock);
                        }
                    }
                }
            } else if (event.type == UART_FIFO_OVF || event.type == UART_BUFFER_FULL ||
                       event.type == UART_FRAME_ERR || event.type == UART_PARITY_ERR ||
                       event.type == UART_BREAK) {
                ++uart_errors;
                // Lost/corrupted UART bytes: discard backlog and require stabilization again.
                uart_flush_input(kPort);
                xQueueReset(events);
                parser.reset();
                invalidate();
            }
        }
        const auto nowUs = esp_timer_get_time();
        if (nowUs - lastReportAtUs >= kLogIntervalUs) {
            measurement_source::Reading reading{};
            if (measurement_source::read(reading)) {
                ESP_LOGI(kTag,
                         "PM1.0=%.0f PM2.5=%.0f PM10=%.0f ug/m3 (atmospheric)",
                         static_cast<double>(reading.pm1),
                         static_cast<double>(reading.pm25),
                         static_cast<double>(reading.pm10));
            } else {
                ESP_LOGW(kTag,
                         "No publishable sample: %s",
                         measurement_source::statusName(measurement_source::status()));
            }
            ESP_LOGI(kTag,
                     "frames=%" PRIu32 " bad_length=%" PRIu32 " bad_checksum=%" PRIu32
                     " uart_errors=%" PRIu32,
                     frames,
                     parser.bad_lengths(),
                     parser.bad_checksums(),
                     uart_errors);
            lastReportAtUs = nowUs;
        }
    }
}
} // namespace

namespace measurement_source {
bool start(const Config &config) {
    if (started) {
        return true;
    }
    if (!GPIO_IS_VALID_GPIO(config.rx_gpio)) {
        return false;
    }
    uart_config_t uart{};
    uart.baud_rate = kBaudRate;
    uart.data_bits = UART_DATA_8_BITS;
    uart.parity = UART_PARITY_DISABLE;
    uart.stop_bits = UART_STOP_BITS_1;
    uart.flow_ctrl = UART_HW_FLOWCTRL_DISABLE;
    uart.source_clk = UART_SCLK_DEFAULT;
    esp_err_t error =
        uart_driver_install(kPort, kRxBufferBytes, kTxBufferBytes, kEventQueueSize, &events, 0);
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "UART driver: %s", esp_err_to_name(error));
        return false;
    }
    error = uart_param_config(kPort, &uart);
    if (error == ESP_OK) {
        error = uart_set_pin(kPort,
                             UART_PIN_NO_CHANGE,
                             config.rx_gpio,
                             UART_PIN_NO_CHANGE,
                             UART_PIN_NO_CHANGE,
                             UART_PIN_NO_CHANGE,
                             UART_PIN_NO_CHANGE);
    }
    if (error == ESP_OK) {
        error = gpio_set_pull_mode(static_cast<gpio_num_t>(config.rx_gpio), GPIO_PULLUP_ONLY);
    }
    if (error == ESP_OK &&
        xTaskCreate(run, "pms5003", kTaskStackBytes, nullptr, kTaskPriority, nullptr) != pdPASS) {
        error = ESP_ERR_NO_MEM;
    }
    if (error != ESP_OK) {
        ESP_LOGE(kTag, "Sensor start: %s", esp_err_to_name(error));
        uart_driver_delete(kPort);
        events = nullptr;
        return false;
    }
    started = true;
    ESP_LOGI(kTag, "UART2 RX GPIO%d, 9600 8N1; stabilization 30s, stale after 10s", config.rx_gpio);
    return true;
}

bool read(Reading &out) {
    portENTER_CRITICAL(&lock);
    const bool available = samples.read(esp_timer_get_time(), out);
    portEXIT_CRITICAL(&lock);
    return available;
}

Status status() {
    portENTER_CRITICAL(&lock);
    const auto result = samples.status(esp_timer_get_time());
    portEXIT_CRITICAL(&lock);
    return result;
}

const char *statusName(Status value) {
    switch (value) {
    case Status::Waiting:
        return "waiting for valid frames";
    case Status::WarmingUp:
        return "warming up";
    case Status::Ready:
        return "ready";
    case Status::Stale:
        return "stale (no valid frames for 10s)";
    }
    return "unknown";
}
} // namespace measurement_source
