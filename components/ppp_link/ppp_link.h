

#include "driver/uart.h"

struct ppp_link_config_s {
    enum {
        PPP_LINK_CLIENT,
        PPP_LINK_SERVER,
    } type;
    uart_port_t uart;
    uart_config_t uart_config;
    struct {
        int tx;
        int rx;
        int rts;
        int cts;
    } io;
    struct {
        int rx_buffer_size;
        int tx_buffer_size;
        int rx_queue_size;
    } buffer;
    struct {
        esp_ip4_addr_t localaddr;
        esp_ip4_addr_t remoteaddr;
        esp_ip4_addr_t dnsaddr1;
        esp_ip4_addr_t dnsaddr2;
        const char *login;
        const char *password;
        int auth_req;
    } ppp_server;
};

typedef struct ppp_link_config_s ppp_link_config_t;

esp_err_t ppp_link_init(const ppp_link_config_t *ppp_link_config);