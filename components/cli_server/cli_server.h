#ifndef __CLI_SERVER_H_
#define __CLI_SERVER_H_

#include "esp_err.h"

struct cli_server_config_s {
    struct {
        int stack_size;
        int prio;
    } task;
    struct {
        int port;
        int max_arg_len;
    } server;
};

// clang-format off
#define DEFAULT_CLI_SERVER_CONFIG {   \
    .task = {                         \
        .stack_size = (5 * 1024),     \
        .prio = 100,                  \
    },                                \
    .server = {                       \
        .port = 1000,                 \
        .max_arg_len = 1024,          \
    }                                 \
  };

// clang-format on

typedef struct cli_server_config_s cli_server_config_t;

esp_err_t cli_server_init(const cli_server_config_t *cli_server_config);

#endif /* __CLI_SERVER_H_ */
