# Local config is copied into build/generated, never into tracked source files.
if(NOT CMAKE_SCRIPT_MODE_FILE)
    file(GLOB local_configs CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/config/*.local.h")
    foreach(config_name wifi api)
        set(config_source "${CMAKE_CURRENT_SOURCE_DIR}/config/${config_name}_config.example.h")
        if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/config/${config_name}_config.local.h")
            set(config_source "${CMAKE_CURRENT_SOURCE_DIR}/config/${config_name}_config.local.h")
        endif()
        configure_file("${config_source}"
                       "${CMAKE_CURRENT_BINARY_DIR}/generated/${config_name}_config.generated.h" COPYONLY)
    endforeach()
endif()

idf_component_register(
    SRCS
        "main.cpp"
        "modules/wifi/wifi_manager.cpp"
        "modules/telemetry/telemetry.cpp"
        "modules/measurements/mock_measurement_source.cpp"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_BINARY_DIR}/generated"
    PRIV_REQUIRES esp_wifi esp_event esp_netif nvs_flash esp_timer esp_http_client mbedtls
)
