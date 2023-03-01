#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "driver/uart.h"
#include "esp_netif_ppp.h"
#include "netif/ppp/ppp.h"
#include "ppp_link.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
static const char *TAG = "ppp_link";
static QueueHandle_t uart_event_queue = NULL;
static int current_phase = PPP_PHASE_DEAD;
static ppp_link_config_t config;

esp_err_t esp_netif_start(esp_netif_t *esp_netif);

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data) {
  if (event_id >= NETIF_PP_PHASE_OFFSET) {
    current_phase = event_id - NETIF_PP_PHASE_OFFSET;
  }
}

static esp_err_t on_ppp_transmit(void *h, void *buffer, size_t len)
{
    int written = uart_write_bytes(config.uart, buffer, len);
    if (len != written)
        ESP_LOGE(TAG, "Available bytes: %d written: %d", len, written);
    return written > 0 ? ESP_OK : ESP_FAIL;
}

static void ppp_task_thread(void *param)
{
    const esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
    esp_netif_t *esp_netif = esp_netif_new(&cfg);
    assert(esp_netif);

    const esp_netif_driver_ifconfig_t driver_ifconfig = {
        .driver_free_rx_buffer = NULL,
        .transmit = on_ppp_transmit,
    };
    ESP_ERROR_CHECK(esp_netif_set_driver_config(esp_netif, &driver_ifconfig));

    // enable both events, so we could notify the modem layer if an error occurred/state changed
    const esp_netif_ppp_config_t ppp_config = {
            .ppp_error_event_enabled = true,
            .ppp_phase_event_enabled = true
    };
    ESP_ERROR_CHECK(esp_netif_ppp_set_params(esp_netif, &ppp_config));

    if (config.type == PPP_LINK_SERVER) {
        ESP_ERROR_CHECK(esp_netif_ppp_start_server(esp_netif, config.ppp_server.localaddr, config.ppp_server.remoteaddr, config.ppp_server.dnsaddr1,
                        config.ppp_server.dnsaddr2, config.ppp_server.login, config.ppp_server.password, config.ppp_server.auth_req));
    }
    while (1) {
        uart_event_t event;

        if (xQueueReceive(uart_event_queue, &event, pdMS_TO_TICKS(100))) {
            switch (event.type) {
            case UART_DATA:
                while(true) {
                    char buffer[512];
                    size_t length = 0;

                    uart_get_buffered_data_len(config.uart, &length);
                    if (!length)
                        break;
                    //ESP_LOGV(TAG, "uart_get_buffered_data_len()=%d", length);
                    length = MIN(sizeof(buffer), length);
                    size_t read_length = uart_read_bytes(config.uart, buffer, length, portMAX_DELAY);
                    if (read_length) {
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
            ESP_ERROR_CHECK(esp_netif_start(esp_netif));
        }
    }
}

esp_err_t ppp_link_init(const ppp_link_config_t *_config) {
    config = *_config;

    ESP_ERROR_CHECK(uart_param_config(config.uart, &config.uart_config));

    ESP_ERROR_CHECK(uart_set_pin(config.uart, config.io.tx, config.io.rx,
                           config.io.rts, config.io.cts));

    ESP_ERROR_CHECK(uart_set_hw_flow_ctrl(config.uart, UART_HW_FLOWCTRL_CTS_RTS, UART_FIFO_LEN - 8));

    ESP_ERROR_CHECK(uart_driver_install(config.uart, config.buffer.rx_buffer_size, config.buffer.tx_buffer_size,
                              config.buffer.rx_queue_size, &uart_event_queue, 0));

    ESP_ERROR_CHECK(uart_set_rx_timeout(config.uart, 1));

    ESP_ERROR_CHECK(uart_set_rx_full_threshold(config.uart, 64));


    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID,
                                             &on_ppp_changed, NULL));

    BaseType_t ret = xTaskCreate(ppp_task_thread, "ppp_task", 1024 * 3, NULL, 100, NULL);
    assert(ret == pdTRUE);

    return ESP_OK;
}