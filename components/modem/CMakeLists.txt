set(srcs "src/esp_modem.c"
        "src/esp_modem_dce_service"
        "src/esp_modem_netif.c"
        "src/esp_modem_compat.c"
        "src/sim800.c"
        "src/sim7600.c"
        "src/bg96.c"
        "src/nullmodem.c")

idf_component_register(SRCS "${srcs}"
                    INCLUDE_DIRS include
                    REQUIRES driver esp_event esp_netif)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")