idf_component_register(SRCS cli_server.h cli_server.c
                    INCLUDE_DIRS .
                  )
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")

