idf_component_register(SRCS "cmd_ping.c"
                    INCLUDE_DIRS .
                    REQUIRES console spi_flash esp_event)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")