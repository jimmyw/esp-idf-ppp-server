/* PPP_SERVER Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include "cmd_iperf.h"
#include "cmd_ping.h"
#include "cmd_system.h"
#include "esp_console.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_modem_api.h"
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "freertos/FreeRTOS.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
//#include "nullmodem.h"
#include <string.h>
#include "sdkconfig.h"

static const char *TAG = "ppp_server";



#if defined(CONFIG_EXAMPLE_FLOW_CONTROL_NONE)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_NONE
#elif defined(CONFIG_EXAMPLE_FLOW_CONTROL_SW)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_SW
#elif defined(CONFIG_EXAMPLE_FLOW_CONTROL_HW)
#define EXAMPLE_FLOW_CONTROL ESP_MODEM_FLOW_CONTROL_HW
#endif


static void on_ppp_changed(void *arg, esp_event_base_t event_base,
                           int32_t event_id, void *event_data) {

  esp_netif_t *netif = *(esp_netif_t **)event_data;
  switch (event_id) {
  case NETIF_PPP_ERRORNONE:
    ESP_LOGI(TAG, "No error. from netif: %p", netif);
    break;
  case NETIF_PPP_ERRORPARAM:
    ESP_LOGI(TAG, "Invalid parameter. from netif: %p", netif);
    break;
  case NETIF_PPP_ERROROPEN:
    ESP_LOGI(TAG, "Unable to open PPP session. from netif: %p", netif);
    break;
  case NETIF_PPP_ERRORDEVICE:
    ESP_LOGI(TAG, "Invalid I/O device for PPP. from netif: %p", netif);
    break;
  case NETIF_PPP_ERRORALLOC:
    ESP_LOGI(TAG, "Unable to allocate resources. from netif: %p", netif);
    break;
  case NETIF_PPP_ERRORUSER:
    ESP_LOGI(TAG, "User interrupted event from netif:%p", netif);
    break;
  case NETIF_PPP_ERRORCONNECT:
    ESP_LOGI(TAG, "Connection lost. netif:%p", netif);
    break;
  case NETIF_PPP_ERRORAUTHFAIL:
    ESP_LOGI(TAG, "Failed authentication challenge. netif:%p", netif);
    break;
  case NETIF_PPP_ERRORPROTOCOL:
    ESP_LOGI(TAG, "Failed to meet protocol. netif:%p", netif);
    break;
  case NETIF_PPP_ERRORPEERDEAD:
    ESP_LOGI(TAG, "Connection timeout netif:%p", netif);
    break;
  case NETIF_PPP_ERRORIDLETIMEOUT:
    ESP_LOGI(TAG, "Idle Timeout netif:%p", netif);
    break;
  case NETIF_PPP_ERRORCONNECTTIME:
    ESP_LOGI(TAG, "Max connect time reached netif:%p", netif);
    break;
  case NETIF_PPP_ERRORLOOPBACK:
    ESP_LOGI(TAG, "Loopback detected netif:%p", netif);
    break;
  default:
    ESP_LOGI(TAG, "PPP state changed event %d", event_id);
    break;
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

    ESP_LOGI(TAG, "GOT ip event!!!");
  } else if (event_id == IP_EVENT_PPP_LOST_IP) {
    ESP_LOGI(TAG, "Modem Disconnect from PPP Server");
  } else if (event_id == IP_EVENT_GOT_IP6) {
    ESP_LOGI(TAG, "GOT IPv6 event!");

    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    ESP_LOGI(TAG, "Got IPv6 address " IPV6STR, IPV62STR(event->ip6_info.ip));
  }
}

esp_modem_dte_config_t get_config() {
  /* create dte object */
  esp_modem_dte_config_t dte_config = ESP_MODEM_DTE_DEFAULT_CONFIG();
  dte_config.uart_config.tx_io_num = CONFIG_EXAMPLE_MODEM_UART_TX_PIN;
  dte_config.uart_config.rx_io_num = CONFIG_EXAMPLE_MODEM_UART_RX_PIN;
  dte_config.uart_config.rts_io_num = CONFIG_EXAMPLE_MODEM_UART_RTS_PIN;
  dte_config.uart_config.cts_io_num = CONFIG_EXAMPLE_MODEM_UART_CTS_PIN;
  dte_config.uart_config.flow_control = EXAMPLE_FLOW_CONTROL;
  dte_config.uart_config.baud_rate = CONFIG_EXAMPLE_MODEM_PPP_BAUDRATE;
  dte_config.uart_config.rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE;
  dte_config.uart_config.tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE;
  dte_config.uart_config.event_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE;
  dte_config.task_stack_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_STACK_SIZE;
  dte_config.task_priority = CONFIG_EXAMPLE_MODEM_UART_EVENT_TASK_PRIORITY;
  dte_config.dte_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE / 2;
  return dte_config;
}

