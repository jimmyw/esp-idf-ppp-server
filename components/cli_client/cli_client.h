#ifndef __CLI_CLIENT_H_
#define __CLI_CLIENT_H_

#include "esp_err.h"

struct cli_client_config_s {
    struct {
        int port;
        const char *host;
    } server;
};

// clang-format off
#define DEFAULT_CLI_CLIENT_CONFIG {   \
    .server = {                       \
        .port = 1000,                 \
        .host = "10.10.0.2",          \
    }                                 \
  };

// clang-format on

typedef struct cli_client_config_s cli_client_config_t;

esp_err_t cli_client_init(const cli_client_config_t *cli_client_config);

#endif /* __CLI_CLIENT_H_ */
