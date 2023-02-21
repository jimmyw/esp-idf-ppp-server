# PPP SERVER example

Instructions

Check out this repo in two locations, one client and one server.

On both sides specify:

* EXAMPLE_MODEM_DEVICE_NULLMODEM
   Will directly connect ppp without any AT crap

* EXAMPLE_MODEM_PPP_BAUDRATE
   to your desired baudrate

*EXAMPLE_MODEM_PPP_FLOW
    0: none, 1: software, 2: hardware


* EXAMPLE_MODEM_UART_TX_PIN
   Default 16
* EXAMPLE_MODEM_UART_RX_PIN
   Default 17
* EXAMPLE_MODEM_UART_RTS_PIN
   Default 27
* EXAMPLE_MODEM_UART_CTS_PIN
   Default 23

* CONFIG_EXAMPLE_LCP_ECHO
   To detect if link goes down

On the server side:
* Apply esp-idf-ppp_server.patch
* Enable CONFIG_LWIP_PPP_SERVER_SUPPORT

Connect both chips togheter, remember to cross TX/TX and RTS/CTX

On boot, client will automatlicly connect to server

Run ping or iptraf to test the link.
