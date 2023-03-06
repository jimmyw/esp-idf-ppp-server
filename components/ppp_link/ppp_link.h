#ifndef __PPP_LINK_H_
#define __PPP_LINK_H_

#include "driver/uart.h"
#include "esp_netif.h"

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

typedef struct ppp_link_config_s ppp_link_config_t;

esp_err_t ppp_link_init(const ppp_link_config_t *ppp_link_config);

#endif /* __PPP_LINK_H_ */
