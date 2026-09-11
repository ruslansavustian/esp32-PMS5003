#include "telemetry.h"
#include "measurement_source.h"
#include "wifi_manager.h"

#include <cinttypes>
#include <cstdio>
#include <cstring>
#include <ctime>

#include "esp_crt_bundle.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include "esp_random.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

namespace {
constexpr char kTag[] = "telemetry";
constexpr char kFirmwareVersion[] = "0.3.0-pms5003";
telemetry::Config settings = {};
bool started = false;

bool isIdentifier(const char* value, size_t minimum, size_t maximum)
{
    const size_t length = std::strlen(value);
    if (length < minimum || length > maximum) return false;
    for (size_t i = 0; i < length; ++i) {
        const char c = value[i];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
              (c >= '0' && c <= '9') || c == '_' || c == '-')) return false;
    }
    return true;
}

bool clockReady()
{
    return std::time(nullptr) >= 1704067200; // 2024-01-01 UTC
}

void post(const char* body, int length, uint32_t sequence)
{
    esp_http_client_config_t config = {};
    config.url = settings.endpoint;
    config.method = HTTP_METHOD_POST;
    config.timeout_ms = 10000;
    config.crt_bundle_attach = esp_crt_bundle_attach;
    config.disable_auto_redirect = true; // Never forward credentials to a redirect target.
    const auto client = esp_http_client_init(&config);
    if (!client) {
        ESP_LOGE(kTag, "Could not allocate HTTP client; sample dropped.");
        return;
    }

    char authorization[136];
    std::snprintf(authorization, sizeof(authorization), "Bearer %s", settings.deviceToken);
    esp_err_t result = esp_http_client_set_header(client, "Content-Type", "application/json");
    if (result == ESP_OK) result = esp_http_client_set_header(client, "Authorization", authorization);
    if (result == ESP_OK) result = esp_http_client_set_post_field(client, body, length);
    if (result == ESP_OK) result = esp_http_client_perform(client);
    if (result == ESP_OK) {
        const int status = esp_http_client_get_status_code(client);
        if (status == 202) {
            ESP_LOGI(kTag, "PMS5003 sample sequence=%" PRIu32 " accepted (HTTP 202).", sequence);
        } else {
            ESP_LOGW(kTag, "HTTP %d; sample not confirmed. Check API configuration.", status);
        }
    } else {
        ESP_LOGW(kTag, "HTTP request failed: %s; sample not retried.", esp_err_to_name(result));
    }
    esp_http_client_cleanup(client);
}

void run(void*)
{
    unsigned char randomBytes[16];
    esp_fill_random(randomBytes, sizeof(randomBytes));
    char bootId[33];
    for (size_t i = 0; i < sizeof(randomBytes); ++i) {
        std::snprintf(bootId + i * 2, 3, "%02x", randomBytes[i]);
    }
    uint32_t sequence = 0;
    bool sntpStarted = false;
    const bool https = std::strncmp(settings.endpoint, "https://", 8) == 0;

    while (true) {
        if (!wifi_manager::isConnected()) {
            ESP_LOGI(kTag, "Waiting for Wi-Fi IPv4...");
        } else {
            if (!sntpStarted) {
                esp_sntp_config_t timeConfig = ESP_NETIF_SNTP_DEFAULT_CONFIG("pool.ntp.org");
                const esp_err_t result = esp_netif_sntp_init(&timeConfig);
                sntpStarted = result == ESP_OK;
                if (!sntpStarted) ESP_LOGW(kTag, "SNTP init failed: %s", esp_err_to_name(result));
            }
            if (https && !clockReady()) {
                ESP_LOGI(kTag, "Waiting for clock sync before verified HTTPS...");
            } else {
                measurement_source::Reading reading{};
                if (!measurement_source::read(reading)) {
                    ESP_LOGW(kTag, "Skipping send: %s",
                             measurement_source::statusName(measurement_source::status()));
                    vTaskDelay(pdMS_TO_TICKS(settings.intervalSeconds * 1000));
                    continue;
                }
                char measuredAt[32] = "null";
                if (clockReady()) {
                    // Preserve sample age rather than labelling a cached sample as measured now.
                    const std::time_t now = std::time(nullptr) -
                        (esp_timer_get_time() - reading.receivedAtUs) / 1000000;
                    std::tm utc = {};
                    gmtime_r(&now, &utc);
                    std::strftime(measuredAt, sizeof(measuredAt), "\"%Y-%m-%dT%H:%M:%SZ\"", &utc);
                }
                const int64_t uptimeMs = reading.receivedAtUs / 1000;
                char body[768];
                const int length = std::snprintf(body, sizeof(body),
                    "{\"schemaVersion\":1,\"deviceId\":\"%s\",\"source\":\"%s\","
                    "\"bootId\":\"%s\",\"sequence\":%" PRIu32 ",\"measuredAt\":%s,"
                    "\"uptimeMs\":%" PRId64 ",\"windowSeconds\":0,\"sampleCount\":1,"
                    "\"pm1_ug_m3\":%.1f,\"pm25_ug_m3\":%.1f,\"pm10_ug_m3\":%.1f,"
                    "\"firmwareVersion\":\"%s\"}",
                    settings.deviceId, reading.source, bootId, sequence, measuredAt, uptimeMs,
                    static_cast<double>(reading.pm1), static_cast<double>(reading.pm25),
                    static_cast<double>(reading.pm10), kFirmwareVersion);
                if (length > 0 && static_cast<size_t>(length) < sizeof(body)) {
                    post(body, length, sequence);
                } else {
                    ESP_LOGE(kTag, "Payload does not fit buffer; sample dropped.");
                }
                ++sequence;
            }
        }
        // A fresh sample on each cycle. No backlog/retry queue in this first iteration.
        vTaskDelay(pdMS_TO_TICKS(settings.intervalSeconds * 1000));
    }
}
}

namespace telemetry {
void start(const Config& config)
{
    if (started) return;
    if (!config.endpoint || !config.deviceId || !config.deviceToken ||
        config.intervalSeconds < 5 || config.intervalSeconds > 3600) {
        ESP_LOGW(kTag, "Invalid telemetry configuration; interval must be 5-3600 seconds.");
        return;
    }
    settings = config;
    const bool https = std::strncmp(settings.endpoint, "https://", 8) == 0;
    const bool http = std::strncmp(settings.endpoint, "http://", 7) == 0;
    if (!(https || (http && settings.allowInsecureHttp)) ||
        !isIdentifier(settings.deviceId, 1, 64) ||
        !isIdentifier(settings.deviceToken, 32, 128)) {
        ESP_LOGW(kTag, "Invalid endpoint/device token; telemetry disabled.");
        return;
    }
    if (http) ESP_LOGW(kTag, "Explicit LAN HTTP mode: transport is unencrypted.");
    // Synchronous TLS can spend seconds in CPU-bound ECDH calculations.
    // Share priority with IDLE so FreeRTOS time slicing lets its watchdog hook run.
    // A delay after perform() cannot help while the TLS call is still running.
    if (xTaskCreate(run, "telemetry", 12288, nullptr, tskIDLE_PRIORITY, nullptr) != pdPASS) {
        ESP_LOGE(kTag, "Could not start telemetry task.");
    } else {
        started = true;
    }
}
}
