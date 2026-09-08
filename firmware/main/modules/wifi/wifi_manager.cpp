#include "modules/wifi/wifi_manager.h"

#include <cstring>
#include <atomic>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "wifi_config.generated.h"

namespace {
constexpr char kTag[] = "wifi_manager";
constexpr unsigned kMaxRetries = 5;
unsigned retryCount = 0;
esp_timer_handle_t retryTimer = nullptr;
std::atomic<bool> connected{false};

void connect(void*)
{
    const esp_err_t result = esp_wifi_connect();
    if (result != ESP_OK) {
        ESP_LOGE(kTag, "Connection request failed: %s. Restart to try again.",
                 esp_err_to_name(result));
    }
}

void onEvent(void*, esp_event_base_t base, int32_t id, void* data)
{
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_START) {
        ESP_LOGI(kTag, "Connecting to configured Wi-Fi...");
        connect(nullptr);
    } else if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        connected.store(false);
        const auto* event = static_cast<wifi_event_sta_disconnected_t*>(data);
        ESP_LOGW(kTag, "Disconnected (reason=%u).", static_cast<unsigned>(event->reason));
        if (retryCount >= kMaxRetries) {
            ESP_LOGE(kTag, "Retry limit reached. Check network/configuration and restart.");
            return;
        }
        const unsigned delaySeconds = 1U << retryCount;
        ++retryCount;
        ESP_LOGI(kTag, "Retry %u/%u in %u seconds.", retryCount, kMaxRetries, delaySeconds);
        ESP_ERROR_CHECK(esp_timer_start_once(retryTimer, delaySeconds * 1000000ULL));
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        const auto* event = static_cast<ip_event_got_ip_t*>(data);
        retryCount = 0;
        connected.store(true);
        ESP_LOGI(kTag, "Wi-Fi connected. IPv4: " IPSTR, IP2STR(&event->ip_info.ip));
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        connected.store(false);
    }
}
}

namespace wifi_manager {
bool isConnected()
{
    return connected.load();
}

void start()
{
    // Validate before starting Wi-Fi; do not log SSID or password.
    constexpr size_t ssidLength = sizeof(wifi_config::kSsid) - 1;
    constexpr size_t passwordLength = sizeof(wifi_config::kPassword) - 1;
    static_assert(ssidLength <= 32, "Wi-Fi SSID must not exceed 32 bytes");
    static_assert(passwordLength <= 63, "Wi-Fi passphrase must not exceed 63 bytes");
    if (ssidLength == 0 || passwordLength < 8) {
        ESP_LOGW(kTag, "Configure main/config/wifi_config.local.h: SSID and 8-63 byte passphrase; rebuild and flash.");
        return;
    }

    // NVS is required by the driver. Never erase stored data automatically.
    ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    ESP_ERROR_CHECK(esp_netif_create_default_wifi_sta() ? ESP_OK : ESP_ERR_NO_MEM);

    wifi_init_config_t initConfig = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&initConfig));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

    esp_timer_create_args_t timerConfig = {};
    timerConfig.callback = connect;
    timerConfig.name = "wifi_retry";
    ESP_ERROR_CHECK(esp_timer_create(&timerConfig, &retryTimer));

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, onEvent, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, onEvent, nullptr));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, onEvent, nullptr));

    wifi_config_t config = {};
    std::memcpy(config.sta.ssid, wifi_config::kSsid, ssidLength);
    std::memcpy(config.sta.password, wifi_config::kPassword, passwordLength);
    config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    config.sta.pmf_cfg.capable = true;
    config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));
    ESP_ERROR_CHECK(esp_wifi_start());
    // No while loop: the driver tasks and event handlers continue running.
}
}
