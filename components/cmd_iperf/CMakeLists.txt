idf_component_register(SRCS "cmd_iperf.c"
                    INCLUDE_DIRS .
                    REQUIRES console spi_flash iperf)
