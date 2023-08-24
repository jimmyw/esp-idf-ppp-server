idf_component_register(SRCS cli_client.h cli_client.c
                    INCLUDE_DIRS .
                    REQUIRES console
                  )
target_compile_options(${COMPONENT_LIB} PRIVATE "-Wno-format")
