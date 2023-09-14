idf_component_register(SRCS "ppp_link.c"
                    INCLUDE_DIRS .
                    REQUIRES driver esp_netif)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
