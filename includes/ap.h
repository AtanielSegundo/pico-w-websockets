#ifndef TAREFA_CORE1_H
#define TAREFA1_CORE1_H

#include <string.h>
#include "pico/cyw43_arch.h"
#include "pico/stdlib.h"
#include "lwip/tcp.h"
#include "lwip/netif.h"
#include "pico/multicore.h"
#include "lwip/apps/mdns.h"
#include "lwip/ip4_addr.h"
#include "lwip/dhcp.h"
#include "cyw43.h"
#include "pico/cyw43_arch.h"
#include "pico/cyw43_driver.h"  

#define FLOAT_CHOP_FACTOR 100
#define SERVER_POLL_DELAY_MS 10

extern char dnss_captive_site[64];

int setup_access_point(const char *ssid, const char *password, const char *site_name);
int setup_connect_to_wifi(const char *ssid, const char *password, const char *site_name, uint32_t auth_mode);

#endif