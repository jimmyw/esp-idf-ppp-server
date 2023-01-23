/* PPPoS Client Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "esp_log.h"
#include "esp_modem.h"
#include "esp_modem_netif.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"
#include "nullmodem.h"
#include "ping/ping_sock.h"
#include <string.h>

#define BROKER_URL "mqtt://mqtt.eclipseprojects.io"

static const char *TAG = "pppos_example";
static EventGroupHandle_t event_group = NULL;
static const int CONNECT_BIT = BIT0;
static const int STOP_BIT = BIT1;

static void modem_event_handler(void *event_handler_arg,
                                esp_event_base_t event_base, int32_t event_id,
                                void *event_data) {
  switch (event_id) {
  case ESP_MODEM_EVENT_PPP_START:
    ESP_LOGI(TAG, "Modem PPP Started");
    break;
  case ESP_MODEM_EVENT_PPP_STOP:
    ESP_LOGI(TAG, "Modem PPP Stopped");
    xEventGroupSetBits(event_group, STOP_BIT);
    break;
  case ESP_MODEM_EVENT_UNKNOWN:
    ESP_LOGW(TAG, "Unknown line received: %s", (char *)event_data);
    break;
  default:
    break;
  }
}

static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data) {
  ESP_LOGI(TAG, "PPP state changed event %d", event_id);
  if (event_id == NETIF_PPP_ERRORUSER) {
    /* User interrupted event from esp-netif */
    esp_netif_t *netif = *(esp_netif_t **)event_data;
    ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
  }
}

static void on_ip_event(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
  ESP_LOGD(TAG, "IP event! %d", event_id);
  if (event_id == IP_EVENT_PPP_GOT_IP) {
    esp_netif_dns_info_t dns_info;

    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    esp_netif_t *netif = event->esp_netif;

    ESP_LOGI(TAG, "Modem Connect to PPP Server");
    ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
    ESP_LOGI(TAG, "IP          : " IPSTR, IP2STR(&event->ip_info.ip));
    ESP_LOGI(TAG, "Netmask     : " IPSTR, IP2STR(&event->ip_info.netmask));
    ESP_LOGI(TAG, "Gateway     : " IPSTR, IP2STR(&event->ip_info.gw));
    esp_netif_get_dns_info(netif, 0, &dns_info);
    ESP_LOGI(TAG, "Name Server1: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    esp_netif_get_dns_info(netif, 1, &dns_info);
    ESP_LOGI(TAG, "Name Server2: " IPSTR, IP2STR(&dns_info.ip.u_addr.ip4));
    ESP_LOGI(TAG, "~~~~~~~~~~~~~~");
    xEventGroupSetBits(event_group, CONNECT_BIT);

    ESP_LOGI(TAG, "GOT ip event!!!");
  } else if (event_id == IP_EVENT_PPP_LOST_IP) {
    ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
  } else if (event_id == IP_EVENT_GOT_IP6) {
    ESP_LOGI(TAG, "GOT IPv6 event!");

    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
  }
}

static void test_on_ping_success(esp_ping_handle_t hdl, void *args) {
  // optionally, get callback arguments
  // const char* str = (const char*) args;
  // printf("%s\r\n", str); // "foo"
  uint8_t ttl;
  uint16_t seqno;
  uint32_t elapsed_time, recv_len;
  ip_addr_t target_addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TTL, &ttl, sizeof(ttl));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                       sizeof(target_addr));
  esp_ping_get_profile(hdl, ESP_PING_PROF_SIZE, &recv_len, sizeof(recv_len));
  esp_ping_get_profile(hdl, ESP_PING_PROF_TIMEGAP, &elapsed_time,
                       sizeof(elapsed_time));
  printf("%d bytes from %s icmp_seq=%d ttl=%d time=%d ms\n", recv_len,
         inet_ntoa(target_addr.u_addr.ip4), seqno, ttl, elapsed_time);
}

static void test_on_ping_timeout(esp_ping_handle_t hdl, void *args) {
  uint16_t seqno;
  ip_addr_t target_addr;
  esp_ping_get_profile(hdl, ESP_PING_PROF_SEQNO, &seqno, sizeof(seqno));
  esp_ping_get_profile(hdl, ESP_PING_PROF_IPADDR, &target_addr,
                       sizeof(target_addr));
  printf("From %s icmp_seq=%d timeout\n", inet_ntoa(target_addr.u_addr.ip4),
         seqno);
}

