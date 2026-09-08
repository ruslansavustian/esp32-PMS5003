idf_component_register(
    SRCS "telemetry.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES wifi_manager measurements esp_http_client mbedtls esp_netif esp_timer esp_hw_support freertos log
)
