idf_component_register(SRCS "ppp_server_main.c"
                    INCLUDE_DIRS "." REQUIRES cmd_iperf cmd_ping cmd_system esp_modem esp_netif esp_event)
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
