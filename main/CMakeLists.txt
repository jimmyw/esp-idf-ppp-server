idf_component_register(SRCS "ppp_server_main.c" "ppp_server.c"
                    INCLUDE_DIRS ".")
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
