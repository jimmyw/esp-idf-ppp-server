#include "cmd_iperf.h"
#include "cmd_ping.h"
#include "cmd_system.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "driver/uart.h"
#include "esp_netif_ppp.h"
#include "netif/ppp/ppp.h"
#include "ppp_server.h"

static const char *TAG = "ppp_server";
static QueueHandle_t uart_event_queue = NULL;
static int current_phase = PPP_PHASE_DEAD;

esp_err_t esp_netif_start(esp_netif_t *esp_netif);

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data) {
  if (event_id >= NETIF_PP_PHASE_OFFSET) {
    current_phase = event_id - NETIF_PP_PHASE_OFFSET;
  }
}

static esp_err_t on_ppp_transmit(void *h, void *buffer, size_t len)
{
    //ESP_LOGD(TAG, "write_bytes: %d", len);
    return uart_write_bytes(UART_NUM_1, buffer, len);
}

static void ppp_task_thread(void *param)
{

    ESP_LOGD(TAG, "ppp_task_thread_init");

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

    ESP_LOGI(TAG, "Will configure as PPP SERVER");
    esp_ip4_addr_t localaddr;
    esp_netif_set_ip4_addr(&localaddr, 10, 10, 0, 1);
    esp_ip4_addr_t remoteaddr;
    esp_netif_set_ip4_addr(&remoteaddr, 10, 10, 0, 2);
    esp_ip4_addr_t dnsaddr1;
    esp_netif_set_ip4_addr(&dnsaddr1, 10, 10, 0, 1);
    esp_ip4_addr_t dnsaddr2;
    esp_netif_set_ip4_addr(&dnsaddr2, 0, 0, 0, 0);

    ESP_ERROR_CHECK(esp_netif_ppp_start_server(esp_netif, localaddr, remoteaddr, dnsaddr1,
                        dnsaddr2, "", "", 0));

    while (1) {
        uart_event_t event;

        if (xQueueReceive(uart_event_queue, &event, pdMS_TO_TICKS(100))) {
            //ESP_LOGD(TAG, "event: %d", event.type);
            switch (event.type) {
            case UART_DATA: {
                    char buffer[512];
                    size_t length = 0;
                    uart_get_buffered_data_len(UART_NUM_1, &length);
                    //ESP_LOGV(TAG, "uart_get_buffered_data_len()=%d", length);
                    length = MIN(sizeof(buffer), length);
                    size_t read_length = uart_read_bytes(UART_NUM_1, buffer, length, portMAX_DELAY);
                    /* pass the input data to configured callback */
                    if (read_length) {
                        esp_err_t res = esp_netif_receive(esp_netif, buffer, read_length, NULL);
                        if (res != ESP_OK) {
                            ESP_LOGE(TAG, "esp_netif_receive error %d", res);
                        } else {
                            //ESP_LOGD(TAG, "ppp received: %d", read_length);
                        }
                    }
                break;
                }
            case UART_FIFO_OVF:
                ESP_LOGW(TAG, "HW FIFO Overflow");
                uart_flush_input(UART_NUM_1);
                xQueueReset(uart_event_queue);
                break;
            case UART_BUFFER_FULL:
                ESP_LOGW(TAG, "Ring Buffer Full");
                uart_flush_input(UART_NUM_1);
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
            ESP_LOGI(TAG, "Connection is dead, starting ppp server");
            ESP_ERROR_CHECK(esp_netif_start(esp_netif));
        }

    }

}

void ppp_server_init() {

    /* Config UART */
    const uart_config_t uart_config = {
        .baud_rate = CONFIG_EXAMPLE_MODEM_PPP_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        .source_clk = UART_SCLK_REF_TICK,
#else
        .source_clk = UART_SCLK_XTAL,
#endif
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_1, &uart_config));

    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_1, CONFIG_EXAMPLE_MODEM_UART_TX_PIN, CONFIG_EXAMPLE_MODEM_UART_RX_PIN,
                           CONFIG_EXAMPLE_MODEM_UART_RTS_PIN, CONFIG_EXAMPLE_MODEM_UART_CTS_PIN));

    ESP_ERROR_CHECK(uart_set_hw_flow_ctrl(UART_NUM_1, UART_HW_FLOWCTRL_CTS_RTS, UART_FIFO_LEN - 8));

    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_1, CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE, CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE,
                              CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE, &uart_event_queue, 0));

    ESP_ERROR_CHECK(uart_set_rx_timeout(UART_NUM_1, 1));

    ESP_ERROR_CHECK(uart_set_rx_full_threshold(UART_NUM_1, 64));


    ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID,
                                             &on_ppp_changed, NULL));


    // Create task to receive the uart data on.
    BaseType_t ret = xTaskCreate(ppp_task_thread, "ppp_task", 1024 * 3, NULL, 100, NULL);
    assert(ret == pdTRUE);
}