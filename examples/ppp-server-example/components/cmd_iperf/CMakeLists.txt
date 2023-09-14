idf_component_register(SRCS "cmd_iperf.c"
                    INCLUDE_DIRS .
                    REQUIRES console spi_flash iperf esp_timer)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")