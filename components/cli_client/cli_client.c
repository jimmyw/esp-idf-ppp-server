#include <assert.h>

#include "esp_log.h"


#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include <lwip/netdb.h>
#include "cli_client.h"
#include "esp_console.h"


static const char *TAG = "cli_client";
static cli_client_config_t config;

void tcp_client(const char *payload)
{
    char rx_buffer[128];

    int addr_family = 0;
    int ip_protocol = 0;

    while (1) {

        struct sockaddr_in dest_addr;
        inet_pton(AF_INET, config.server.host, &dest_addr.sin_addr);
        dest_addr.sin_family = AF_INET;
        dest_addr.sin_port = htons(config.server.port);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;


        int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created, connecting to %s:%d", config.server.host, config.server.port);

        int err = connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Successfully connected");

        while (1) {
            int err = send(sock, payload, strlen(payload), 0);
            if (err < 0) {
                ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                break;
            }

            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occurred during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, config.server.host);
                ESP_LOGI(TAG, "%s", rx_buffer);
            }
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
        }
    }
}


static int cmd_cli_client(int argc, char **argv)
{
    // Concat all argument parts into one string
    char buffer[512] = {};
    for (int i = 1; i < argc; i++) {
        if ((sizeof(buffer) - strlen(buffer)) <= (strlen(argv[i]) + 2)) {
            printf("Argument to long, limit %d\n", sizeof(buffer));
            return 20;
        }
        strcat(buffer, argv[i]);
        if (i != argc - 1)
            strcat(buffer, " ");
    }

    tcp_client(buffer);
    return 0;
}


esp_err_t cli_client_init(const cli_client_config_t *_config)
{
    config = *_config;
    const esp_console_cmd_t cli_client_cmd = {
        .command = "cli_client",
        .help = "Run cli client command",
        .hint = NULL,
        .func = &cmd_cli_client,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&cli_client_cmd));

    return ESP_OK;
}
