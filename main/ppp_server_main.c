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
#include "esp_netif.h"
#include "esp_netif_ppp.h"
#include "netif/ppp/ppp.h"

#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/inet.h"
#include "lwip/netdb.h"
#include "lwip/sockets.h"
#include "mqtt_client.h"
#include <string.h>
#include "ppp_link.h"

static const char *TAG = "ppp_server_main";

#define MOUNT_PATH "/data"
#define HISTORY_PATH MOUNT_PATH "/history.txt"

static void initialize_filesystem(void)
{
    static wl_handle_t wl_handle;
    const esp_vfs_fat_mount_config_t mount_config = {
        .max_files = 4,
        .format_if_mount_failed = true
    };
    esp_err_t err = esp_vfs_fat_spiflash_mount_rw_wl(MOUNT_PATH, "storage", &mount_config, &wl_handle);
    if (err != ESP_OK) {
        return;
    }
}

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
  case NETIF_PPP_PHASE_DEAD:
      ESP_LOGD(TAG, "Phase Dead");
      break;
  case NETIF_PPP_PHASE_INITIALIZE:
      ESP_LOGD(TAG, "Phase Start");
      break;
  case NETIF_PPP_PHASE_ESTABLISH:
      ESP_LOGD(TAG, "Phase Establish");
      break;
  case NETIF_PPP_PHASE_AUTHENTICATE:
      ESP_LOGD(TAG, "Phase Authenticate");
      break;
  case NETIF_PPP_PHASE_NETWORK:
      ESP_LOGD(TAG, "Phase Network");
      break;
  case NETIF_PPP_PHASE_RUNNING:
      ESP_LOGD(TAG, "Phase Running");
      break;
  case NETIF_PPP_PHASE_TERMINATE:
      ESP_LOGD(TAG, "Phase Terminate");
      break;
  case NETIF_PPP_PHASE_DISCONNECT:
      ESP_LOGD(TAG, "Phase Disconnect");
      break;
  default:
      ESP_LOGW(TAG, "Unknown PPP event %d", event_id);
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


static int cmd_ppp_server(int argc, char **argv)
{
  const ppp_link_config_t ppp_link_config = {
    .type = PPP_LINK_SERVER,
    .uart = UART_NUM_1,
    .uart_config = {
        .baud_rate = CONFIG_EXAMPLE_MODEM_PPP_BAUDRATE,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
#if CONFIG_IDF_TARGET_ESP32 || CONFIG_IDF_TARGET_ESP32S2
        .source_clk = UART_SCLK_REF_TICK,
#else
        .source_clk = UART_SCLK_XTAL,
#endif
        .flow_ctrl = UART_HW_FLOWCTRL_CTS_RTS,
        .rx_flow_ctrl_thresh = UART_FIFO_LEN - 8,
    },
    .io = {
      .tx = CONFIG_EXAMPLE_MODEM_UART_TX_PIN,
      .rx = CONFIG_EXAMPLE_MODEM_UART_RX_PIN,
      .rts = CONFIG_EXAMPLE_MODEM_UART_RTS_PIN,
      .cts = CONFIG_EXAMPLE_MODEM_UART_CTS_PIN
    },
    .buffer = {
      .rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE,
      .tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE,
      .rx_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE
    },
    .ppp_server = {
      .localaddr.addr = esp_netif_htonl(esp_netif_ip4_makeu32(10, 10, 0, 1)),
      .remoteaddr.addr = esp_netif_htonl(esp_netif_ip4_makeu32(10, 10, 0, 2)),
      .dnsaddr1.addr = esp_netif_htonl(esp_netif_ip4_makeu32(10, 10, 0, 1)),
    }
  };

  ESP_LOGI(TAG, "Will configure as PPP SERVER");
  ppp_link_init(&ppp_link_config);

  return 0;
}


static int cmd_ppp_client(int argc, char **argv)
{
  const ppp_link_config_t ppp_link_config = {
    .type = PPP_LINK_CLIENT,
    .uart = UART_NUM_1,
    .uart_config = {
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
    },
    .io = {
      .tx = CONFIG_EXAMPLE_MODEM_UART_TX_PIN,
      .rx = CONFIG_EXAMPLE_MODEM_UART_RX_PIN,
      .rts = CONFIG_EXAMPLE_MODEM_UART_RTS_PIN,
      .cts = CONFIG_EXAMPLE_MODEM_UART_CTS_PIN
    },
    .buffer = {
      .rx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_RX_BUFFER_SIZE,
      .tx_buffer_size = CONFIG_EXAMPLE_MODEM_UART_TX_BUFFER_SIZE,
      .rx_queue_size = CONFIG_EXAMPLE_MODEM_UART_EVENT_QUEUE_SIZE
    },
  };

  ESP_LOGI(TAG, "Will configure as PPP CLIENT");
  ppp_link_init(&ppp_link_config);
  return 0;
}

void app_main(void) {

  esp_console_repl_t *repl = NULL;
  esp_console_repl_config_t repl_config = ESP_CONSOLE_REPL_CONFIG_DEFAULT();
  esp_console_dev_uart_config_t uart_config = ESP_CONSOLE_DEV_UART_CONFIG_DEFAULT();
  initialize_filesystem();
  repl_config.history_save_path = HISTORY_PATH;
  repl_config.prompt = "ppp_server>";

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
  esp_log_level_set("ppp_link", ESP_LOG_VERBOSE);
  esp_log_level_set("ppp_server_main", ESP_LOG_VERBOSE);
  esp_log_level_set("esp-netif_lwip-ppp", ESP_LOG_VERBOSE);
  esp_log_level_set("*", ESP_LOG_INFO);



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
