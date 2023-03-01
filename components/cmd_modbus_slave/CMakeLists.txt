idf_component_register(SRCS  "modbus_slave.c"
                    INCLUDE_DIRS .
                    REQUIRES console spi_flash esp_event esp-modbus modbus_params)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")