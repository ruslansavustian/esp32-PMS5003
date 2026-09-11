idf_component_register(
    SRCS "pms5003_source.cpp" "pms5003_parser.cpp"
    INCLUDE_DIRS "include"
    PRIV_REQUIRES esp_driver_uart esp_driver_gpio esp_timer freertos log
)
