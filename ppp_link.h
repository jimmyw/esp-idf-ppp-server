#ifndef __PPP_LINK_H_
#define __PPP_LINK_H_

#include "esp_netif.h"

#include "driver/uart.h"

#include "hal/gpio_types.h"

struct ppp_link_config_s {
    enum {
        PPP_LINK_CLIENT,
#ifdef CONFIG_PPP_SERVER_SUPPORT
        PPP_LINK_SERVER,
#endif
    } type;
    uart_port_t uart;
    uart_config_t uart_config;
    struct {
        gpio_num_t tx;
        gpio_num_t rx;
        gpio_num_t rts;
        gpio_num_t cts;
    } io;
    struct {
        int rx_buffer_size;
        int tx_buffer_size;
        int rx_queue_size;
    } buffer;
    struct {
        int stack_size;
        int prio;
    } task;
#ifdef CONFIG_PPP_SERVER_SUPPORT
    struct {
        esp_ip4_addr_t localaddr;
        esp_ip4_addr_t remoteaddr;
        esp_ip4_addr_t dnsaddr1;
        esp_ip4_addr_t dnsaddr2;
        const char *login;
        const char *password;
        int auth_req;
    } ppp_server;
#endif
};

// clang-format off
#define PPP_LINK_CFG_DEFAULT() {                    \
    .type = PPP_LINK_CLIENT,                        \
    .uart = UART_NUM_NC,                            \
    .uart_config = {                                \
        .baud_rate = 115200,                        \
        .data_bits = UART_DATA_8_BITS,              \
        .parity = UART_PARITY_DISABLE,              \
        .stop_bits = UART_STOP_BITS_1,              \
        .source_clk = UART_CLK,                     \
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,      \
        .rx_flow_ctrl_thresh = UART_FIFO_LEN - 8,   \
    },                                              \
    .io = {                                         \
        .tx = GPIO_NUM_NC,                          \
        .rx = GPIO_NUM_NC,                          \
        .rts = UART_PIN_NO_CHANGE,                  \
        .cts = UART_PIN_NO_CHANGE,                  \
    },                                              \
    .buffer = {                                     \
        .rx_buffer_size = 2048,                     \
        .tx_buffer_size = 2048,                     \
        .rx_queue_size = 30,                        \
    },                                              \
    .task = {                                       \
        .stack_size = (3 * 1024),                   \
        .prio = 100,                                \
    } \
};
// clang-format on

typedef struct ppp_link_config_s ppp_link_config_t;

esp_err_t ppp_link_init(const ppp_link_config_t *ppp_link_config);

#endif /* __PPP_LINK_H_ */
