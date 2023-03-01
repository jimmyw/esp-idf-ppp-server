idf_component_register(SRCS "modbus_params.c" 
                    INCLUDE_DIRS .
                    REQUIRES console spi_flash esp_event esp-modbus)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")