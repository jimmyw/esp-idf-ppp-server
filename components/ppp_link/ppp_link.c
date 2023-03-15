#include "ppp_link.h"
#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"

#include "driver/uart.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"

#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "netif/ppp/ppp.h"

#define PPP_LINK_TASK_STACK_SIZE (3 * 1024)
#define PPP_LINK_TASK_PRIO 100

#define MAX_PPP_FRAME_SIZE (PPP_MAXMRU + 10) // 10 bytes of ppp framing around max 1500 bytes information

static const char *TAG = "ppp_link";
static QueueHandle_t uart_event_queue;
static int current_phase = PPP_PHASE_DEAD;
static ppp_link_config_t config;

static void on_ppp_changed(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id >= NETIF_PP_PHASE_OFFSET) {
        current_phase = event_id - NETIF_PP_PHASE_OFFSET;
    }
}

static esp_err_t on_ppp_transmit(void *h, void *buffer, size_t len)
{
    size_t free_size = 0;
    ESP_ERROR_CHECK(uart_get_tx_buffer_free_size(config.uart, &free_size));

    if (unlikely(free_size < len)) {
        // ESP_LOGW(TAG, "Uart TX buffer full. free_size: %d len: %d", free_size, len);
        return ESP_FAIL;
    }
    int written = uart_write_bytes(config.uart, buffer, len);
    if (unlikely(len != written)) {
        ESP_LOGE(TAG, "Failed to write bytes. bytes: %d free: %d written: %d", len, free_size, written);
        abort();
    }
    return ESP_OK;
}

static void ppp_task_thread(void *param)
{
    const esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&cfg);
    assert(esp_netif);

    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_GOT_IP, esp_netif_action_connected, esp_netif));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_PPP_LOST_IP, esp_netif_action_disconnected, esp_netif));

    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = NULL,
        .transmit = on_ppp_transmit,
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    // enable both events, so we could notify the modem layer if an error occurred/state changed
    const esp_netif_ppp_config_t ppp_config = {.ppp_error_event_enabled = true, .ppp_phase_event_enabled = true};
    ESP_ERROR_CHECK(esp_netif_ppp_set_params(esp_netif, &ppp_config));

#ifdef CONFIG_PPP_SERVER_SUPPORT
    if (config.type == PPP_LINK_SERVER) {
        ESP_ERROR_CHECK(esp_netif_ppp_start_server(esp_netif, config.ppp_server.localaddr, config.ppp_server.remoteaddr, config.ppp_server.dnsaddr1,
                                                   config.ppp_server.dnsaddr2, config.ppp_server.login, config.ppp_server.password,
                                                   config.ppp_server.auth_req));
    }
#endif

    while (1) {
        uart_event_t event;

        if (xQueueReceive(uart_event_queue, &event, pdMS_TO_TICKS(100))) {
            switch (event.type) {
            case UART_DATA:
                while (true) {
                    char buffer[512];
                    size_t length = 0;

                    uart_get_buffered_data_len(config.uart, &length);
                    if (!length)
                        break;

                    length = MIN(sizeof(buffer), length);
                    size_t read_length = uart_read_bytes(config.uart, buffer, length, portMAX_DELAY);
                    if (read_length > 0) {
                        esp_err_t res = esp_netif_receive(esp_netif, buffer, read_length, NULL);
                        if (res != ESP_OK) {
                            ESP_LOGE(TAG, "esp_netif_receive error %d", res);
                            break;
                        }
                    }
                }
                break;
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "HW FIFO Overflow");
                uart_flush_input(config.uart);
                xQueueReset(uart_event_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring Buffer Full");
                uart_flush_input(config.uart);
                xQueueReset(uart_event_queue);
                break;
            case UART_BREAK:
                ESP_LOGW(TAG, "Rx Break");
                break;
            case UART_PARITY_ERR:
                ESP_LOGE(TAG, "Parity Error");
                break;
            case UART_FRAME_ERR:
                ESP_LOGE(TAG, "Frame Error");
                break;
            default:
                ESP_LOGW(TAG, "unknown uart event type: %d", event.type);
                break;
            }
        }

        if (current_phase == PPP_PHASE_DEAD) {
            ESP_LOGI(TAG, "Connection is dead, restarting ppp interface");
            esp_netif_action_start(esp_netif, NULL, 0, NULL);
        }
    }
}

esp_err_t ppp_link_init(const ppp_link_config_t *_config)
{
    config = *_config;

    // Tx buffer needs to be able to contain at least 1 full frame.
    assert(config.buffer.tx_buffer_size >= MAX_PPP_FRAME_SIZE);

    ESP_ERROR_CHECK(uart_param_config(config.uart, &config.uart_config));

    ESP_ERROR_CHECK(uart_set_pin(config.uart, config.io.tx, config.io.rx, config.io.rts, config.io.cts));

    ESP_ERROR_CHECK(
        uart_driver_install(config.uart, config.buffer.rx_buffer_size, config.buffer.tx_buffer_size, config.buffer.rx_queue_size, &uart_event_queue, 0));

    ESP_ERROR_CHECK(uart_set_rx_timeout(config.uart, 1));

    ESP_ERROR_CHECK(uart_set_rx_full_threshold(config.uart, 64));

    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID, &on_ppp_changed, NULL));

    BaseType_t ret = xTaskCreate(ppp_task_thread, "ppp_task", PPP_LINK_TASK_STACK_SIZE, NULL, PPP_LINK_TASK_PRIO, NULL);
    assert(ret == pdTRUE);

    return ESP_OK;
}