esp_modem_dce_t *get_dce() {
  static esp_modem_dce_t *dce = NULL;
  static esp_netif_t *esp_netif = NULL;

  if (dce) {
    esp_modem_destroy(dce);
    dce = NULL;
  }

  if (esp_netif) {
    esp_netif_destroy(esp_netif);
    esp_netif = NULL;
  }

  esp_modem_dte_config_t dte_config = get_config();

  /* Configure the PPP netif */
  esp_modem_dce_config_t dce_config = ESP_MODEM_DCE_DEFAULT_CONFIG(CONFIG_EXAMPLE_MODEM_PPP_APN);
  esp_netif_config_t netif_ppp_config = ESP_NETIF_DEFAULT_PPP();
  esp_netif = esp_netif_new(&netif_ppp_config);
  assert(esp_netif);

  dce = esp_modem_new_dev(ESP_MODEM_DCE_NULLMODEM, &dte_config, &dce_config, esp_netif);
  assert(dce);
  return dce;
}

static int cmd_ppp_server(int argc, char **argv)
{
#if  0
  esp_modem_dte_config_t config = get_config();

  modem_dte_t *dte = esp_modem_dte_init(&config);

  /* Register event handler */
  ESP_ERROR_CHECK(esp_modem_set_event_handler(dte, modem_event_handler,
                                              ESP_EVENT_ANY_ID, NULL));

  // Init netif object
  esp_netif_config_t cfg = ESP_NETIF_DEFAULT_PPP();
  esp_netif_t *esp_netif = esp_netif_new(&cfg);
  assert(esp_netif);

  /* Initialize a nullmodem connection using a serial cable */
  void *modem_netif_adapter = esp_modem_netif_setup(dte);
  esp_modem_netif_set_default_handlers(modem_netif_adapter, esp_netif);
  nullmodem_init(dte);

  /* Configure ppp endpoint as server */
  ESP_LOGI(TAG, "Will configure as PPP SERVER");
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

  /* attach the modem to the network interface */
  esp_netif_attach(esp_netif, modem_netif_adapter);
#endif
  return 0;
}


static int cmd_ppp_client(int argc, char **argv)
{
  esp_modem_dce_t *dce = get_dce();

  /* Configure ppp endpoint as client */
  ESP_LOGI(TAG, "Will configure as PPP CLIENT");

  esp_err_t err = esp_modem_set_mode(dce, ESP_MODEM_MODE_DATA);
  if (err != ESP_OK) {
      ESP_LOGE(TAG, "esp_modem_set_mode(ESP_MODEM_MODE_DATA) failed with %d", err);
      return 1;
  }
  return 0;
}

void app_main(void) {

  esp_console_repl_t *repl = NULL;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
#if CONFIG_EXAMPLE_STORE_HISTORY
  initialize_filesystem();
  repl_config.history_save_path = HISTORY_PATH;
#endif
  repl_config.prompt = "iperf>";

  // init console REPL environment
  ESP_ERROR_CHECK(esp_console_new_repl_uart(&uart_config, &repl_config, &repl));

  /* Register commands */
  register_system_common();
  register_iperf();
  register_ping();

  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, ESP_EVENT_ANY_ID,
                                             &on_ip_event, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(NETIF_PPP_STATUS, ESP_EVENT_ANY_ID,
                                             &on_ppp_changed, NULL));

  esp_log_level_set("esp_netif_lwip", ESP_LOG_VERBOSE);
  esp_log_level_set("esp-modem", ESP_LOG_VERBOSE);
  esp_log_level_set("uart_terminal", ESP_LOG_VERBOSE);
  esp_log_level_set("modem_api", ESP_LOG_VERBOSE);
  esp_log_level_set("command_lib", ESP_LOG_VERBOSE);
  esp_log_level_set("uart_terminal", ESP_LOG_VERBOSE);
  esp_log_level_set("*", ESP_LOG_VERBOSE);


  const esp_console_cmd_t ppp_server = {
      .command = "ppp_server",
      .help = "Start ppp server",
      .hint = NULL,
      .func = &cmd_ppp_server,
  };
  ESP_ERROR_CHECK( esp_console_cmd_register(&ppp_server) );
  const esp_console_cmd_t ppp_client = {
      .command = "ppp_client",
      .help = "Start ppp client",
      .hint = NULL,
      .func = &cmd_ppp_client,
  };
  ESP_ERROR_CHECK( esp_console_cmd_register(&ppp_client) );

  /* Sleep forever */
  ESP_LOGI(TAG,
           " ============================================================");
  ESP_LOGI(TAG,
           " |       Steps to Test Bandwidth                             |");
  ESP_LOGI(TAG,
           " |                                                           |");
  ESP_LOGI(TAG,
           " |  1. Run ppp_server or ppp_client                          |");
  ESP_LOGI(TAG,
           " |  2. Wait ESP32 to get IP from DHCP                        |");
  ESP_LOGI(TAG,
           " |  3. ping -c 10 10.10.0.2                                  |");
  ESP_LOGI(TAG,
           " |  4. Server UDP: 'iperf -u -s 10.10.0.1 -i 3'              |");
  ESP_LOGI(TAG,
           " |  5. Client UDP: 'iperf -u -c 10.10.0.1 -t 60 -i 3'        |");
  ESP_LOGI(TAG,
           " |  6. Server TCP: 'iperf -s 10.10.0.1 -i 3'                 |");
  ESP_LOGI(TAG,
           " |  7. Client TCP: 'iperf -c 10.10.0.1 -t 60 -i 3'           |");
  ESP_LOGI(TAG,
           " |                                                           |");
  ESP_LOGI(TAG,
           " =============================================================");

  // start console REPL
  ESP_ERROR_CHECK(esp_console_start_repl(repl));
}