static void test_on_ping_end(esp_ping_handle_t hdl, void *args) {
  uint32_t transmitted;
  uint32_t received;
  uint32_t total_time_ms;

  esp_ping_get_profile(hdl, ESP_PING_PROF_REQUEST, &transmitted,
                       sizeof(transmitted));
  esp_ping_get_profile(hdl, ESP_PING_PROF_REPLY, &received, sizeof(received));
  esp_ping_get_profile(hdl, ESP_PING_PROF_DURATION, &total_time_ms,
                       sizeof(total_time_ms));
  printf("%d packets transmitted, %d received, time %dms\n", transmitted,
         received, total_time_ms);
}

void app_main(void) {
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                             &on_ip_event, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID,
                                             &on_ppp_changed, NULL));

  event_group = xEventGroupCreate();
  esp_log_level_set("esp_netif_lwip", ESP_LOG_VERBOSE);
  esp_log_level_set("*", ESP_LOG_VERBOSE);

  /* create dte object */
  esp_modem_dte_config_t config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  /* setup UART specific configuration based on kconfig options */
  config.tx_io_num = 16;
  config.rx_io_num = 17;
  config.flow_control = UART_HW_FLOWCTRL_DISABLE;
  config.baud_rate = 115200;
  // config.rts_io_num = 0;
  // config.cts_io_num = 0;
  config.rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;
  config.tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE;
  config.event_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE;
  config.event_task_stack_size =
      CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
  config.event_task_priority = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_PRIORITY;
  config.dte_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE / 2;

  modem_dte_t *dte = esp_modem_dte_init(&config);

  /* Register event handler */
  ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, modem_event_handler,
                                              ESP_EVENT_ANY_ID, NULL));

  // Init netif object
  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
  esp_netif_t *esp_netif = esp_netif_new(&cfg);
  assert(esp_netif);

  // ESP_ERROR_CHECK(esp_netif_dhcpc_stop(esp_netif));

  /*
  esp_netif_ip_info_t ip;
  memset(&ip, 0, sizeof(esp_netif_ip_info_t));
  ip.ip.addr = ipaddr_addr("10.10.10.1");
  ip.netmask.addr = ipaddr_addr("255.255.255.0");
    ip.gw.addr = ipaddr_addr("10.10.10.2");
  ESP_ERROR_CHECK(esp_netif_set_ip_info(esp_netif, &ip));*/

  void *modem_netif_adapter = esp_modem_netif_setup(dte);
  esp_modem_netif_set_default_handlers(modem_netif_adapter, esp_netif);

  nullmodem_init(dte);

#if CONFIG_LWIP_PPP_SERVER_SUPPORT
  esp_ip4_addr_t localaddr;
  esp_netif_set_ip4_addr(&localaddr, 10, 10, 0, 1);
  esp_ip4_addr_t remoteaddr;
  esp_netif_set_ip4_addr(&remoteaddr, 10, 10, 0, 2);
  esp_ip4_addr_t dnsaddr1;
  esp_netif_set_ip4_addr(&dnsaddr1, 10, 10, 0, 1);
  esp_ip4_addr_t dnsaddr2;
  esp_netif_set_ip4_addr(&dnsaddr2, 0, 0, 0, 0);

  esp_netif_ppp_start_server(esp_netif, localaddr, remoteaddr, dnsaddr1,
                             dnsaddr2, "", "", 0);
#else
  esp_netif_ppp_set_auth(esp_netif, NETIF_PPP_AUTHTYPE_NONE, NULL, NULL);
#endif
  /* attach the modem to the network interface */
  esp_netif_attach(esp_netif, modem_netif_adapter);

  /* Wait for IP address */
  xEventGroupWaitBits(event_group, CONNECT_BIT, pdTRUE, pdTRUE, portMAX_DELAY);
  ESP_LOGI(TAG, "Now connected, starting infinite ping session");

  esp_ping_config_t ping_config = ESP_PING_DEFAULT_CONFIG();
#if CONFIG_LWIP_PPP_SERVER_SUPPORT
  IP_ADDR4(&ping_config.target_addr, 10, 10, 0, 2);
#else
  IP_ADDR4(&ping_config.target_addr, 10, 10, 0, 1);
#endif
  ping_config.count = ESP_PING_COUNT_INFINITE; // ping in infinite mode,
                                               // esp_ping_stop can stop it

  /* set callback functions */
  esp_ping_callbacks_t cbs;
  cbs.on_ping_success = test_on_ping_success;
  cbs.on_ping_timeout = test_on_ping_timeout;
  cbs.on_ping_end = test_on_ping_end;

  esp_ping_handle_t ping;
  esp_ping_new_session(&ping_config, &cbs, &ping);
  esp_ping_start(ping);

  while (true) {
    vTaskDelay(1000);
  }
}
