idf_component_register(SRCS "iperf.c"
                    INCLUDE_DIRS "include"
                    REQUIRES lwip esp_timer esp_netif)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")