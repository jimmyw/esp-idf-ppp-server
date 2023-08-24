idf_component_register(SRCS cli_server.h cli_server.c cli_common.c
                    INCLUDE_DIRS .
                    REQUIRES console
                  )
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

