#include <assert.h>
#include <lwip/netdb.h>
#include <stdio.h>

#include "esp_console.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "cli_client.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

static const char *TAG = "cli_client";
static cli_client_config_t config;

int cli_client(const char *payload)
{
    char rx_buffer[128];

    struct sockaddr_in dest_addr;
    inet_pton(AF_INET, config.server.host, &dest_addr.sin_addr);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(config.server.port);

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (sock < 0) {
        printf("Unable to create socket: errno %d\n", errno);
        return 1;
    }
    ESP_LOGI(TAG, "Socket created, connecting to %s:%d", config.server.host, config.server.port);

    int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        printf("Socket unable to connect: errno %d\n", errno);
        return 2;
    }
    ESP_LOGI(TAG, "Successfully connected");

    err = send(sock, payload, strlen(payload), 0);
    if (err < 0) {
        printf("Error occurred during sending: errno %d\n", errno);
        return 3;
    }

    int res = 0;
    while (1) {

        int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        // Error occurred during receiving
        if (len < 0) {

            if (errno == 128) // Closed socket, cli command done.
                break;
            printf("recv failed: errno %d\n", errno);
            res = 4;
            break;
        }
        fwrite(rx_buffer, len, 1, stdout);
    }

    if (sock != -1) {
        shutdown(sock, 0);
        close(sock);
    }
    return res;
}

static int cmd_cli_client(int argc, char **argv)
{
    // Concat all argument parts into one string
    char buffer[config.client.max_cmd_len];
    memset(buffer, 0, config.client.max_cmd_len);
    for (int i = 1; i < argc; i++) {
        if ((sizeof(buffer) - strlen(buffer)) <= (strlen(argv[i]) + 2)) {
            printf("Argument to long, limit %d\n", config.client.max_cmd_len);
            return 20;
        }
        strcat(buffer, argv[i]);
        if (i != argc - 1)
            strcat(buffer, " ");
    }

    return cli_client(buffer);
}

esp_err_t cli_client_init(const cli_client_config_t *_config)
{
    config = *_config;
    const esp_console_cmd_t cli_client_cmd = {
        .command = config.client.cmd,
        .help = "Run cli client command",
        .hint = NULL,
        .func = &cmd_cli_client,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cli_client_cmd));

    return ESP_OK;
}
