#include <assert.h>
#include <lwip/netdb.h>

#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "linenoise/linenoise.h"

#include "cli_common.h"
#include "cli_server.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"

static const char *TAG = "cli_server";
static cli_server_config_t config;

static int console_write(void *c, const char *data, int len)
{
    int sock = (int)c;
    // send() can return less bytes than supplied length.
    // Walk-around for robust implementation.
    int to_write = len;
    while (to_write > 0) {
        int written = send(sock, data + (len - to_write), to_write, 0);
        ESP_LOGW(TAG, "%d bytes sent", written);
        if (written < 0) {
            ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
            // Failed to retransmit, giving up
            return 0;
        }
        to_write -= written;
    }
    return len;
}

static int console_read(void *c, char *data, int len)
{
    ESP_LOGI(TAG, "Read %d bytes", len);
    int sock = (int)c;
    int res = recv(sock, data, len, 0);
    if (res > 0) {
        ESP_LOGW(TAG ,"Got: '%.*s'", data, len);
    }
    return res;
}

static int linenoiseDumb(char* buf, size_t buflen, const char* prompt) {
    /* dumb terminal, fall back to fgets */
    fputs(prompt, stdout);
    size_t count = 0;
    while (count < buflen) {
        int c = fgetc(stdin);
        if (c == '\n') {
            break;
        } else if (c >= 0x1c && c <= 0x1f){
            continue; /* consume arrow keys */
        } else if (c == 127 || c == 0x8) {
            if (count > 0) {
                buf[count - 1] = 0;
                count --;
            }
            fputs("\x08 ", stdout); /* Windows CMD: erase symbol under cursor */
        } else {
            buf[count] = c;
            ++count;
        }
        fputc(c, stdout); /* echo */
    }
    fputc('\n', stdout);
    fflush(stdout);
    return count;
}

static void sanitize(char* src) {
    char* dst = src;
    for (int c = *src; c != 0; src++, c = *src) {
        if (isprint(c)) {
            *dst = c;
            ++dst;
        }
    }
    *dst = 0;
}

static void cli_server_run(const int sock)
{
    FILE *orig_stdin = __getreent()->_stdin;
    FILE *orig_stdout = __getreent()->_stdout;
    FILE *orig_stderr = __getreent()->_stderr;

    FILE *remote_console = funopen((void *)sock, &console_read, &console_write, NULL, NULL);

    __getreent()->_stdin = remote_console;
    __getreent()->_stdout = remote_console;
    __getreent()->_stderr = remote_console;

    char buffer[1024];
    while (1) {
        ESP_LOGE(TAG, "Reading data from sock: %d", sock);
        int count = linenoiseDumb(buffer, sizeof(buffer), "cli_server>");
        if (count <= 0) {
            ESP_LOGE(TAG, "No linenoise");
            break;
        }
        sanitize(buffer);
        ESP_LOGW(TAG, "line: '%s'", buffer);

        printf("\n"); // Extra newline.

        run_multiple_commands(buffer, false);


        // Just run one command and quit.
        break;
    }
    ESP_LOGE(TAG, "Finished sock: %d", sock);

    fflush(remote_console);
    __getreent()->_stdin = orig_stdin;
    __getreent()->_stdout = orig_stdout;
    __getreent()->_stderr = orig_stderr;
    fclose(remote_console);
}

static void cli_task_thread(void *param)
{

    struct sockaddr_storage dest_addr;

    struct sockaddr_in *dest_addr_ip4 = (struct sockaddr_in *)&dest_addr;
    dest_addr_ip4->sin_addr.s_addr = htonl(INADDR_ANY);
    dest_addr_ip4->sin_family = AF_INET;
    dest_addr_ip4->sin_port = htons(config.server.port);

    int listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (listen_sock < 0) {
        ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
        vTaskDelete(NULL);
        return;
    }
    int opt = 1;
    setsockopt(listen_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    ESP_LOGD(TAG, "Socket created");

    int err = bind(listen_sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err != 0) {
        ESP_LOGE(TAG, "Socket unable to bind: errno %d", errno);
        ESP_LOGE(TAG, "IPPROTO: %d", AF_INET);
        goto CLEAN_UP;
    }
    ESP_LOGD(TAG, "Socket bound, port %d", config.server.port);

    err = listen(listen_sock, 1);
    if (err != 0) {
        ESP_LOGE(TAG, "Error occurred during listen: errno %d", errno);
        goto CLEAN_UP;
    }

    while (1) {
        struct sockaddr_storage source_addr; // Large enough for both IPv4 or IPv6
        socklen_t addr_len = sizeof(source_addr);
        int sock = accept(listen_sock, (struct sockaddr *)&source_addr, &addr_len);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to accept connection: errno %d", errno);
            break;
        }

        // Convert ip address to string
        {
            char addr_str[128];
            if (source_addr.ss_family == PF_INET) {
                inet_ntoa_r(((struct sockaddr_in *)&source_addr)->sin_addr, addr_str, sizeof(addr_str) - 1);
            }
            ESP_LOGD(TAG, "Socket accepted ip address: %s", addr_str);
        }

        cli_server_run(sock);

        shutdown(sock, 0);
        close(sock);
    }

CLEAN_UP:
    close(listen_sock);
    vTaskDelete(NULL);
}

esp_err_t cli_server_init(const cli_server_config_t *_config)
{
    config = *_config;

    BaseType_t ret = xTaskCreate(cli_task_thread, "cli_server_task", config.task.stack_size, NULL, config.task.prio, NULL);
    assert(ret == pdTRUE);

    return ESP_OK;
}
